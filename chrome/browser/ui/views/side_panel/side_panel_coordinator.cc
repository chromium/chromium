// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_combobox_model.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_toolbar_container.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/separator.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace {

const char kGlobalSidePanelRegistryKey[] = "global_side_panel_registry_key";

constexpr int kSidePanelContentViewId = 42;
constexpr int kSidePanelContentWrapperViewId = 43;

SidePanelEntry::Id GetDefaultEntry() {
  return base::FeatureList::IsEnabled(features::kPowerBookmarksSidePanel)
             ? SidePanelEntry::Id::kBookmarks
             : SidePanelEntry::Id::kReadingList;
}

std::unique_ptr<views::ImageButton> CreateControlButton(
    views::View* host,
    base::RepeatingClosure pressed_callback,
    const gfx::VectorIcon& icon,
    const std::u16string& tooltip_text,
    ui::ElementIdentifier view_id,
    int dip_size) {
  auto button = views::CreateVectorImageButtonWithNativeTheme(pressed_callback,
                                                              icon, dip_size);
  button->SetTooltipText(tooltip_text);
  button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  views::InstallCircleHighlightPathGenerator(button.get());

  int minimum_button_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_BUTTON_MINIMUM_SIZE);
  button->SetMinimumImageSize(
      gfx::Size(minimum_button_size, minimum_button_size));

  button->SetProperty(
      views::kMarginsKey,
      gfx::Insets().set_left(ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_RELATED_BUTTON_HORIZONTAL)));
  button->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification().WithAlignment(views::LayoutAlignment::kEnd));
  button->SetProperty(views::kElementIdentifierKey, view_id);

  return button;
}

using PopulateSidePanelCallback = base::OnceCallback<void(
    SidePanelEntry* entry,
    absl::optional<std::unique_ptr<views::View>> content_view)>;

// SidePanelContentSwappingContainer is used as the content wrapper for views
// hosted in the side panel. This uses the SidePanelContentProxy to check if or
// wait for a SidePanelEntry's content view to be ready to be shown then only
// swaps the views when the content is ready. If the SidePanelContextProxy
// doesn't exist, the content is swapped immediately.
class SidePanelContentSwappingContainer : public views::View {
 public:
  explicit SidePanelContentSwappingContainer(bool show_immediately_for_testing)
      : show_immediately_for_testing_(show_immediately_for_testing) {
    SetUseDefaultFillLayout(true);
    SetBackground(
        views::CreateThemedSolidBackground(kColorSidePanelBackground));
    SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kUnbounded));
    SetID(kSidePanelContentWrapperViewId);
  }

  ~SidePanelContentSwappingContainer() override {
    ResetLoadingEntryIfNecessary();
  }

  void RequestEntry(SidePanelEntry* entry, PopulateSidePanelCallback callback) {
    DCHECK(entry);
    ResetLoadingEntryIfNecessary();
    auto content_view = entry->GetContent();
    SidePanelContentProxy* content_proxy =
        SidePanelUtil::GetSidePanelContentProxy(content_view.get());
    if (content_proxy->IsAvailable() || show_immediately_for_testing_) {
      std::move(callback).Run(entry, std::move(content_view));
    } else {
      entry->CacheView(std::move(content_view));
      loading_entry_ = entry;
      loaded_callback_ = std::move(callback);
      content_proxy->SetAvailableCallback(
          base::BindOnce(&SidePanelContentSwappingContainer::RunLoadedCallback,
                         base::Unretained(this)));
    }
  }

  void ResetLoadingEntryIfNecessary() {
    if (loading_entry_ && loading_entry_->CachedView()) {
      // The available callback here is used for showing the entry once it has
      // loaded. We need to reset this to make sure it is not triggered to be
      // shown once available.
      SidePanelUtil::GetSidePanelContentProxy(loading_entry_->CachedView())
          ->ResetAvailableCallback();
    }
    loading_entry_ = nullptr;
  }

  SidePanelEntry* loading_entry() const { return loading_entry_; }

 private:
  void RunLoadedCallback() {
    DCHECK(!loaded_callback_.is_null());
    SidePanelEntry* entry = loading_entry_;
    loading_entry_ = nullptr;
    std::move(loaded_callback_).Run(entry, absl::nullopt);
  }

  // When true, don't delay switching panels.
  bool show_immediately_for_testing_;
  // If the SidePanelEntry is ever discarded by the SidePanelCoordinator then we
  // are always either immediately switching to a different entry (where this
  // value would be reset) or closing the side panel (where this would be
  // destroyed).
  raw_ptr<SidePanelEntry> loading_entry_ = nullptr;
  PopulateSidePanelCallback loaded_callback_;
};

}  // namespace

