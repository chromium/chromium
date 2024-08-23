// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_header.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/lens/lens_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo_result.h"
#include "ui/actions/action_id.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int kSidePanelContentWrapperViewId = 43;

void ConfigureControlButton(views::ImageButton* button) {
  button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  views::InstallCircleHighlightPathGenerator(button);

  int minimum_button_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_BUTTON_MINIMUM_SIZE);
  button->SetMinimumImageSize(
      gfx::Size(minimum_button_size, minimum_button_size));

  button->SetProperty(
      views::kMarginsKey,
      gfx::Insets().set_left(ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::
              DISTANCE_SIDE_PANEL_HEADER_INTERIOR_MARGIN_HORIZONTAL)));

  button->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification().WithAlignment(views::LayoutAlignment::kEnd));
}

std::unique_ptr<views::ToggleImageButton> CreatePinToggleButton(
    base::RepeatingClosure pressed_callback) {
  auto button =
      std::make_unique<views::ToggleImageButton>(std::move(pressed_callback));
  views::ConfigureVectorImageButton(button.get());
  ConfigureControlButton(button.get());
  button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_HEADER_PIN_BUTTON_TOOLTIP));
  button->SetToggledTooltipText(
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_HEADER_UNPIN_BUTTON_TOOLTIP));

  int dip_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE);
  const gfx::VectorIcon& pin_icon = kKeepIcon;
  const gfx::VectorIcon& unpin_icon = kKeepFilledIcon;
  views::SetImageFromVectorIconWithColorId(
      button.get(), pin_icon, kColorSidePanelHeaderButtonIcon,
      kColorSidePanelHeaderButtonIconDisabled, dip_size);
  const ui::ImageModel& normal_image = ui::ImageModel::FromVectorIcon(
      unpin_icon, kColorSidePanelHeaderButtonIcon, dip_size);
  const ui::ImageModel& disabled_image = ui::ImageModel::FromVectorIcon(
      unpin_icon, kColorSidePanelHeaderButtonIconDisabled, dip_size);
  button->SetToggledImageModel(views::Button::STATE_NORMAL, normal_image);
  button->SetToggledImageModel(views::Button::STATE_DISABLED, disabled_image);
  button->SetProperty(views::kElementIdentifierKey,
                      kSidePanelPinButtonElementId);
  return button;
}

std::unique_ptr<views::ImageButton> CreateControlButton(
    views::View* host,
    base::RepeatingClosure pressed_callback,
    const gfx::VectorIcon& icon,
    const std::u16string& tooltip_text,
    ui::ElementIdentifier view_id,
    int dip_size) {
  auto button = views::CreateVectorImageButtonWithNativeTheme(
      pressed_callback, icon, dip_size, kColorSidePanelHeaderButtonIcon,
      kColorSidePanelHeaderButtonIconDisabled);
  button->SetTooltipText(tooltip_text);
  ConfigureControlButton(button.get());
  button->SetProperty(views::kElementIdentifierKey, view_id);

  return button;
}

std::unique_ptr<views::ImageView> CreateIcon() {
  std::unique_ptr<views::ImageView> icon = std::make_unique<views::ImageView>();
  const int horizontal_margin =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::
              DISTANCE_SIDE_PANEL_HEADER_INTERIOR_MARGIN_HORIZONTAL) *
      2;
  icon->SetProperty(views::kMarginsKey,
                    gfx::Insets().set_left(horizontal_margin));
  return icon;
}

std::unique_ptr<views::Label> CreateTitle() {
  std::unique_ptr<views::Label> title = std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL,
      views::style::STYLE_HEADLINE_5);

  title->SetEnabledColorId(kColorSidePanelEntryTitle);
  title->SetSubpixelRenderingEnabled(false);
  const int horizontal_margin =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::
              DISTANCE_SIDE_PANEL_HEADER_INTERIOR_MARGIN_HORIZONTAL) *
      2;
  title->SetProperty(views::kMarginsKey,
                     gfx::Insets().set_left(horizontal_margin));
  title->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithAlignment(views::LayoutAlignment::kStart));
  return title;
}

using PopulateSidePanelCallback = base::OnceCallback<void(
    SidePanelEntry* entry,
    std::optional<std::unique_ptr<views::View>> content_view)>;

// SidePanelContentSwappingContainer is used as the content wrapper for views
// hosted in the side panel. This uses the SidePanelContentProxy to check if or
// wait for a SidePanelEntry's content view to be ready to be shown then only
// swaps the views when the content is ready. If the SidePanelContextProxy
// doesn't exist, the content is swapped immediately.
class SidePanelContentSwappingContainer : public views::View {
  METADATA_HEADER(SidePanelContentSwappingContainer, views::View)

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

