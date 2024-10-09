// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"

#include <memory>
#include <string>
#include <utility>

#include "base/cancelable_callback.h"
#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
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
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
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
#include "components/user_education/common/feature_promo_controller.h"
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
#include "ui/views/controls/button/menu_button.h"
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

}  // namespace

// This class uses the SidePanelContentProxy to wait for the SidePanelEntry's
// content view to be ready to be shown.
class SidePanelCoordinator::SidePanelEntryWaiter {
 public:
  // Calling this method cancels all previous calls to this method.
  // If the entry is destroyed while waiting, the callback is not invoked.
  void WaitForEntry(SidePanelEntry* entry, PopulateSidePanelCallback callback) {
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
      loaded_callback_.Reset(
          base::BindOnce(&SidePanelEntryWaiter::RunLoadedCallback,
                         base::Unretained(this), std::move(callback)));
      content_proxy->SetAvailableCallback(loaded_callback_.callback());
    }
  }

  void ResetLoadingEntryIfNecessary() {
    loading_entry_.reset();
    loaded_callback_.Cancel();
  }

  void SetNoDelaysForTesting(bool no_delays_for_testing) {
    show_immediately_for_testing_ = no_delays_for_testing;
  }

  SidePanelEntry* loading_entry() const { return loading_entry_.get(); }

 private:
  void RunLoadedCallback(PopulateSidePanelCallback callback) {
    // content_proxy is owned by content_view which is owned by SidePanelEntry.
    // If this callback runs then loading_entry_ must be valid.
    CHECK(loading_entry_);
    SidePanelEntry* entry = loading_entry_.get();
    loading_entry_.reset();
    std::move(callback).Run(entry, std::nullopt);
  }

  // When true, don't delay switching panels.
  bool show_immediately_for_testing_ = false;
  // Tracks the entry that is loading.
  base::WeakPtr<SidePanelEntry> loading_entry_;
  // This class will load at most one entry at a time. If a new one is
  // requested, the old one is canceled automatically.
  base::CancelableOnceClosure loaded_callback_;
};

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

  window_registry_ =
      std::make_unique<SidePanelRegistry>(browser_view_->browser());

  browser_view_->browser()->tab_strip_model()->AddObserver(this);

  browser_view_->unified_side_panel()->AddHeaderView(CreateHeader());

  waiter_ = std::make_unique<SidePanelEntryWaiter>();
}

SidePanelCoordinator::~SidePanelCoordinator() = default;

void SidePanelCoordinator::Init(Browser* browser) {
  SidePanelUtil::PopulateGlobalEntries(browser, window_registry_.get());
}

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
  std::optional<UniqueKey> unique_key = GetUniqueKeyForKey(entry_key);
  CHECK(unique_key.has_value());
  Show(unique_key.value(), open_trigger, /*suppress_animations=*/false);
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
    if (waiter_->loading_entry() == GetEntryForKey(key)) {
      waiter_->ResetLoadingEntryIfNecessary();
      Close();
      return;
    }
  }

  std::optional<UniqueKey> unique_key = GetUniqueKeyForKey(key);
  if (unique_key.has_value()) {
    Show(unique_key.value(), open_trigger, /*suppress_animations=*/false);
  }
}

void SidePanelCoordinator::OpenInNewTab() {
  if (!browser_view_->unified_side_panel() || !current_key_) {
    return;
  }

  GURL new_tab_url = GetEntryForUniqueKey(*current_key_)->GetOpenInNewTabURL();
  if (!new_tab_url.is_valid())
    return;

  SidePanelUtil::RecordNewTabButtonClicked(current_key_->key.id());
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
      GetActionItem(current_key_->key)->GetActionId();
  CHECK(action_id.has_value());

  bool updated_pin_state = false;

  // TODO(b/310910098): Clean condition up once/if ToolbarActionModel and
  // PinnedToolbarActionModel are merged together.
  if (const std::optional<extensions::ExtensionId> extension_id =
          current_key_->key.extension_id();
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

  SidePanelUtil::RecordPinnedButtonClicked(current_key_->key.id(),
                                           updated_pin_state);
  header_pin_button_->GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(updated_pin_state ? IDS_SIDE_PANEL_PINNED
                                                  : IDS_SIDE_PANEL_UNPINNED));

  // Close/cancel IPH for side panel pinning, if shown.
  MaybeEndPinPromo(/*pinned=*/true);
}