SidePanelCoordinator::SidePanelCoordinator(BrowserView* browser_view)
    : browser_view_(browser_view) {
  combobox_model_ = std::make_unique<SidePanelComboboxModel>();

  auto global_registry = std::make_unique<SidePanelRegistry>();
  global_registry_ = global_registry.get();
  registry_observations_.AddObservation(global_registry_);
  browser_view->browser()->SetUserData(kGlobalSidePanelRegistryKey,
                                       std::move(global_registry));

  browser_view_->browser()->tab_strip_model()->AddObserver(this);

  SidePanelUtil::PopulateGlobalEntries(browser_view->browser(),
                                       global_registry_);
}

SidePanelCoordinator::~SidePanelCoordinator() {
  browser_view_->browser()->tab_strip_model()->RemoveObserver(this);
  view_state_observers_.Clear();
}

// static
SidePanelRegistry* SidePanelCoordinator::GetGlobalSidePanelRegistry(
    Browser* browser) {
  return static_cast<SidePanelRegistry*>(
      browser->GetUserData(kGlobalSidePanelRegistryKey));
}

void SidePanelCoordinator::Show(
    absl::optional<SidePanelEntry::Id> entry_id,
    absl::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger) {
  if (entry_id.has_value()) {
    Show(SidePanelEntry::Key(entry_id.value()), open_trigger);
  } else {
    Show(GetLastActiveEntryKey().value_or(
             SidePanelEntry::Key(GetDefaultEntry())),
         open_trigger);
  }
}

void SidePanelCoordinator::Show(
    SidePanelEntry::Key entry_key,
    absl::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger) {
  Show(GetEntryForKey(entry_key), open_trigger);
}

void SidePanelCoordinator::AddSidePanelViewStateObserver(
    SidePanelViewStateObserver* observer) {
  view_state_observers_.AddObserver(observer);
}

void SidePanelCoordinator::RemoveSidePanelViewStateObserver(
    SidePanelViewStateObserver* observer) {
  view_state_observers_.RemoveObserver(observer);
}

void SidePanelCoordinator::SetSidePanelButtonTooltipText(
    std::u16string tooltip_text) {
  auto* toolbar = browser_view_->toolbar();
  // On Progressive web apps, the toolbar can be null when opening the side
  // panel. This check is added as a added safeguard.
  if (toolbar && toolbar->GetSidePanelButton()) {
    toolbar->GetSidePanelButton()->SetTooltipText(tooltip_text);
  }
}

void SidePanelCoordinator::Close() {
  if (!GetContentView())
    return;

  if (current_entry_) {
    // Reset current_entry_ first to prevent current_entry->OnEntryHidden() from
    // calling multiple times. This could happen in the edge cases when callback
    // inside current_entry->OnEntryHidden() is calling Close() to trigger race
    // condition.
    auto* current_entry = current_entry_.get();
    current_entry_.reset();
    current_entry->OnEntryHidden();
  }

  if (global_registry_->active_entry().has_value()) {
    last_active_global_entry_key_ =
        global_registry_->active_entry().value()->key();
  }
  // Reset active entry values for all observed registries and clear cache for
  // everything except remaining active entries (i.e. if another tab has an
  // active contextual entry).
  global_registry_->ResetActiveEntry();
  if (auto* contextual_registry = GetActiveContextualRegistry())
    contextual_registry->ResetActiveEntry();
  ClearCachedEntryViews();

  // TODO(pbos): Make this button observe panel-visibility state instead.
  SetSidePanelButtonTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_PANEL_SHOW));

  // `OnEntryWillDeregister` (triggered by calling `OnEntryHidden`) may already
  // have deleted the content view, so check that it still exists.
  if (views::View* content_view = GetContentView())
    browser_view_->unified_side_panel()->RemoveChildViewT(content_view);
  header_combobox_ = nullptr;
  SidePanelUtil::RecordSidePanelClosed(opened_timestamp_);

  for (SidePanelViewStateObserver& view_state_observer :
       view_state_observers_) {
    view_state_observer.OnSidePanelDidClose();
  }
}