  ~SidePanelContentSwappingContainer() override { loading_entry_ = nullptr; }

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
      loading_entry_ = entry->GetWeakPtr();
      loaded_callback_ = std::move(callback);
      content_proxy->SetAvailableCallback(
          base::BindOnce(&SidePanelContentSwappingContainer::RunLoadedCallback,
                         base::Unretained(this)));
    }
  }

  void ResetLoadingEntryIfNecessary() {
    if (loading_entry_) {
      loading_entry_->ResetLoadTimestamp();
      if (loading_entry_->CachedView()) {
        // The available callback here is used for showing the entry once it has
        // loaded. We need to reset this to make sure it is not triggered to be
        // shown once available.
        SidePanelUtil::GetSidePanelContentProxy(loading_entry_->CachedView())
            ->ResetAvailableCallback();
      }
    }
    loading_entry_ = nullptr;
  }

  void SetNoDelaysForTesting(bool no_delays_for_testing) {
    show_immediately_for_testing_ = no_delays_for_testing;
  }

  SidePanelEntry* loading_entry() const {
    return loading_entry_ ? loading_entry_.get() : nullptr;
  }

 private:
  void RunLoadedCallback() {
    // |loading_entry_| could be a nullptr here if it has been reset during
    // SidePanelContentSwappingContainer destruction.
    if (!loading_entry_) {
      return;
    }
    DCHECK(!loaded_callback_.is_null());
    SidePanelEntry* entry = loading_entry_.get();
    loading_entry_ = nullptr;
    std::move(loaded_callback_).Run(entry, std::nullopt);
  }

  // When true, don't delay switching panels.
  bool show_immediately_for_testing_;
  // Use a weak pointer so that loading side panel entry can be reset
  // automatically if the entry is destroyed.
  base::WeakPtr<SidePanelEntry> loading_entry_ = nullptr;
  PopulateSidePanelCallback loaded_callback_;
};

BEGIN_METADATA(SidePanelContentSwappingContainer)
END_METADATA

}  // namespace

SidePanelCoordinator::SidePanelCoordinator(BrowserView* browser_view)
    : browser_view_(browser_view) {
  pinned_model_observation_.Observe(
      PinnedToolbarActionsModel::Get(browser_view_->GetProfile()));
  // When the SidePanelPinning feature is enabled observe changes to the
  // pinned actions so we can update the pin button appropriately.
  // TODO(b/310910098): Observe the PinnedToolbarActionModel instead when
  // pinned extensions are fully merged into it.
  extensions_model_observation_.Observe(
      ToolbarActionsModel::Get(browser_view_->browser()->profile()));

  window_registry_ = std::make_unique<SidePanelRegistry>();

  browser_view_->browser()->tab_strip_model()->AddObserver(this);

  SidePanelUtil::PopulateGlobalEntries(browser_view->browser(),
                                       window_registry_.get());
  browser_view_->unified_side_panel()->AddHeaderView(CreateHeader());

  if (!browser_view_->GetIsWebAppType()) {
    browser_view_->MaybeShowStartupFeaturePromo(
        feature_engagement::kIPHSidePanelGenericMenuFeature);
  }

  // Add observation for the window registry after global entries have been
  // populated. This avoids re-entrancy during construction.
  registry_observations_.AddObservation(window_registry_.get());
}

SidePanelCoordinator::~SidePanelCoordinator() = default;

void SidePanelCoordinator::TearDownPreBrowserViewDestruction() {
  extensions_model_observation_.Reset();
  pinned_model_observation_.Reset();
}

SidePanelRegistry* SidePanelCoordinator::GetWindowRegistry() {
  return window_registry_.get();
}

void SidePanelCoordinator::OnToolbarPinnedActionsChanged() {
  UpdateHeaderPinButtonState();
}

actions::ActionItem* SidePanelCoordinator::GetActionItem(
    SidePanelEntry::Key entry_key) {
  BrowserActions* const browser_actions =
      browser_view_->browser()->browser_actions();
  if (entry_key.id() == SidePanelEntryId::kExtension) {
    std::optional<actions::ActionId> extension_action_id =
        actions::ActionIdMap::StringToActionId(entry_key.ToString());
    CHECK(extension_action_id.has_value());
    actions::ActionItem* const action_item =
        actions::ActionManager::Get().FindAction(
            extension_action_id.value(), browser_actions->root_action_item());
    CHECK(action_item);
    return action_item;
  }

  std::optional<actions::ActionId> action_id =
      SidePanelEntryIdToActionId(entry_key.id());
  CHECK(action_id.has_value());
  return actions::ActionManager::Get().FindAction(
      action_id.value(), browser_actions->root_action_item());
}

void SidePanelCoordinator::Show(
    SidePanelEntry::Id entry_id,
    std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger) {
  Show(SidePanelEntry::Key(entry_id), open_trigger);
}