void SidePanelCoordinator::OpenMoreInfoMenu() {
  more_info_menu_model_ =
      GetEntryForUniqueKey(*current_key_)->GetMoreInfoMenuModel();
  CHECK(more_info_menu_model_);
  menu_runner_ = std::make_unique<views::MenuRunner>(
      more_info_menu_model_.get(), views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(header_more_info_button_->GetWidget(),
                          static_cast<views::MenuButtonController*>(
                              header_more_info_button_->button_controller()),
                          header_more_info_button_->GetAnchorBoundsInScreen(),
                          views::MenuAnchorPosition::kTopRight,
                          ui::MENU_SOURCE_NONE);
}

std::optional<SidePanelEntry::Id> SidePanelCoordinator::GetCurrentEntryId()
    const {
  return current_key_
             ? std::optional<SidePanelEntry::Id>(current_key_->key.id())
             : std::nullopt;
}

bool SidePanelCoordinator::IsSidePanelShowing() const {
  return current_key_.has_value();
}

bool SidePanelCoordinator::IsSidePanelEntryShowing(
    const SidePanelEntry::Key& entry_key) const {
  return current_key_ && current_key_->key == entry_key;
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

bool SidePanelCoordinator::IsSidePanelEntryShowing(
    const SidePanelEntry::Key& entry_key,
    bool for_tab) const {
  return current_key_ && current_key_->key == entry_key &&
         current_key_->tab_handle.has_value() == for_tab;
}

SidePanelEntry* SidePanelCoordinator::GetLoadingEntryForTesting() const {
  return waiter_->loading_entry();
}

void SidePanelCoordinator::Show(
    const UniqueKey& input,
    std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
    bool suppress_animations) {
  // Side panel is not supported for non-normal browsers.
  if (!browser_view_->browser()->is_type_normal()) {
    return;
  }

  SidePanelEntry* entry = GetEntryForUniqueKey(input);

  if (!IsSidePanelShowing()) {
    opened_timestamp_ = base::TimeTicks::Now();
    SidePanelUtil::RecordSidePanelOpen(open_trigger);
    // Record usage for side panel promo.
    feature_engagement::TrackerFactory::GetForBrowserContext(
        browser_view_->GetProfile())
        ->NotifyEvent("side_panel_shown");

    // Close IPH for side panel if shown.
    ClosePromoAndMaybeNotifyUsed(
        feature_engagement::kIPHReadingListInSidePanelFeature,
        SidePanelEntryId::kReadingList, input.key.id());
    ClosePromoAndMaybeNotifyUsed(
        feature_engagement::kIPHPowerBookmarksSidePanelFeature,
        SidePanelEntryId::kBookmarks, input.key.id());
    ClosePromoAndMaybeNotifyUsed(
        feature_engagement::kIPHReadingModeSidePanelFeature,
        SidePanelEntryId::kReadAnything, input.key.id());
  }

  SidePanelUtil::RecordSidePanelShowOrChangeEntryTrigger(open_trigger);

  // If the side panel was in the process of closing, notify observers that the
  // close was cancelled.
  if (browser_view_->unified_side_panel()->IsClosing()) {
    view_state_observers_.Notify(
        &SidePanelViewStateObserver::OnSidePanelCloseInterrupted);
  }

  // If the side panel is already showing, cancel all loads and do nothing.
  if (current_key_ && *current_key_ == input) {
    waiter_->ResetLoadingEntryIfNecessary();

    // If the side panel is in the process of closing, show it instead.
    if (browser_view_->unified_side_panel()->state() ==
        SidePanel::State::kClosing) {
      browser_view_->unified_side_panel()->Open(/*animated=*/true);
      NotifyPinnedContainerOfActiveStateChange(entry->key(), true);
    }
    return;
  }

  SidePanelUtil::RecordEntryShowTriggeredMetrics(
      browser_view_->browser(), entry->key().id(), open_trigger);

  waiter_->WaitForEntry(
      entry,
      base::BindOnce(&SidePanelCoordinator::PopulateSidePanel,
                     base::Unretained(this), suppress_animations, input));
}

// There are 3 different contexts in which the side panel can be closed. All go
// through Close(). These are:
//   (1) Some C++ code called Close(). This includes built-in features such as
//   LensOverlayController, extensions, and the user clicking the "X" button on
//   the side-panel header. This includes indirect code paths such as Toggle(),
//   and the active side-panel entry being deregistered. This is expected to
//   start the process of closing the side-panel. All tab and window-scoped
//   state is valid.
//   (2) This class was showing a tab-scoped side panel entry. That tab has
//   already been detached (e.g. closed). This class has been informed via
//   TabStripModel::OnTabStripModelChanged. The browser window is still valid
//   but all tab-scoped state is invalid.
//   (3) This class was showing a tab-scoped side panel entry. The window is in
//   the process of closing. All tabs have been detached, and this class was
//   informed via TabStripModel::OnTabStripModelChanged. Both window and
//   tab-scoped state is invalid.
//   (4) At the moment that this comment was written, if this class is showing
//   a window-scoped side-panel entry, and the window is closed via any
//   mechanism, this method is not called.
void SidePanelCoordinator::Close(bool suppress_animations) {
  if (!IsSidePanelShowing() ||
      browser_view_->unified_side_panel()->IsClosing()) {
    return;
  }

  if (current_key_) {
    if (browser_view_->toolbar()->pinned_toolbar_actions_container()) {
      NotifyPinnedContainerOfActiveStateChange(current_key_->key, false);
    }
    SidePanelEntry* entry = GetEntryForUniqueKey(*current_key_);
    if (entry) {
      entry->OnEntryWillHide(SidePanelEntryHideReason::kSidePanelClosed);
    }
  }
  browser_view_->unified_side_panel()->Close(
      /*animated=*/!suppress_animations);

  MaybeEndPinPromo(/*pinned=*/false);
}

SidePanelEntry* SidePanelCoordinator::GetEntryForKey(
    const SidePanelEntry::Key& entry_key) const {
  if (auto* contextual_entry = GetActiveContextualEntryForKey(entry_key)) {
    return contextual_entry;
  }

  return window_registry_->GetEntryForKey(entry_key);
}

std::optional<SidePanelCoordinator::UniqueKey>
SidePanelCoordinator::GetUniqueKeyForKey(
    const SidePanelEntry::Key& entry_key) const {
  if (GetActiveContextualRegistry() &&
      GetActiveContextualRegistry()->GetEntryForKey(entry_key)) {
    return UniqueKey{
        browser_view_->browser()->GetActiveTabInterface()->GetTabHandle(),
        entry_key};
  }

  if (window_registry_->GetEntryForKey(entry_key)) {
    return UniqueKey{/*tab_handle=*/std::nullopt, entry_key};
  }
  return std::nullopt;
}

SidePanelEntry* SidePanelCoordinator::GetActiveContextualEntryForKey(
    const SidePanelEntry::Key& entry_key) const {
  return GetActiveContextualRegistry()
             ? GetActiveContextualRegistry()->GetEntryForKey(entry_key)
             : nullptr;
}

void SidePanelCoordinator::PopulateSidePanel(
    bool suppress_animations,
    const UniqueKey& unique_key,
    SidePanelEntry* entry,
    std::optional<std::unique_ptr<views::View>> content_view) {
  actions::ActionItem* const action_item = GetActionItem(entry->key());
  UpdatePanelIconAndTitle(
      action_item->GetImage(), action_item->GetText(),
      entry->GetProperty(kShouldShowTitleInSidePanelHeaderKey),
      (entry->key().id() == SidePanelEntryId::kExtension));

  auto* content_wrapper =
      browser_view_->unified_side_panel()->GetContentParentView();
  DCHECK(content_wrapper);
  // |content_wrapper| should have either no child views or one child view for
  // the currently hosted SidePanelEntry.
  DCHECK(content_wrapper->children().size() <= 1);

  const bool opening_side_panel = !IsSidePanelShowing();

  content_wrapper->SetVisible(true);
  browser_view_->unified_side_panel()->Open(/*animated=*/!suppress_animations);

  SidePanelEntry* previous_entry = current_entry_.get();

  if (content_wrapper->children().size()) {
    if (previous_entry) {
      previous_entry->OnEntryWillHide(SidePanelEntryHideReason::kReplaced);
      auto previous_entry_view = content_wrapper->RemoveChildViewT(
          content_wrapper->children().front());
      previous_entry->CacheView(std::move(previous_entry_view));
    } else {
      // It is possible for |previous_entry| to no longer exist but for the
      // child view to still be hosted if the tab is removed from the tab strip
      // and the side panel remains open because the next active tab has an
      // active side panel entry. Make sure the remove the child view here.
      content_wrapper->RemoveChildViewT(content_wrapper->children().front());
    }
  }
  auto* content = content_wrapper->AddChildView(
      content_view.has_value() ? std::move(content_view.value())
                               : entry->GetContent());
  if (auto* contextual_registry = GetActiveContextualRegistry())
    contextual_registry->ResetActiveEntry();
  current_key_ = unique_key;
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
  header_more_info_button_->SetVisible(entry->SupportsMoreInfoButton());

  if (base::FeatureList::IsEnabled(features::kSidePanelResizing)) {
    browser_view_->unified_side_panel()->UpdateWidthOnEntryChanged();
  }

  // Notify the observers when the side panel is opened (made visible). However,
  // the observers are not renotified when the side panel entry changes.
  if (opening_side_panel) {
    view_state_observers_.Notify(
        &SidePanelViewStateObserver::OnSidePanelDidOpen);
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

  header_more_info_button_ = header->AddChildView(CreateControlButton(
      header.get(),
      // Callback will not be used since a button controller is being set.
      base::RepeatingClosure(), kHelpMenuIcon,
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_HEADER_MORE_INFO_BUTTON_TOOLTIP),
      kSidePanelMoreInfoButtonElementId,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE)));
  header_more_info_button_->SetFocusBehavior(
      views::View::FocusBehavior::ALWAYS);
  // The icon is later set as visible for side panels that support it.
  header_more_info_button_->SetVisible(false);
  // A menu button controller is used so that the button remains pressed while
  // the menu is open.
  header_more_info_button_->SetButtonController(
      std::make_unique<views::MenuButtonController>(
          header_more_info_button_,
          base::BindRepeating(&SidePanelCoordinator::OpenMoreInfoMenu,
                              base::Unretained(this)),
          std::make_unique<views::Button::DefaultButtonControllerDelegate>(
              header_more_info_button_)));

  auto* header_close_button = header->AddChildView(CreateControlButton(
      header.get(),
      base::BindRepeating(&SidePanelUI::Close, base::Unretained(this)),
      views::kIcCloseIcon,
      l10n_util::GetStringUTF16(IDS_ACCNAME_SIDE_PANEL_CLOSE),
      kSidePanelCloseButtonElementId,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE)));
  header_close_button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  return header;
}