void SidePanelCoordinator::Toggle() {
  auto* side_panel_container = browser_view_->toolbar()->side_panel_container();
  if (IsSidePanelShowing() &&
      (!side_panel_container ||
       !side_panel_container->IsActiveEntryPinnedAndVisible())) {
    Close();
  } else {
    absl::optional<SidePanelEntry::Id> entry_id = absl::nullopt;
    if (browser_view_->browser()->window()->IsFeaturePromoActive(
            feature_engagement::kIPHPowerBookmarksSidePanelFeature)) {
      entry_id = absl::make_optional(SidePanelEntry::Id::kBookmarks);
    } else if (side_panel_container &&
               side_panel_container->IsActiveEntryPinnedAndVisible()) {
      // Update entry_id here since otherwise the entry triggered in 'Show' will
      // be the CSC entry when instead this should toggle away from the CSC
      // entry and show the last active global entry or the default entry if
      // there is no last active global entry.
      entry_id = absl::make_optional(
          GetLastActiveGlobalEntryKey()
              .value_or(SidePanelEntry::Key(GetDefaultEntry()))
              .id());
    }
    Show(entry_id, SidePanelUtil::SidePanelOpenTrigger::kToolbarButton);
  }
}

void SidePanelCoordinator::OpenInNewTab() {
  if (!GetContentView() || !current_entry_)
    return;

  GURL new_tab_url = current_entry_->GetOpenInNewTabURL();
  if (!new_tab_url.is_valid())
    return;

  SidePanelUtil::RecordNewTabButtonClicked(current_entry_->key().id());
  content::OpenURLParams params(new_tab_url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                /*is_renderer_initiated=*/false);
  browser_view_->browser()->OpenURL(params);
  Close();
}

void SidePanelCoordinator::UpdatePinState() {
  PrefService* pref_service = browser_view_->GetProfile()->GetPrefs();
  if (pref_service) {
    bool current_state = pref_service->GetBoolean(
        prefs::kSidePanelCompanionEntryPinnedToToolbar);
    pref_service->SetBoolean(prefs::kSidePanelCompanionEntryPinnedToToolbar,
                             !current_state);
  }
}

absl::optional<SidePanelEntry::Id> SidePanelCoordinator::GetCurrentEntryId()
    const {
  return current_entry_
             ? absl::optional<SidePanelEntry::Id>(current_entry_->key().id())
             : absl::nullopt;
}

SidePanelEntry::Id SidePanelCoordinator::GetComboboxDisplayedEntryIdForTesting()
    const {
  return combobox_model_->GetKeyAt(header_combobox_->GetSelectedIndex().value())
      .id();
}

SidePanelEntry* SidePanelCoordinator::GetLoadingEntryForTesting() const {
  return GetLoadingEntry();
}

bool SidePanelCoordinator::IsSidePanelShowing() const {
  return GetContentView() != nullptr;
}

bool SidePanelCoordinator::IsSidePanelEntryShowing(
    const SidePanelEntry* entry) const {
  return IsSidePanelShowing() && current_entry_ &&
         current_entry_.get() == entry;
}

void SidePanelCoordinator::Show(
    SidePanelEntry* entry,
    absl::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger) {
  if (!entry) {
    return;
  }

  if (GetContentView() == nullptr) {
    InitializeSidePanel();
    opened_timestamp_ = base::TimeTicks::Now();
    SidePanelUtil::RecordSidePanelOpen(open_trigger);
    // Record usage for side panel promo.
    feature_engagement::TrackerFactory::GetForBrowserContext(
        browser_view_->GetProfile())
        ->NotifyEvent("side_panel_shown");

    // Close IPH for side panel if shown.
    browser_view_->browser()->window()->CloseFeaturePromo(
        feature_engagement::kIPHReadingListInSidePanelFeature);
    browser_view_->browser()->window()->CloseFeaturePromo(
        feature_engagement::kIPHPowerBookmarksSidePanelFeature);
  }

  SidePanelContentSwappingContainer* content_wrapper =
      static_cast<SidePanelContentSwappingContainer*>(
          GetContentView()->GetViewByID(kSidePanelContentWrapperViewId));
  DCHECK(content_wrapper);

  // If we are already loading this entry, do nothing.
  if (content_wrapper->loading_entry() == entry) {
    return;
  }

  // If we are already showing this entry, make sure we prevent any loading
  // entry from showing once the load has finished. Say if we are showing A then
  // trigger B to show but switch back to A while B is still loading (and not
  // yet shown) we want to make sure B will not then be shown when it has
  // finished loading. Note, this does not cancel the triggered load of B, B
  // remains cached.
  if (current_entry_.get() == entry) {
    if (content_wrapper->loading_entry()) {
      content_wrapper->ResetLoadingEntryIfNecessary();
    }
    return;
  }

  SidePanelUtil::RecordEntryShowTriggeredMetrics(entry->key().id(),
                                                 open_trigger);

  content_wrapper->RequestEntry(
      entry, base::BindOnce(&SidePanelCoordinator::PopulateSidePanel,
                            base::Unretained(this)));
}