void SidePanelCoordinator::Show(
    SidePanelEntry::Key entry_key,
    std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger) {
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

void SidePanelCoordinator::Close() {
  Close(/*suppress_animations=*/false);
}

void SidePanelCoordinator::Toggle(
    SidePanelEntryKey key,
    SidePanelUtil::SidePanelOpenTrigger open_trigger) {
  // If an entry is already showing in the sidepanel, the sidepanel
  // should be closed.
  if (IsSidePanelEntryShowing(key) &&
      !browser_view_->unified_side_panel()->IsClosing()) {
    Close();
    return;
  }

  // If the entry is the loading entry and is toggled,
  // it should also be closed. This handles quick double clicks
  // to close the sidepanel.
  if (IsSidePanelShowing()) {
    SidePanelContentSwappingContainer* content_wrapper =
        static_cast<SidePanelContentSwappingContainer*>(
            browser_view_->unified_side_panel()->GetViewByID(
                kSidePanelContentWrapperViewId));

    if (content_wrapper->loading_entry() == GetEntryForKey(key)) {
      content_wrapper->ResetLoadingEntryIfNecessary();
      Close();
      return;
    }
  }

  Show(key, open_trigger);
}

void SidePanelCoordinator::OpenInNewTab() {
  if (!browser_view_->unified_side_panel() || !current_entry_) {
    return;
  }

  GURL new_tab_url = current_entry_->GetOpenInNewTabURL();
  if (!new_tab_url.is_valid())
    return;

  SidePanelUtil::RecordNewTabButtonClicked(current_entry_->key().id());
  content::OpenURLParams params(new_tab_url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                /*is_renderer_initiated=*/false);
  browser_view_->browser()->OpenURL(params, /*navigation_handle_callback=*/{});
  Close();
}

void SidePanelCoordinator::UpdatePinState() {
  Profile* const profile = browser_view_->GetProfile();

  std::optional<actions::ActionId> action_id =
      GetActionItem(current_entry_->key())->GetActionId();
  CHECK(action_id.has_value());

  bool updated_pin_state = false;

  // TODO(b/310910098): Clean condition up once/if ToolbarActionModel and
  // PinnedToolbarActionModel are merged together.
  if (const std::optional<extensions::ExtensionId> extension_id =
          current_entry_->key().extension_id();
      extension_id.has_value()) {
    ToolbarActionsModel* const actions_model =
        ToolbarActionsModel::Get(profile);

    updated_pin_state = !actions_model->IsActionPinned(*extension_id);
    actions_model->SetActionVisibility(*extension_id, updated_pin_state);
  } else {
    PinnedToolbarActionsModel* const actions_model =
        PinnedToolbarActionsModel::Get(profile);

    updated_pin_state = !actions_model->Contains(action_id.value());
    actions_model->UpdatePinnedState(action_id.value(), updated_pin_state);
  }

  SidePanelUtil::RecordPinnedButtonClicked(current_entry_->key().id(),
                                           updated_pin_state);
  header_pin_button_->GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(updated_pin_state ? IDS_SIDE_PANEL_PINNED
                                                  : IDS_SIDE_PANEL_UNPINNED));

  // Close/cancel IPH for side panel pinning, if shown.
  MaybeEndPinPromo(/*pinned=*/true);
}

std::optional<SidePanelEntry::Id> SidePanelCoordinator::GetCurrentEntryId()
    const {
  return current_entry_
             ? std::optional<SidePanelEntry::Id>(current_entry_->key().id())
             : std::nullopt;
}

bool SidePanelCoordinator::IsSidePanelShowing() const {
  if (auto* side_panel = browser_view_->unified_side_panel()) {
    return side_panel->GetVisible();
  }
  return false;
}

bool SidePanelCoordinator::IsSidePanelEntryShowing(
    const SidePanelEntry::Key& entry_key) const {
  return IsSidePanelShowing() && current_entry_ &&
         current_entry_->key() == entry_key;
}

content::WebContents* SidePanelCoordinator::GetWebContentsForTest(
    SidePanelEntryId id) {
  if (auto* entry = GetEntryForKey(SidePanelEntryKey(id))) {
    entry->CacheView(entry->GetContent());
    if (entry->CachedView()) {
      if (auto* view = entry->CachedView()->GetViewByID(
              SidePanelWebUIView::kSidePanelWebViewId)) {
        return (static_cast<views::WebView*>(view))->web_contents();
      }
    }
  }
  return nullptr;
}

void SidePanelCoordinator::DisableAnimationsForTesting() {
  browser_view_->unified_side_panel()
      ->DisableAnimationsForTesting();  // IN-TEST
}

SidePanelEntry* SidePanelCoordinator::GetLoadingEntryForTesting() const {
  return GetLoadingEntry();
}

bool SidePanelCoordinator::IsSidePanelEntryShowing(
    const SidePanelEntry* entry) const {
  return IsSidePanelShowing() && current_entry_ &&
         current_entry_.get() == entry;
}