std::optional<SidePanelCoordinator::UniqueKey>
SidePanelCoordinator::GetNewActiveKeyOnDeregister(
    SidePanelRegistry* deregistering_registry,
    const SidePanelEntry::Key& key) {
  // This function should only be called when the side panel view is shown.
  DCHECK(IsSidePanelShowing());

  // Attempt to return an entry in the following fallback order: global entry
  // for `key` if a contextual entry is deregistered > active global entry >
  // null.
  if (deregistering_registry == GetActiveContextualRegistry() &&
      window_registry_->GetEntryForKey(key)) {
    return UniqueKey{/*tab_handle=*/std::nullopt, key};
  }

  if (window_registry_->active_entry()) {
    return UniqueKey{/*tab_handle=*/std::nullopt,
                     (*window_registry_->active_entry())->key()};
  }
  return std::nullopt;
}

std::optional<SidePanelCoordinator::UniqueKey>
SidePanelCoordinator::GetNewActiveKeyOnTabChanged() {
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
    return UniqueKey{
        browser_view_->browser()->GetActiveTabInterface()->GetTabHandle(),
        (*active_contextual_registry->active_entry())->key()};
  }

  if (current_key_ && window_registry_->GetEntryForKey(current_key_->key)) {
    return GetUniqueKeyForKey(current_key_->key);
  }

  if (window_registry_->active_entry()) {
    return GetUniqueKeyForKey((*window_registry_->active_entry())->key());
  }

  return std::nullopt;
}