views::View* SidePanelCoordinator::GetContentView() const {
  return browser_view_->unified_side_panel()->GetViewByID(
      kSidePanelContentViewId);
}

SidePanelEntry* SidePanelCoordinator::GetEntryForKey(
    const SidePanelEntry::Key& entry_key) {
  if (auto* contextual_entry = GetActiveContextualEntryForKey(entry_key)) {
    return contextual_entry;
  }

  return global_registry_->GetEntryForKey(entry_key);
}

SidePanelEntry* SidePanelCoordinator::GetActiveContextualEntryForKey(
    const SidePanelEntry::Key& entry_key) {
  return GetActiveContextualRegistry()
             ? GetActiveContextualRegistry()->GetEntryForKey(entry_key)
             : nullptr;
}

SidePanelEntry* SidePanelCoordinator::GetLoadingEntry() const {
  SidePanelContentSwappingContainer* content_wrapper =
      static_cast<SidePanelContentSwappingContainer*>(
          GetContentView()->GetViewByID(kSidePanelContentWrapperViewId));
  DCHECK(content_wrapper);
  return content_wrapper->loading_entry();
}

bool SidePanelCoordinator::IsGlobalEntryShowing(
    const SidePanelEntry::Key& entry_key) const {
  if (!GetContentView() || !current_entry_) {
    return false;
  }

  return global_registry_->GetEntryForKey(entry_key) == current_entry_.get();
}

void SidePanelCoordinator::InitializeSidePanel() {
  // TODO(pbos): Make this button observe panel-visibility state instead.
  SetSidePanelButtonTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_PANEL_HIDE));

  auto container = std::make_unique<views::FlexLayoutView>();
  // Align views vertically top to bottom.
  container->SetOrientation(views::LayoutOrientation::kVertical);
  container->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  // Stretch views to fill horizontal bounds.
  container->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  container->SetID(kSidePanelContentViewId);

  container->AddChildView(CreateHeader());
  container->AddChildView(std::make_unique<views::Separator>())
      ->SetColorId(kColorSidePanelContentAreaSeparator);

  auto content_wrapper = std::make_unique<SidePanelContentSwappingContainer>(
      no_delays_for_testing_);
  container->AddChildView(std::move(content_wrapper));
  // Set to not visible so that the side panel is not shown until content is
  // ready to be shown.
  container->SetVisible(false);

  browser_view_->unified_side_panel()->AddChildView(std::move(container));
}

void SidePanelCoordinator::PopulateSidePanel(
    SidePanelEntry* entry,
    absl::optional<std::unique_ptr<views::View>> content_view) {
  // Ensure that the correct combobox entry is selected. This may not be the
  // case if `Show()` was called after registering a contextual entry.
  DCHECK(header_combobox_);
  SetSelectedEntryInCombobox(entry->key());

  auto* content_wrapper =
      GetContentView()->GetViewByID(kSidePanelContentWrapperViewId);
  DCHECK(content_wrapper);
  // |content_wrapper| should have either no child views or one child view for
  // the currently hosted SidePanelEntry.
  DCHECK(content_wrapper->children().size() <= 1);

  content_wrapper->SetVisible(true);
  GetContentView()->SetVisible(true);
  if (current_entry_ && content_wrapper->children().size()) {
    auto current_entry_view =
        content_wrapper->RemoveChildViewT(content_wrapper->children().front());
    current_entry_->CacheView(std::move(current_entry_view));
  }
  auto* content = content_wrapper->AddChildView(
      content_view.has_value() ? std::move(content_view.value())
                               : entry->GetContent());
  if (auto* contextual_registry = GetActiveContextualRegistry())
    contextual_registry->ResetActiveEntry();
  auto* previous_entry = current_entry_.get();
  current_entry_ = entry->GetWeakPtr();
  entry->OnEntryShown();
  if (previous_entry) {
    previous_entry->OnEntryHidden();
  } else {
    content->RequestFocus();
  }
  header_open_in_new_tab_button_->SetVisible(
      current_entry_->SupportsNewTabButton());
  UpdateNewTabButtonState();
  header_pin_button_->SetVisible(current_entry_->key().id() ==
                                 SidePanelEntry::Id::kSearchCompanion);
  if (auto* side_panel_container =
          browser_view_->toolbar()->side_panel_container()) {
    side_panel_container->UpdateSidePanelContainerButtonsState();
  }
}