void SidePanelCoordinator::Show(
    SidePanelEntry* entry,
    std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
    bool suppress_animations) {
  // Side panel is not supported for non-normal browsers.
  if (!browser_view_->browser()->is_type_normal()) {
    return;
  }

  if (!entry) {
    return;
  }

  if (browser_view_->unified_side_panel()->GetViewByID(
          kSidePanelContentWrapperViewId) == nullptr) {
    InitializeSidePanel();
  }

  if (!IsSidePanelShowing()) {
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
    browser_view_->browser()->window()->CloseFeaturePromo(
        feature_engagement::kIPHReadingModeSidePanelFeature);

    // Close IPH for side panel menu, if shown.
    browser_view_->browser()->window()->CloseFeaturePromo(
        feature_engagement::kIPHSidePanelGenericMenuFeature);
  }

  SidePanelUtil::RecordSidePanelShowOrChangeEntryTrigger(open_trigger);

  // If the side panel was in the process of closing, notify observers that the
  // close was cancelled.
  if (browser_view_->unified_side_panel()->IsClosing()) {
    for (SidePanelViewStateObserver& view_state_observer :
         view_state_observers_) {
      view_state_observer.OnSidePanelCloseInterrupted();
    }
  }

  SidePanelContentSwappingContainer* content_wrapper =
      static_cast<SidePanelContentSwappingContainer*>(
          browser_view_->unified_side_panel()->GetViewByID(
              kSidePanelContentWrapperViewId));
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
    content_wrapper->SetProperty(
        kSidePanelContentStateKey,
        static_cast<std::underlying_type_t<SidePanelContentState>>(
            SidePanelContentState::kReadyToShow));
    if (content_wrapper->loading_entry()) {
      content_wrapper->ResetLoadingEntryIfNecessary();
    }
    if (browser_view_->toolbar()->pinned_toolbar_actions_container()) {
      NotifyPinnedContainerOfActiveStateChange(entry->key(), true);
    }
    return;
  }

  SidePanelUtil::RecordEntryShowTriggeredMetrics(
      browser_view_->browser(), entry->key().id(), open_trigger);

  content_wrapper->RequestEntry(
      entry, base::BindOnce(&SidePanelCoordinator::PopulateSidePanel,
                            base::Unretained(this), suppress_animations));
}

void SidePanelCoordinator::Close(bool suppress_animations) {
  if (!IsSidePanelShowing() ||
      browser_view_->unified_side_panel()->IsClosing()) {
    return;
  }

  if (current_entry_) {
    if (browser_view_->toolbar()->pinned_toolbar_actions_container()) {
      NotifyPinnedContainerOfActiveStateChange(current_entry_->key(), false);
    }
    current_entry_->OnEntryWillHide(SidePanelEntryHideReason::kSidePanelClosed);
  }
  if (views::View* content_wrapper =
          browser_view_->unified_side_panel()->GetViewByID(
              kSidePanelContentWrapperViewId)) {
    content_wrapper->SetProperty(
        kSidePanelContentStateKey,
        static_cast<std::underlying_type_t<SidePanelContentState>>(
            suppress_animations ? SidePanelContentState::kHideImmediately
                                : SidePanelContentState::kReadyToHide));
  }

  MaybeEndPinPromo(/*pinned=*/false);
}

SidePanelEntry* SidePanelCoordinator::GetEntryForKey(
    const SidePanelEntry::Key& entry_key) const {
  if (auto* contextual_entry = GetActiveContextualEntryForKey(entry_key)) {
    return contextual_entry;
  }

  return window_registry_->GetEntryForKey(entry_key);
}

SidePanelEntry* SidePanelCoordinator::GetActiveContextualEntryForKey(
    const SidePanelEntry::Key& entry_key) const {
  return GetActiveContextualRegistry()
             ? GetActiveContextualRegistry()->GetEntryForKey(entry_key)
             : nullptr;
}

SidePanelEntry* SidePanelCoordinator::GetLoadingEntry() const {
  SidePanelContentSwappingContainer* content_wrapper =
      static_cast<SidePanelContentSwappingContainer*>(
          browser_view_->unified_side_panel()->GetViewByID(
              kSidePanelContentWrapperViewId));
  DCHECK(content_wrapper);
  return content_wrapper->loading_entry();
}

bool SidePanelCoordinator::IsGlobalEntryShowing(
    const SidePanelEntry::Key& entry_key) const {
  if (!IsSidePanelShowing() || !current_entry_) {
    return false;
  }

  return window_registry_->GetEntryForKey(entry_key) == current_entry_.get();
}

void SidePanelCoordinator::InitializeSidePanel() {
  auto content_wrapper = std::make_unique<SidePanelContentSwappingContainer>(
      no_delays_for_testing_);  // Set to not visible so that the side panel is
                                // not shown until content is
  // ready to be shown.
  content_wrapper->SetVisible(false);

  browser_view_->unified_side_panel()->AddChildView(std::move(content_wrapper));
}