void SidePanelCoordinator::NotifyPinnedContainerOfActiveStateChange(
    SidePanelEntryKey key,
    bool is_active) {
  auto* toolbar_container =
      browser_view_->toolbar()->pinned_toolbar_actions_container();
  CHECK(toolbar_container);

  // Active extension side-panels have different UI in the toolbar than active
  // built-in side-panels.
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
      (current_key_->key.id() == SidePanelEntryId::kLensOverlayResults)
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
    browser_view_->NotifyFeaturePromoFeatureUsed(
        *pending_pin_promo_,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
    if (pending_pin_promo_ ==
        &feature_engagement::kIPHSidePanelLensOverlayPinnableFeature) {
      browser_view_->MaybeShowFeaturePromo(
          feature_engagement::kIPHSidePanelLensOverlayPinnableFollowupFeature);
    }
  } else {
    browser_view_->AbortFeaturePromo(*pending_pin_promo_);
  }

  pin_promo_timer_.Stop();
  pending_pin_promo_ = nullptr;
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
  if (!browser_view_->unified_side_panel()->IsClosing() && current_key_ &&
      (current_key_->key == entry->key())) {
    // If a global entry is deregistered but we are currently showing a
    // tab-scoped key, then do nothing.
    if (registry == window_registry_.get() && current_key_->tab_handle) {
      entry->CacheView(std::move(entry_view));
      return;
    }

    // Fetch the entry's view from the side panel container if it is shown.
    auto* content_wrapper =
        browser_view_->unified_side_panel()->GetContentParentView();
    if (content_wrapper->children().size() == 1) {
      entry_view = content_wrapper->RemoveChildViewT(
          content_wrapper->children().front());
      // TODO(crbug.com/40897366): Log the time elapsed between when this view
      // is removed, to when the new active entry's view is shown. This can
      // determine if the user will notice a flash in the side panel in between
      // different entries being shown.
    }

    // If there is going to be any change to UI, it must be done synchronously
    // to avoid state referring to a deregistered SidePanelEntry. Both of these
    // control flows will result in a synchronous re-entrancy into
    // OnViewVisibilityChanged.
    if (std::optional<UniqueKey> active_entry =
            GetNewActiveKeyOnDeregister(registry, entry->key())) {
      Show(active_entry.value(),
           SidePanelUtil::SidePanelOpenTrigger::kSidePanelEntryDeregistered,
           /*suppress_animations=*/true);
    } else {
      Close(/*suppress_animations=*/true);
    }
  }

  // Cache the deregistering entry's view. This needs to be done after Close()
  // might be called because Close() clears all cached views.
  entry->CacheView(std::move(entry_view));
}

