// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_combobox_model.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
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

constexpr SidePanelEntry::Id kDefaultEntry = SidePanelEntry::Id::kReadingList;

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
  global_registry->AddObserver(this);
  global_registry_ = global_registry.get();
  browser_view->browser()->SetUserData(kGlobalSidePanelRegistryKey,
                                       std::move(global_registry));

  browser_view_->browser()->tab_strip_model()->AddObserver(this);

  SidePanelUtil::PopulateGlobalEntries(browser_view->browser(),
                                       GetGlobalSidePanelRegistry());
}

SidePanelCoordinator::~SidePanelCoordinator() {
  browser_view_->browser()->tab_strip_model()->RemoveObserver(this);
  view_state_observers_.Clear();
}

void SidePanelCoordinator::Show(
    absl::optional<SidePanelEntry::Id> entry_id,
    absl::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger) {
  if (entry_id.has_value()) {
    Show(SidePanelEntry::Key(entry_id.value()), open_trigger);
  } else {
    Show(GetLastActiveEntryKey().value_or(SidePanelEntry::Key(kDefaultEntry)),
         open_trigger);
  }
}

void SidePanelCoordinator::Show(
    SidePanelEntry::Key entry_key,
    absl::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger) {
  SidePanelEntry* entry = GetEntryForKey(entry_key);
  if (!entry)
    return;

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
  }

  SidePanelContentSwappingContainer* content_wrapper =
      static_cast<SidePanelContentSwappingContainer*>(
          GetContentView()->GetViewByID(kSidePanelContentWrapperViewId));
  DCHECK(content_wrapper);

  // If we are already loading this entry, do nothing.
  if (content_wrapper->loading_entry() == entry)
    return;

  // If we are already showing this entry, make sure we prevent any loading
  // entry from showing once the load has finished. Say if we are showing A then
  // trigger B to show but switch back to A while B is still loading (and not
  // yet shown) we want to make sure B will not then be shown when it has
  // finished loading. Note, this does not cancel the triggered load of B, B
  // remains cached.
  if (current_entry_.get() == entry) {
    if (content_wrapper->loading_entry())
      content_wrapper->ResetLoadingEntryIfNecessary();
    return;
  }

  SidePanelUtil::RecordEntryShowTriggeredMetrics(entry->key().id(),
                                                 open_trigger);

  content_wrapper->RequestEntry(
      entry, base::BindOnce(&SidePanelCoordinator::PopulateSidePanel,
                            base::Unretained(this)));
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
  if (toolbar && toolbar->side_panel_button())
    toolbar->side_panel_button()->SetTooltipText(tooltip_text);
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
  if (IsSidePanelShowing()) {
    Close();
  } else {
    Show(absl::nullopt, SidePanelUtil::SidePanelOpenTrigger::kToolbarButton);
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

SidePanelRegistry* SidePanelCoordinator::GetGlobalSidePanelRegistry() {
  return static_cast<SidePanelRegistry*>(
      browser_view_->browser()->GetUserData(kGlobalSidePanelRegistryKey));
}

void SidePanelCoordinator::SetNoDelaysForTesting() {
  no_delays_for_testing_ = true;
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
  SidePanelContentSwappingContainer* content_wrapper =
      static_cast<SidePanelContentSwappingContainer*>(
          GetContentView()->GetViewByID(kSidePanelContentWrapperViewId));
  DCHECK(content_wrapper);
  return content_wrapper->loading_entry();
}

bool SidePanelCoordinator::IsSidePanelShowing() {
  return GetContentView() != nullptr;
}

views::View* SidePanelCoordinator::GetContentView() const {
  return browser_view_->unified_side_panel()->GetViewByID(
      kSidePanelContentViewId);
}

SidePanelEntry* SidePanelCoordinator::GetEntryForKey(
    const SidePanelEntry::Key& entry_key) {
  if (auto* entry = global_registry_->GetEntryForKey(entry_key)) {
    return entry;
  }
  if (auto* contextual_registry = GetActiveContextualRegistry()) {
    if (auto* entry = contextual_registry->GetEntryForKey(entry_key)) {
      return entry;
    }
  }
  return nullptr;
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
  header_combobox_->SetSelectedIndex(
      combobox_model_->GetIndexForKey(entry->key()));
  header_combobox_->SchedulePaint();

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
  const SidePanelContentSwappingContainer* content_wrapper =
      static_cast<SidePanelContentSwappingContainer*>(
          GetContentView()->GetViewByID(kSidePanelContentWrapperViewId));
  DCHECK(content_wrapper);
  if (const auto* entry = content_wrapper->loading_entry())
    return entry->key();

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
  combobox->SetSelectedIndex(combobox_model_->GetIndexForKey(
      (GetLastActiveEntryKey().value_or(SidePanelEntry::Key(kDefaultEntry)))));
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

void SidePanelCoordinator::OnEntryRegistered(SidePanelEntry* entry) {
  combobox_model_->AddItem(entry);
  if (GetContentView()) {
    header_combobox_->SetSelectedIndex(combobox_model_->GetIndexForKey(
        GetLastActiveEntryKey().value_or(SidePanelEntry::Key(kDefaultEntry))));
    header_combobox_->SchedulePaint();
  }
}

void SidePanelCoordinator::OnEntryWillDeregister(SidePanelEntry* entry) {
  absl::optional<SidePanelEntry::Key> selected_key = GetSelectedKey();
  combobox_model_->RemoveItem(entry->key());
  if (GetContentView()) {
    header_combobox_->SetSelectedIndex(combobox_model_->GetIndexForKey(
        GetLastActiveEntryKey().value_or(SidePanelEntry::Key(kDefaultEntry))));
    header_combobox_->SchedulePaint();
  }

  // If the active global entry is the entry being deregistered, reset
  // last_active_global_entry_key_.
  if (last_active_global_entry_key_.has_value() &&
      entry->key() == last_active_global_entry_key_.value()) {
    last_active_global_entry_key_ = absl::nullopt;
  }

  // Update the current entry to make sure we don't show an entry that is being
  // removed or close the panel if the entry being deregistered is the only one
  // that has been visible.
  if (GetContentView() && selected_key.has_value() &&
      selected_key.value() == entry->key()) {
    if (global_registry_->active_entry().has_value()) {
      Show(GetLastActiveEntryKey().value_or(SidePanelEntry::Key(kDefaultEntry)),
           SidePanelUtil::SidePanelOpenTrigger::kSidePanelEntryDeregistered);
    } else {
      Close();
    }
  }
}

void SidePanelCoordinator::OnEntryIconUpdated(SidePanelEntry* entry) {
  combobox_model_->UpdateIconForEntry(entry);
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
    old_contextual_registry->RemoveObserver(this);
    combobox_model_->RemoveItems(old_contextual_registry->entries());
  }

  // Add the current tab's contextual registry and update the combobox.
  auto* new_contextual_registry =
      SidePanelRegistry::Get(selection.new_contents);
  if (new_contextual_registry) {
    new_contextual_registry->AddObserver(this);
    combobox_model_->AddItems(new_contextual_registry->entries());
  }

  // If an active entry is available, show it. If not, close the panel.
  if (GetContentView()) {
    if ((!new_contextual_registry ||
         !new_contextual_registry->active_entry().has_value()) &&
        !global_registry_->active_entry().has_value()) {
      // Cache the view of the old contextual registry if it was active.
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
    } else {
      Show(GetLastActiveEntryKey().value_or(SidePanelEntry::Key(kDefaultEntry)),
           SidePanelUtil::SidePanelOpenTrigger::kTabChanged);
      header_combobox_->SetSelectedIndex(
          combobox_model_->GetIndexForKey((GetLastActiveEntryKey().value_or(
              SidePanelEntry::Key(kDefaultEntry)))));
      header_combobox_->SchedulePaint();
    }
  } else if (new_contextual_registry &&
             new_contextual_registry->active_entry().has_value()) {
    Show(new_contextual_registry->active_entry().value()->key().id(),
         SidePanelUtil::SidePanelOpenTrigger::kTabChanged);
  }
}

void SidePanelCoordinator::UpdateNewTabButtonState() {
  if (header_open_in_new_tab_button_ && current_entry_) {
    header_open_in_new_tab_button_->SetEnabled(
        current_entry_->GetOpenInNewTabURL().is_valid());
  }
}