void SidePanelCoordinator::PopulateSidePanel(
    bool suppress_animations,
    SidePanelEntry* entry,
    std::optional<std::unique_ptr<views::View>> content_view) {
  actions::ActionItem* const action_item = GetActionItem(entry->key());
  UpdatePanelIconAndTitle(
      action_item->GetImage(), action_item->GetText(),
      entry->GetProperty(kShouldShowTitleInSidePanelHeaderKey),
      (entry->key().id() == SidePanelEntryId::kExtension));

  auto* content_wrapper = browser_view_->unified_side_panel()->GetViewByID(
      kSidePanelContentWrapperViewId);
  DCHECK(content_wrapper);
  // |content_wrapper| should have either no child views or one child view for
  // the currently hosted SidePanelEntry.
  DCHECK(content_wrapper->children().size() <= 1);

  const bool opening_side_panel = !IsSidePanelShowing();

  content_wrapper->SetVisible(true);
  content_wrapper->SetProperty(
      kSidePanelContentStateKey,
      static_cast<std::underlying_type_t<SidePanelContentState>>(
          suppress_animations ? SidePanelContentState::kShowImmediately
                              : SidePanelContentState::kReadyToShow));

  if (current_entry_ && content_wrapper->children().size()) {
    current_entry_->OnEntryWillHide(SidePanelEntryHideReason::kReplaced);
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
  if (browser_view_->toolbar()->pinned_toolbar_actions_container()) {
    NotifyPinnedContainerOfActiveStateChange(entry->key(), true);
    // Notify active state change only if the entry ids for the side panel are
    // different. This is to ensure extensions container isn't notified if we
    // switch between different extensions side panels or between global to
    // contextual side panel of the same extension.
    if (previous_entry && previous_entry->key().id() != entry->key().id()) {
      NotifyPinnedContainerOfActiveStateChange(previous_entry->key(), false);
    }
  }
  entry->OnEntryShown();
  if (previous_entry) {
    previous_entry->OnEntryHidden();
  } else {
    content->RequestFocus();
  }
  UpdateNewTabButtonState();
  UpdateHeaderPinButtonState();

  // Notify the observers when the side panel is opened (made visible). However,
  // the observers are not renotified when the side panel entry changes.
  if (opening_side_panel) {
    for (SidePanelViewStateObserver& view_state_observer :
         view_state_observers_) {
      view_state_observer.OnSidePanelDidOpen();
    }
  }

  if (base::FeatureList::IsEnabled(features::kSidePanelResizing)) {
    const base::Value::Dict& dict =
        browser_view_->browser()->profile()->GetPrefs()->GetDict(
            prefs::kSidePanelIdToWidth);
    std::string current_entry_id = SidePanelEntryIdToString(entry->key().id());

    std::optional<int> default_width = dict.FindInt(current_entry_id);

    if (default_width.has_value()) {
      auto* sp = browser_view_->unified_side_panel();
      sp->SetPanelWidth(default_width.value());
    }
  }
}

void SidePanelCoordinator::ClearCachedEntryViews() {
  window_registry_->ClearCachedEntryViews();
  TabStripModel* model = browser_view_->browser()->tab_strip_model();
  for (int index = 0; index < model->count(); ++index) {
    auto* tab =
        browser_view_->browser()->tab_strip_model()->GetTabAtIndex(index);
    tab->GetTabFeatures()->side_panel_registry()->ClearCachedEntryViews();
  }
}

SidePanelRegistry* SidePanelCoordinator::GetActiveContextualRegistry() const {
  if (browser_view_->browser()->tab_strip_model()->empty()) {
    return nullptr;
  }
  return browser_view_->browser()
      ->GetActiveTabInterface()
      ->GetTabFeatures()
      ->side_panel_registry();
}

std::unique_ptr<views::View> SidePanelCoordinator::CreateHeader() {
  auto header = std::make_unique<SidePanelHeader>();
  auto* const layout =
      header->SetLayoutManager(std::make_unique<views::FlexLayout>());

  // Set alignments for horizontal (main) and vertical (cross) axes.
  layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // The minimum cross axis size should the expected height of the header.
  constexpr int kDefaultSidePanelHeaderHeight = 40;
  layout->SetMinimumCrossAxisSize(kDefaultSidePanelHeaderHeight);

  panel_icon_ = header->AddChildView(CreateIcon());
  panel_title_ = header->AddChildView(CreateTitle());

  header_pin_button_ =
      header->AddChildView(CreatePinToggleButton(base::BindRepeating(
          &SidePanelCoordinator::UpdatePinState, base::Unretained(this))));
  header_pin_button_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  // By default, the button's accessible description is set to the button's
  // tooltip text. For the pin button, we only want the accessible name to be
  // read on accessibility mode since it includes the tooltip text. Thus we set
  // the accessible description.
  header_pin_button_->GetViewAccessibility().SetDescription(
      std::u16string(), ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);
  // The icon is later set as visible for side panels that support it.
  header_pin_button_->SetVisible(false);

  header_open_in_new_tab_button_ = header->AddChildView(CreateControlButton(
      header.get(),
      base::BindRepeating(&SidePanelCoordinator::OpenInNewTab,
                          base::Unretained(this)),
      kOpenInNewIcon, l10n_util::GetStringUTF16(IDS_ACCNAME_OPEN_IN_NEW_TAB),
      kSidePanelOpenInNewTabButtonElementId,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE)));
  header_open_in_new_tab_button_->SetFocusBehavior(
      views::View::FocusBehavior::ALWAYS);
  // The icon is later set as visible for side panels that support it.
  header_open_in_new_tab_button_->SetVisible(false);

  auto* header_close_button = header->AddChildView(CreateControlButton(
      header.get(),
      base::BindRepeating(&SidePanelUI::Close, base::Unretained(this)),
      views::kIcCloseIcon, l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE),
      kSidePanelCloseButtonElementId,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE)));
  header_close_button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  return header;
}