void SidePanelCoordinator::ClearCachedEntryViews() {
  global_registry_->ClearCachedEntryViews();
  TabStripModel* model = browser_view_->browser()->tab_strip_model();
  if (!model)
    return;
  for (int index = 0; index < model->count(); ++index) {
    auto* web_contents =
        browser_view_->browser()->tab_strip_model()->GetWebContentsAt(index);
    if (auto* registry = SidePanelRegistry::Get(web_contents))
      registry->ClearCachedEntryViews();
  }
}

absl::optional<SidePanelEntry::Key>
SidePanelCoordinator::GetLastActiveEntryKey() const {
  // If a contextual entry is active, return that. If not, return the last
  // active global entry. If neither exist, fall back to kReadingList.
  if (GetActiveContextualRegistry() &&
      GetActiveContextualRegistry()->active_entry().has_value()) {
    return GetActiveContextualRegistry()->active_entry().value()->key();
  }

  return GetLastActiveGlobalEntryKey();
}

absl::optional<SidePanelEntry::Key>
SidePanelCoordinator::GetLastActiveGlobalEntryKey() const {
  // Return the last active global entry. If neither exist, fall back to the
  // default entry.
  if (global_registry_->active_entry().has_value())
    return global_registry_->active_entry().value()->key();

  if (last_active_global_entry_key_.has_value())
    return last_active_global_entry_key_.value();

  return absl::nullopt;
}

absl::optional<SidePanelEntry::Key> SidePanelCoordinator::GetSelectedKey()
    const {
  if (!header_combobox_)
    return absl::nullopt;

  // If we are waiting on content swapping delays we want to return the id for
  // the entry we are attempting to swap to.
  if (const auto* entry = GetLoadingEntry()) {
    return entry->key();
  }

  // If we are not waiting on content swapping we want to return the active
  // selected entry id.
  return combobox_model_->GetKeyAt(
      header_combobox_->GetSelectedIndex().value());
}

SidePanelRegistry* SidePanelCoordinator::GetActiveContextualRegistry() const {
  if (auto* web_contents =
          browser_view_->browser()->tab_strip_model()->GetActiveWebContents()) {
    return SidePanelRegistry::Get(web_contents);
  }
  return nullptr;
}