void SidePanelCoordinator::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // If the browser window is closing, do nothing.
  if (tab_strip_model->closing_all()) {
    return;
  }

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
  }

  // Add the current tab's contextual registry.
  SidePanelRegistry* new_contextual_registry = nullptr;
  if (selection.new_contents) {
    new_contextual_registry =
        SidePanelRegistry::GetDeprecated(selection.new_contents);
  }

  // Show an entry in the following fallback order: new contextual registry's
  // active entry > active global entry > none (close the side panel).
  if (IsSidePanelShowing() &&
      !browser_view_->unified_side_panel()->IsClosing()) {
    // Attempt to find a suitable entry to be shown after the tab switch and if
    // one is found, show it.
    if (std::optional<UniqueKey> unique_key = GetNewActiveKeyOnTabChanged()) {
      Show(unique_key.value(), SidePanelUtil::SidePanelOpenTrigger::kTabChanged,
           /*suppress_animations=*/true);
    } else {
      // If there is no suitable entry to be shown after the tab switch, cache
      // the view of the old contextual registry (if it was active), and close
      // the side panel.
      if (old_contextual_registry && old_contextual_registry->active_entry() &&
          current_key_ &&
          (*old_contextual_registry->active_entry())->key() ==
              current_key_->key &&
          current_key_->tab_handle) {
        auto* content_wrapper =
            browser_view_->unified_side_panel()->GetContentParentView();
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
    Show({browser_view_->browser()->GetActiveTabInterface()->GetTabHandle(),
          (*new_contextual_registry->active_entry())->key()},
         SidePanelUtil::SidePanelOpenTrigger::kTabChanged,
         /*suppress_animations=*/true);
  }
}

void SidePanelCoordinator::UpdateNewTabButtonState() {
  if (header_open_in_new_tab_button_ && current_key_) {
    SidePanelEntry* current_entry = GetEntryForUniqueKey(*current_key_);
    bool has_open_in_new_tab_button =
        current_entry->SupportsNewTabButton() &&
        current_entry->GetOpenInNewTabURL().is_valid();
    header_open_in_new_tab_button_->SetVisible(has_open_in_new_tab_button);
  }
}

void SidePanelCoordinator::UpdateHeaderPinButtonState() {
  if (!current_key_) {
    return;
  }

  Profile* const profile = browser_view_->GetProfile();
  actions::ActionItem* const action_item = GetActionItem(current_key_->key);
  std::optional<actions::ActionId> action_id = action_item->GetActionId();
  CHECK(action_id.has_value());

  bool current_pinned_state = false;

  // TODO(b/310910098): Clean condition up once/if ToolbarActionModel and
  // PinnedToolbarActionModel are merged together.
  if (const std::optional<extensions::ExtensionId> extension_id =
          current_key_->key.extension_id();
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

SidePanelEntry* SidePanelCoordinator::GetCurrentSidePanelEntryForTesting() {
  return GetEntryForUniqueKey(*current_key_);
}

void SidePanelCoordinator::SetNoDelaysForTesting(bool no_delays_for_testing) {
  waiter_->SetNoDelaysForTesting(no_delays_for_testing);  // IN-TEST
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
  // This method is called in 3 situations:
  //   (1) The SidePanel was previously invisible, and Show() is called. This is
  //   independent of the /*suppress_animations*/ parameter, and is re-entrant.
  //   (2) The SidePanel was previously visible and has finished becoming
  //   invisible. This is asynchronous if animated, and re-entrant if
  //   non-animated.
  //   (3) A parent view or widget changes its visibility state (e.g. window
  //   becomes visible).
  //   We currently only take action on (2). We use `current_key_` to
  //   distinguish (3) from (2). We use visibility to distinguish (1) from (2).
  if (observed_view->GetVisible() || !current_key_) {
    return;
  }

  bool closing_global = !current_key_->tab_handle;

  // Reset current_key_ first to prevent previous_entry->OnEntryHidden()
  // from calling multiple times. This could happen in the edge cases when
  // callback inside current_entry->OnEntryHidden() is calling Close() to
  // trigger race condition.
  SidePanelEntry* previous_entry = current_entry_.get();
  current_key_.reset();
  current_entry_.reset();
  if (previous_entry) {
    previous_entry->OnEntryHidden();
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
  auto* content_wrapper =
      browser_view_->unified_side_panel()->GetContentParentView();
  if (!content_wrapper->children().empty()) {
    content_wrapper->RemoveChildViewT(content_wrapper->children().front());
  }
  SidePanelUtil::RecordSidePanelClosed(opened_timestamp_);

  view_state_observers_.Notify(
      &SidePanelViewStateObserver::OnSidePanelDidClose);
}

void SidePanelCoordinator::OnActionsChanged() {
  if (current_key_) {
    UpdateHeaderPinButtonState();
  }
}

SidePanelEntry* SidePanelCoordinator::GetEntryForUniqueKey(
    const UniqueKey& unique_key) const {
  SidePanelEntry* entry = nullptr;
  if (unique_key.tab_handle) {
    entry = GetActiveContextualEntryForKey(unique_key.key);
  } else {
    entry = window_registry_->GetEntryForKey(unique_key.key);
  }
  return entry;
}

void SidePanelCoordinator::ClosePromoAndMaybeNotifyUsed(
    const base::Feature& promo_feature,
    SidePanelEntryId promo_id,
    SidePanelEntryId actual_id) {
  if (promo_id == actual_id) {
    browser_view_->NotifyFeaturePromoFeatureUsed(
        promo_feature, FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  } else {
    browser_view_->AbortFeaturePromo(promo_feature);
  }
}