SidePanelEntry* SidePanelCoordinator::GetNewActiveEntryOnDeregister(
    SidePanelRegistry* deregistering_registry,
    const SidePanelEntry::Key& key) {
  // This function should only be called when the side panel view is shown.
  DCHECK(IsSidePanelShowing());

  // Attempt to return an entry in the following fallback order: global entry
  // for `key` if a contextual entry is deregistered > active global entry >
  // null.
  if (deregistering_registry == GetActiveContextualRegistry() &&
      window_registry_->GetEntryForKey(key)) {
    return window_registry_->GetEntryForKey(key);
  }

  return window_registry_->active_entry().value_or(nullptr);
}

SidePanelEntry* SidePanelCoordinator::GetNewActiveEntryOnTabChanged() {
  // This function should only be called when the side panel view is shown.
  DCHECK(IsSidePanelShowing());

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
      window_registry_->GetEntryForKey(current_entry_->key())) {
    return GetEntryForKey(current_entry_->key());
  }

  return window_registry_->active_entry()
             ? GetEntryForKey((*window_registry_->active_entry())->key())
             : nullptr;
}

void SidePanelCoordinator::NotifyPinnedContainerOfActiveStateChange(
    SidePanelEntryKey key,
    bool is_active) {
  auto* toolbar_container =
      browser_view_->toolbar()->pinned_toolbar_actions_container();
  CHECK(toolbar_container);

  if (key.id() == SidePanelEntryId::kExtension) {
    browser_view_->toolbar()->extensions_container()->UpdateSidePanelState(
        is_active);
  } else {
    std::optional<actions::ActionId> action_id =
        SidePanelEntryIdToActionId(key.id());
    CHECK(action_id.has_value());
    toolbar_container->UpdateActionState(*action_id, is_active);
  }
}

void SidePanelCoordinator::MaybeQueuePinPromo() {
  // Which feature is shown depends on the specific side panel that is showing.
  const base::Feature* const iph_feature =
      (current_entry_->key().id() == SidePanelEntryId::kLensOverlayResults)
          ? &feature_engagement::kIPHSidePanelLensOverlayPinnableFeature
          : &feature_engagement::kIPHSidePanelGenericPinnableFeature;

  // If the desired promo hasn't changed, there's nothing to do.
  if (pending_pin_promo_ == iph_feature) {
    return;
  }

  // End or cancel the current promo.
  if (pending_pin_promo_) {
    MaybeEndPinPromo(/*pinned=*/false);
  }

  // Queue up the next promo to be shown, if there is one that can be shown.
  pending_pin_promo_ = iph_feature;
  if (iph_feature && !browser_view_->CanShowFeaturePromo(*iph_feature)
                          .is_blocked_this_instance()) {
    // Default to ten second delay, but allow setting a different parameter via
    // field trial.
    const base::TimeDelta delay = base::GetFieldTrialParamByFeatureAsTimeDelta(
        *iph_feature, "x_custom_iph_delay", base::Seconds(10));
    pin_promo_timer_.Start(FROM_HERE, delay,
                           base::BindOnce(&SidePanelCoordinator::ShowPinPromo,
                                          base::Unretained(this)));
  }
}

void SidePanelCoordinator::ShowPinPromo() {
  if (!pending_pin_promo_) {
    return;
  }

  browser_view_->browser()->window()->MaybeShowFeaturePromo(
      *pending_pin_promo_);
}

void SidePanelCoordinator::MaybeEndPinPromo(bool pinned) {
  if (!pending_pin_promo_) {
    return;
  }

  if (pinned) {
    browser_view_->CloseFeaturePromo(
        *pending_pin_promo_,
        user_education::EndFeaturePromoReason::kFeatureEngaged);
    if (pending_pin_promo_ ==
        &feature_engagement::kIPHSidePanelLensOverlayPinnableFeature) {
      browser_view_->NotifyPromoFeatureUsed(
          feature_engagement::kIPHSidePanelLensOverlayPinnableFeature);
      browser_view_->MaybeShowFeaturePromo(
          feature_engagement::kIPHSidePanelLensOverlayPinnableFollowupFeature);
    } else {
      browser_view_->NotifyFeatureEngagementEvent(
          feature_engagement::events::kSidePanelPinned);
    }
  } else {
    browser_view_->CloseFeaturePromo(
        *pending_pin_promo_,
        user_education::EndFeaturePromoReason::kAbortPromo);
  }

  pin_promo_timer_.Stop();
  pending_pin_promo_ = nullptr;
}