std::unique_ptr<views::View> SidePanelCoordinator::CreateHeader() {
  auto header = std::make_unique<views::FlexLayoutView>();
  // ChromeLayoutProvider for providing margins.
  ChromeLayoutProvider* const chrome_layout_provider =
      ChromeLayoutProvider::Get();

  // Set the interior margins of the header on the left and right sides.
  header->SetInteriorMargin(gfx::Insets::VH(
      0, chrome_layout_provider->GetDistanceMetric(
             ChromeDistanceMetric::
                 DISTANCE_SIDE_PANEL_HEADER_INTERIOR_MARGIN_HORIZONTAL)));
  // Set alignments for horizontal (main) and vertical (cross) axes.
  header->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  header->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // The minimum cross axis size should the expected height of the header.
  constexpr int kDefaultSidePanelHeaderHeight = 40;
  header->SetMinimumCrossAxisSize(kDefaultSidePanelHeaderHeight);
  header->SetBackground(
      views::CreateThemedSolidBackground(ui::kColorWindowBackground));

  header_combobox_ = header->AddChildView(CreateCombobox());
  header_combobox_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  header_combobox_->SetProperty(views::kElementIdentifierKey,
                                kSidePanelComboboxElementId);

  // TODO(corising): Update icon and tooltip once provided by UX.
  header_pin_button_ = header->AddChildView(CreateControlButton(
      header.get(),
      base::BindRepeating(&SidePanelCoordinator::UpdatePinState,
                          base::Unretained(this)),
      views::kPinIcon,
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_UNPIN),
      kSidePanelPinButtonElementId,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE)));
  header_pin_button_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  // The icon is later set as visible for side panels that support it.
  header_pin_button_->SetVisible(false);

  header_open_in_new_tab_button_ = header->AddChildView(CreateControlButton(
      header.get(),
      base::BindRepeating(&SidePanelCoordinator::OpenInNewTab,
                          base::Unretained(this)),
      vector_icons::kOpenInNewIcon,
      l10n_util::GetStringUTF16(IDS_ACCNAME_OPEN_IN_NEW_TAB),
      kSidePanelOpenInNewTabButtonElementId,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE)));
  header_open_in_new_tab_button_->SetFocusBehavior(
      views::View::FocusBehavior::ALWAYS);
  // The icon is later set as visible for side panels that support it.
  header_open_in_new_tab_button_->SetVisible(false);

  auto* header_close_button = header->AddChildView(CreateControlButton(
      header.get(),
      base::BindRepeating(&SidePanelCoordinator::Close, base::Unretained(this)),
      views::kIcCloseIcon, l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE),
      kSidePanelCloseButtonElementId,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE)));
  header_close_button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  return header;
}

std::unique_ptr<views::Combobox> SidePanelCoordinator::CreateCombobox() {
  auto combobox = std::make_unique<views::Combobox>(combobox_model_.get());
  combobox->SetMenuSelectionAtCallback(
      base::BindRepeating(&SidePanelCoordinator::OnComboboxChangeTriggered,
                          base::Unretained(this)));
  combobox->SetSelectedIndex(
      combobox_model_->GetIndexForKey((GetLastActiveEntryKey().value_or(
          SidePanelEntry::Key(GetDefaultEntry())))));
  combobox->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_SIDE_PANEL_SELECTOR));
  combobox->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/false)
          .WithAlignment(views::LayoutAlignment::kStart));
  combobox->SetBorderColorId(ui::kColorSidePanelComboboxBorder);
  combobox->SetBackgroundColorId(ui::kColorSidePanelComboboxBackground);
  combobox->SetEventHighlighting(true);
  combobox->SetSizeToLargestLabel(false);
  return combobox;
}

bool SidePanelCoordinator::OnComboboxChangeTriggered(size_t index) {
  SidePanelEntry::Key entry_key = combobox_model_->GetKeyAt(index);
  Show(entry_key, SidePanelUtil::SidePanelOpenTrigger::kComboboxSelected);
  views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
      kSidePanelComboboxChangedCustomEventId, header_combobox_);
  return true;
}

void SidePanelCoordinator::SetSelectedEntryInCombobox(
    const SidePanelEntry::Key& entry_key) {
  header_combobox_->SetSelectedIndex(
      combobox_model_->GetIndexForKey(entry_key));
  header_combobox_->SchedulePaint();
}

bool SidePanelCoordinator::ShouldRemoveFromComboboxOnDeregister(
    SidePanelRegistry* deregistering_registry,
    const SidePanelEntry::Key& key) {
  // Remove the entry from the combobox if one of these conditions are met:
  //  - The entry will be deregistered from the global registry and there's no
  //    entry with the same key in the active contextual registry.
  //  - The entry will be deregistered from a contextual registry and there's
  //    no entry with the same key in the global registry.
  bool remove_if_global = deregistering_registry == global_registry_ &&
                          !GetActiveContextualEntryForKey(key);
  bool remove_if_contextual =
      deregistering_registry == GetActiveContextualRegistry() &&
      !global_registry_->GetEntryForKey(key);

  return remove_if_global || remove_if_contextual;
}

SidePanelEntry* SidePanelCoordinator::GetNewActiveEntryOnDeregister(
    SidePanelRegistry* deregistering_registry,
    const SidePanelEntry::Key& key) {
  // This function should only be called when the side panel view is shown.
  DCHECK(GetContentView());

  // Attempt to return an entry in the following fallback order: global entry
  // for `key` if a contextual entry is deregistered > active global entry >
  // null.
  if (deregistering_registry == GetActiveContextualRegistry() &&
      global_registry_->GetEntryForKey(key)) {
    return global_registry_->GetEntryForKey(key);
  }

  return global_registry_->active_entry().value_or(nullptr);
}