void SidePanelCoordinator::OnEntryRegistered(SidePanelRegistry* registry,
                                             SidePanelEntry* entry) {
  // If `entry` is a contextual entry and the global entry with the same key is
  // currently being shown, show the new `entry`.
  if (registry == GetActiveContextualRegistry() &&
      IsGlobalEntryShowing(entry->key())) {
    Show(entry, SidePanelUtil::SidePanelOpenTrigger::kExtensionEntryRegistered);
  }
}

void SidePanelCoordinator::OnEntryWillDeregister(SidePanelRegistry* registry,
                                                 SidePanelEntry* entry) {
  // Save the entry's view: if it has a cached view, retrieve it. Otherwise if
  // the entry is shown, get it from the side panel view. This is necessary so
  // the view can be preserved so it won't be destroyed by Close().
  std::unique_ptr<views::View> entry_view =
      entry->CachedView() ? entry->GetContent() : nullptr;

  // Update the current entry to make sure we don't show an entry that is being
  // removed or close the panel if the entry being deregistered is the only one
  // that has been visible.
  if (IsSidePanelShowing() &&
      !browser_view_->unified_side_panel()->IsClosing() && current_entry_ &&
      (current_entry_->key() == entry->key())) {
    // If a global entry is deregistered but a contextual entry with the same
    // key is shown, do nothing.
    if (registry == window_registry_.get() &&
        GetActiveContextualEntryForKey(entry->key())) {
      entry->CacheView(std::move(entry_view));
      return;
    }

    // Fetch the entry's view from the side panel container if it is shown.
    auto* content_wrapper = browser_view_->unified_side_panel()->GetViewByID(
        kSidePanelContentWrapperViewId);
    DCHECK(content_wrapper);
    if (content_wrapper->children().size() == 1) {
      entry_view = content_wrapper->RemoveChildViewT(
          content_wrapper->children().front());
      // TODO(crbug.com/40897366): Log the time elapsed between when this view
      // is removed, to when the new active entry's view is shown. This can
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

  // Only background tabs can be discarded. In this case, nothing needs to
  // happen.
  if (change.type() == TabStripModelChange::kReplaced) {
    return;
  }

  // Handle removing the previous tab's contextual registry if one exists. In
  // the event that the tab was removed for deletion, registry removal is
  // already handled by SidePanelCoordinator::OnRegistryDestroying
  bool removed_for_deletion =
      (change.type() == TabStripModelChange::kRemoved) &&
      (change.GetRemove()->contents[0].remove_reason ==
       TabStripModelChange::RemoveReason::kDeleted);
  SidePanelRegistry* old_contextual_registry = nullptr;
  if (!removed_for_deletion && selection.old_contents) {
    old_contextual_registry =
        SidePanelRegistry::GetDeprecated(selection.old_contents);
    if (old_contextual_registry) {
      registry_observations_.RemoveObservation(old_contextual_registry);
    }
  }

  // Add the current tab's contextual registry.
  SidePanelRegistry* new_contextual_registry = nullptr;
  if (selection.new_contents) {
    new_contextual_registry =
        SidePanelRegistry::GetDeprecated(selection.new_contents);
    // Registries are per-tab whereas this is listening to WebContents changes.
    // During a tab-discard the WebContents changes but the tab stays the same,
    // hence the need to check if the source is already being observed. This
    // observer method should eventually be replaced with a tab observation
    // method.
    if (new_contextual_registry &&
        !registry_observations_.IsObservingSource(new_contextual_registry)) {
      registry_observations_.AddObservation(new_contextual_registry);
    }
  }

  // Show an entry in the following fallback order: new contextual registry's
  // active entry > active global entry > none (close the side panel).
  if (IsSidePanelShowing() &&
      !browser_view_->unified_side_panel()->IsClosing()) {
    // Attempt to find a suitable entry to be shown after the tab switch and if
    // one is found, show it.
    if (auto* new_active_entry = GetNewActiveEntryOnTabChanged()) {
      Show(new_active_entry, SidePanelUtil::SidePanelOpenTrigger::kTabChanged,
           /*suppress_animations=*/true);
    } else {
      // If there is no suitable entry to be shown after the tab switch, cache
      // the view of the old contextual registry (if it was active), and close
      // the side panel.
      if (old_contextual_registry && old_contextual_registry->active_entry() &&
          *old_contextual_registry->active_entry() == current_entry_.get()) {
        auto* content_wrapper =
            browser_view_->unified_side_panel()->GetViewByID(
                kSidePanelContentWrapperViewId);
        DCHECK(content_wrapper);
        DCHECK(content_wrapper->children().size() == 1);
        auto current_entry_view = content_wrapper->RemoveChildViewT(
            content_wrapper->children().front());
        auto* active_entry = old_contextual_registry->active_entry().value();
        active_entry->CacheView(std::move(current_entry_view));
      }
      Close(/*suppress_animations=*/true);
    }
  } else if (new_contextual_registry &&
             new_contextual_registry->active_entry().has_value()) {
    Show(new_contextual_registry->active_entry().value(),
         SidePanelUtil::SidePanelOpenTrigger::kTabChanged,
         /*suppress_animations=*/true);
  }
}

void SidePanelCoordinator::UpdateNewTabButtonState() {
  if (header_open_in_new_tab_button_ && current_entry_) {
    bool has_open_in_new_tab_button =
        current_entry_->SupportsNewTabButton() &&
        current_entry_->GetOpenInNewTabURL().is_valid();
    header_open_in_new_tab_button_->SetVisible(has_open_in_new_tab_button);
  }
}

void SidePanelCoordinator::UpdateHeaderPinButtonState() {
  if (!IsSidePanelShowing() || !current_entry_) {
    return;
  }

  Profile* const profile = browser_view_->GetProfile();
  actions::ActionItem* const action_item = GetActionItem(current_entry_->key());
  std::optional<actions::ActionId> action_id = action_item->GetActionId();
  CHECK(action_id.has_value());

  bool current_pinned_state = false;

  // TODO(b/310910098): Clean condition up once/if ToolbarActionModel and
  // PinnedToolbarActionModel are merged together.
  if (const std::optional<extensions::ExtensionId> extension_id =
          current_entry_->key().extension_id();
      extension_id.has_value()) {
    ToolbarActionsModel* const actions_model =
        ToolbarActionsModel::Get(profile);

    current_pinned_state = actions_model->IsActionPinned(*extension_id);
  } else {
    PinnedToolbarActionsModel* const actions_model =
        PinnedToolbarActionsModel::Get(profile);

    current_pinned_state = actions_model->Contains(action_id.value());
  }

  header_pin_button_->SetToggled(current_pinned_state);
  header_pin_button_->SetVisible(
      !profile->IsIncognitoProfile() && !profile->IsGuestSession() &&
      action_item->GetProperty(actions::kActionItemPinnableKey));

  if (!current_pinned_state) {
    // Show IPH for side panel pinning icon.
    MaybeQueuePinPromo();
  }
}

void SidePanelCoordinator::SetNoDelaysForTesting(bool no_delays_for_testing) {
  no_delays_for_testing_ = no_delays_for_testing;
  if (SidePanel* side_panel = browser_view_->unified_side_panel()) {
    if (auto* wrapper = static_cast<SidePanelContentSwappingContainer*>(
            side_panel->GetViewByID(kSidePanelContentWrapperViewId))) {
      wrapper->SetNoDelaysForTesting(no_delays_for_testing_);  // IN-TEST
    }
  }
}

void SidePanelCoordinator::UpdatePanelIconAndTitle(
    const ui::ImageModel& icon,
    const std::u16string& text,
    const bool should_show_title_text,
    const bool is_extension) {
  if (is_extension) {
    ui::ImageModel updated_icon = icon;
    if (icon.IsVectorIcon()) {
      updated_icon = ui::ImageModel::FromVectorIcon(
          *icon.GetVectorIcon().vector_icon(), kColorSidePanelEntryIcon,
          icon.GetVectorIcon().icon_size());
    }
    panel_icon_->SetImage(updated_icon);
  }
  panel_icon_->SetVisible(is_extension);
  panel_title_->SetText(should_show_title_text ? text : std::u16string());
}

void SidePanelCoordinator::OnViewVisibilityChanged(views::View* observed_view,
                                                   views::View* starting_from) {
  if (!observed_view->GetVisible()) {
    bool closing_global = false;
    if (current_entry_) {
      // Reset current_entry_ first to prevent current_entry->OnEntryHidden()
      // from calling multiple times. This could happen in the edge cases when
      // callback inside current_entry->OnEntryHidden() is calling Close() to
      // trigger race condition.
      auto* current_entry = current_entry_.get();
      current_entry_.reset();
      if (window_registry_->GetEntryForKey(current_entry->key()) ==
          current_entry) {
        closing_global = true;
      }
      current_entry->OnEntryHidden();
    }

    // Reset active entry values for all observed registries and clear cache for
    // everything except remaining active entries (i.e. if another tab has an
    // active contextual entry).
    if (auto* contextual_registry = GetActiveContextualRegistry()) {
      contextual_registry->ResetActiveEntry();
      if (closing_global) {
        // Reset last active entry in contextual registry as global entry should
        // take precedence.
        contextual_registry->ResetLastActiveEntry();
      }
    }
    window_registry_->ResetActiveEntry();
    ClearCachedEntryViews();

    // `OnEntryWillDeregister` (triggered by calling `OnEntryHidden`) may
    // already have deleted the content container, so check that it still
    // exists.
    if (auto* content_wrapper =
            browser_view_->unified_side_panel()->GetViewByID(
                kSidePanelContentWrapperViewId)) {
      if (!content_wrapper->children().empty()) {
        content_wrapper->RemoveChildViewT(content_wrapper->children().front());
      }
    }
    SidePanelUtil::RecordSidePanelClosed(opened_timestamp_);

    for (SidePanelViewStateObserver& view_state_observer :
         view_state_observers_) {
      view_state_observer.OnSidePanelDidClose();
    }
  }
}

void SidePanelCoordinator::OnActionsChanged() {
  if (current_entry_) {
    UpdateHeaderPinButtonState();
  }
}