SidePanelEntry* SidePanelCoordinator::GetNewActiveEntryOnTabChanged() {
  // This function should only be called when the side panel view is shown.
  DCHECK(GetContentView());

  // Attempt to return an entry in the following fallback order:
  //  - the new tab's registry's active entry
  //  - if the active entry's key is registered in the global registry:
  //    - the new tab's registry's entry with the same key
  //    - the global registry's entry with the same key (note that
  //      GetEntryForKey will return this fallback order)
  //  - if there is an active entry in the global registry:
  //    - the new tab's registry's entry with the same key
  //    - the global registry's active entry (note that GetEntryForKey will
  //      return this fallback order)
  //  - no entry (this closes the side panel)
  // Note: GetActiveContextualRegistry() returns the registry for the new tab in
  // this function.
  // Note: If Show() is called with an entry returned by this function, then
  // that entry will be active in its owning registry.
  auto* active_contextual_registry = GetActiveContextualRegistry();
  if (active_contextual_registry &&
      active_contextual_registry->active_entry()) {
    return *active_contextual_registry->active_entry();
  }

  if (current_entry_ &&
      global_registry_->GetEntryForKey(current_entry_->key())) {
    return GetEntryForKey(current_entry_->key());
  }

  return global_registry_->active_entry()
             ? GetEntryForKey((*global_registry_->active_entry())->key())
             : nullptr;
}

void SidePanelCoordinator::OnEntryRegistered(SidePanelRegistry* registry,
                                             SidePanelEntry* entry) {
  combobox_model_->AddItem(entry);
  if (GetContentView()) {
    SetSelectedEntryInCombobox(GetLastActiveEntryKey().value_or(
        SidePanelEntry::Key(GetDefaultEntry())));
  }

  // If `entry` is a contextual entry and the global entry with the same key is
  // currently being shown, show the new `entry`.
  if (registry == GetActiveContextualRegistry() &&
      IsGlobalEntryShowing(entry->key())) {
    Show(entry, SidePanelUtil::SidePanelOpenTrigger::kExtensionEntryRegistered);
  }
}

void SidePanelCoordinator::OnEntryWillDeregister(SidePanelRegistry* registry,
                                                 SidePanelEntry* entry) {
  absl::optional<SidePanelEntry::Key> selected_key = GetSelectedKey();
  if (ShouldRemoveFromComboboxOnDeregister(registry, entry->key())) {
    combobox_model_->RemoveItem(entry->key());

    if (GetContentView()) {
      SetSelectedEntryInCombobox(GetLastActiveEntryKey().value_or(
          SidePanelEntry::Key(GetDefaultEntry())));
    }
  }

  // If the active global entry is the entry being deregistered, reset
  // last_active_global_entry_key_.
  if (registry == global_registry_ &&
      last_active_global_entry_key_.has_value() &&
      entry->key() == last_active_global_entry_key_.value()) {
    last_active_global_entry_key_ = absl::nullopt;
  }

  // Save the entry's view: if it has a cached view, retrieve it. Otherwise if
  // the entry is shown, get it from the side panel view. This is necessary so
  // the view can be preserved so it won't be destroyed by Close().
  std::unique_ptr<views::View> entry_view =
      entry->CachedView() ? entry->GetContent() : nullptr;

  // Update the current entry to make sure we don't show an entry that is being
  // removed or close the panel if the entry being deregistered is the only one
  // that has been visible.
  if (GetContentView() && selected_key.has_value() &&
      selected_key.value() == entry->key()) {
    // If a global entry is deregistered but a contextual entry with the same
    // key is shown, do nothing.
    if (registry == global_registry_ &&
        GetActiveContextualEntryForKey(entry->key())) {
      entry->CacheView(std::move(entry_view));
      return;
    }

    // Fetch the entry's view from the side panel container if it is shown.
    auto* content_wrapper =
        GetContentView()->GetViewByID(kSidePanelContentWrapperViewId);
    DCHECK(content_wrapper);
    if (content_wrapper->children().size() == 1) {
      entry_view = content_wrapper->RemoveChildViewT(
          content_wrapper->children().front());
      // TODO(crbug.com/1423211): Log the time elapsed between when this view is
      // removed, to when the new active entry's view is shown. This can
      // determine if the user will notice a flash in the side panel in between
      // different entries being shown.
    }

    if (auto* new_active_entry =
            GetNewActiveEntryOnDeregister(registry, entry->key())) {
      Show(new_active_entry,
           SidePanelUtil::SidePanelOpenTrigger::kSidePanelEntryDeregistered);
    } else {
      Close();
    }
  }

  // Cache the deregistering entry's view. This needs to be done after Close()
  // might be called because Close() clears all cached views.
  entry->CacheView(std::move(entry_view));
}

void SidePanelCoordinator::OnEntryIconUpdated(SidePanelEntry* entry) {
  combobox_model_->UpdateIconForEntry(entry);
}

void SidePanelCoordinator::OnRegistryDestroying(SidePanelRegistry* registry) {
  registry_observations_.RemoveObservation(registry);
}

void SidePanelCoordinator::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed()) {
    return;
  }
  // Handle removing the previous tab's contextual registry if one exists and
  // update the combobox.
  auto* old_contextual_registry =
      SidePanelRegistry::Get(selection.old_contents);
  if (old_contextual_registry) {
    registry_observations_.RemoveObservation(old_contextual_registry);
    std::vector<SidePanelEntry::Key> contextual_keys_to_remove;

    // Only remove the previous tab's contextual entries from the combobox if
    // they are not in the global registry.
    for (auto const& entry : old_contextual_registry->entries()) {
      if (!global_registry_->GetEntryForKey(entry->key())) {
        contextual_keys_to_remove.push_back(entry->key());
      }
    }

    combobox_model_->RemoveItems(contextual_keys_to_remove);
  }

  // Add the current tab's contextual registry and update the combobox.
  auto* new_contextual_registry =
      SidePanelRegistry::Get(selection.new_contents);
  if (new_contextual_registry) {
    registry_observations_.AddObservation(new_contextual_registry);
    combobox_model_->AddItems(new_contextual_registry->entries());
  }

  // Show an entry in the following fallback order: new contextual registry's
  // active entry > active global entry > none (close the side panel).
  if (GetContentView()) {
    // Attempt to find a suitable entry to be shown after the tab switch and if
    // one is found, show it.
    if (auto* new_active_entry = GetNewActiveEntryOnTabChanged()) {
      Show(new_active_entry, SidePanelUtil::SidePanelOpenTrigger::kTabChanged);
      SetSelectedEntryInCombobox(new_active_entry->key());
    } else {
      // If there is no suitable entry to be shown after the tab switch, cache
      // the view of the old contextual registry (if it was active), and close
      // the side panel.
      if (old_contextual_registry && old_contextual_registry->active_entry() &&
          *old_contextual_registry->active_entry() == current_entry_.get()) {
        auto* content_wrapper =
            GetContentView()->GetViewByID(kSidePanelContentWrapperViewId);
        DCHECK(content_wrapper);
        DCHECK(content_wrapper->children().size() == 1);
        auto current_entry_view = content_wrapper->RemoveChildViewT(
            content_wrapper->children().front());
        auto* active_entry = old_contextual_registry->active_entry().value();
        active_entry->CacheView(std::move(current_entry_view));
      }
      Close();
    }
  } else if (new_contextual_registry &&
             new_contextual_registry->active_entry().has_value()) {
    Show(new_contextual_registry->active_entry().value(),
         SidePanelUtil::SidePanelOpenTrigger::kTabChanged);
  }
}

void SidePanelCoordinator::UpdateNewTabButtonState() {
  if (header_open_in_new_tab_button_ && current_entry_) {
    header_open_in_new_tab_button_->SetEnabled(
        current_entry_->GetOpenInNewTabURL().is_valid());
  }
}

void SidePanelCoordinator::UpdateToolbarButtonHighlight(
    bool side_panel_visible) {
  auto* side_panel_button = browser_view_->toolbar()->GetSidePanelButton();
  side_panel_button->SetHighlighted(side_panel_visible);
  side_panel_button->SetTooltipText(l10n_util::GetStringUTF16(
      side_panel_visible ? IDS_TOOLTIP_SIDE_PANEL_HIDE
                         : IDS_TOOLTIP_SIDE_PANEL_SHOW));
}

void SidePanelCoordinator::OnViewVisibilityChanged(views::View* observed_view,
                                                   views::View* starting_from) {
  UpdateToolbarButtonHighlight(observed_view->GetVisible());
}
