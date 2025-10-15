// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"

#include <memory>
#include <string>
#include <string_view>
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
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_waiter.h"
#include "chrome/browser/ui/views/side_panel/side_panel_header.h"
#include "chrome/browser/ui/views/side_panel/side_panel_toolbar_pinning_controller.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/lens/lens_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

SidePanelCoordinator::SidePanelCoordinator(BrowserView* browser_view)
    : SidePanelUIBase(browser_view->browser()), browser_view_(browser_view) {
  side_panel_toolbar_pinning_controller_ =
      std::make_unique<SidePanelToolbarPinningController>(browser_view_);
  side_panel_toolbar_pinning_controller_observation_.Observe(
      side_panel_toolbar_pinning_controller_.get());

  auto side_panel_header = std::make_unique<SidePanelHeader>(
      base::BindRepeating(&SidePanelCoordinator::UpdatePinState,
                          base::Unretained(this)),
      base::BindRepeating(&SidePanelCoordinator::OpenInNewTab,
                          base::Unretained(this)),
      base::BindRepeating(&SidePanelCoordinator::OpenMoreInfoMenu,
                          base::Unretained(this)),
      base::BindRepeating(&SidePanelUI::Close, base::Unretained(this)));
  browser_view_->contents_height_side_panel()->AddHeaderView(
      std::move(side_panel_header));
}

SidePanelCoordinator::~SidePanelCoordinator() = default;

void SidePanelCoordinator::Init(Browser* browser) {
  SidePanelUtil::PopulateGlobalEntries(browser, window_registry_.get());
}

void SidePanelCoordinator::TearDownPreBrowserWindowDestruction() {
  side_panel_toolbar_pinning_controller_observation_.Reset();
  side_panel_toolbar_pinning_controller_.reset();
}

void SidePanelCoordinator::OnPinStateChanged() {
  if (!current_key()) {
    return;
  }
  if (SidePanelEntry* entry = GetEntryForUniqueKey(*current_key())) {
    UpdateSidePanelHeader(entry);
  }
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
      !browser_view_->contents_height_side_panel()->IsClosing()) {
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
  if (!current_key()) {
    return;
  }

  GURL new_tab_url = GetEntryForUniqueKey(*current_key())->GetOpenInNewTabURL();
  if (!new_tab_url.is_valid()) {
    return;
  }

  SidePanelUtil::RecordNewTabButtonClicked(current_key()->key.id());
  content::OpenURLParams params(new_tab_url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                /*is_renderer_initiated=*/false);
  browser_view_->browser()->OpenURL(params, /*navigation_handle_callback=*/{});
  Close();
}

void SidePanelCoordinator::UpdatePinState() {
  if (current_key()) {
    side_panel_toolbar_pinning_controller_->UpdatePinState(current_key()->key);
    browser_view_->contents_height_side_panel()
        ->GetHeaderView<SidePanelHeader>()
        ->header_pin_button()
        ->GetViewAccessibility()
        .AnnounceText(l10n_util::GetStringUTF16(
            side_panel_toolbar_pinning_controller_->GetPinnedStateFor(
                current_key()->key)
                ? IDS_SIDE_PANEL_PINNED
                : IDS_SIDE_PANEL_UNPINNED));
    // Close/cancel IPH for side panel pinning, if shown.
    MaybeEndPinPromo(/*pinned=*/true);
  }
}

void SidePanelCoordinator::OpenMoreInfoMenu() {
  more_info_menu_model_ =
      GetEntryForUniqueKey(*current_key())->GetMoreInfoMenuModel();
  CHECK(more_info_menu_model_);
  views::ImageButton* const header_more_info_button =
      browser_view_->contents_height_side_panel()
          ->GetHeaderView<SidePanelHeader>()
          ->header_more_info_button();
  menu_runner_ = std::make_unique<views::MenuRunner>(
      more_info_menu_model_.get(), views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(header_more_info_button->GetWidget(),
                          static_cast<views::MenuButtonController*>(
                              header_more_info_button->button_controller()),
                          header_more_info_button->GetAnchorBoundsInScreen(),
                          views::MenuAnchorPosition::kTopRight,
                          ui::mojom::MenuSourceType::kNone);
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
  browser_view_->contents_height_side_panel()
      ->DisableAnimationsForTesting();  // IN-TEST
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

  // If the side panel is already showing, cancel all loads and do nothing.
  if (current_key() && *current_key() == input) {
    waiter_->ResetLoadingEntryIfNecessary();

    // If the side panel is in the process of closing, show it instead.
    if (browser_view_->contents_height_side_panel()->state() ==
        SidePanel::State::kClosing) {
      browser_view_->contents_height_side_panel()->Open(/*animated=*/true);
      side_panel_toolbar_pinning_controller_->UpdateActiveState(
          entry->key(), entry->should_show_ephemerally_in_toolbar());
    }
    return;
  }

  SidePanelUtil::RecordEntryShowTriggeredMetrics(
      browser_view_->browser(), entry->key().id(), open_trigger);

  waiter_->WaitForEntry(
      entry, base::BindOnce(&SidePanelCoordinator::PopulateSidePanel,
                            base::Unretained(this), suppress_animations, input,
                            open_trigger));
}

base::CallbackListSubscription SidePanelCoordinator::RegisterSidePanelShown(
    ShownCallback callback) {
  return shown_callback_list_.Add(std::move(callback));
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
      browser_view_->contents_height_side_panel()->IsClosing()) {
    return;
  }

  if (current_key()) {
    if (browser_view_->toolbar()->pinned_toolbar_actions_container()) {
      side_panel_toolbar_pinning_controller_->UpdateActiveState(
          current_key()->key, false);
    }
    SidePanelEntry* entry = GetEntryForUniqueKey(*current_key());
    if (entry) {
      entry->OnEntryWillHide(SidePanelEntryHideReason::kSidePanelClosed);
    }
  }
  browser_view_->contents_height_side_panel()->Close(
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

void SidePanelCoordinator::UpdateSidePanelHeader(SidePanelEntry* entry) {
  SidePanel* side_panel = browser_view_->contents_height_side_panel();
  auto* side_panel_header = side_panel->GetHeaderView<SidePanelHeader>();

  if (!side_panel_header || !entry->should_show_header()) {
    side_panel->SetHeaderVisibility(false);
    return;
  }

  side_panel->SetHeaderVisibility(true);

  actions::ActionItem* const action_item =
      SidePanelUtil::GetActionItem(browser_view_->browser(), entry->key());
  std::u16string_view title_text =
      entry->GetProperty(kShouldShowTitleInSidePanelHeaderKey)
          ? action_item->GetText()
          : std::u16string_view();
  side_panel_header->panel_title()->SetText(title_text);

  side_panel_header->panel_icon()->SetVisible(entry->key().id() ==
                                              SidePanelEntryId::kExtension);
  if (side_panel_header->panel_icon()->GetVisible()) {
    ui::ImageModel icon = action_item->GetImage();
    if (icon.IsVectorIcon()) {
      icon = ui::ImageModel::FromVectorIcon(*icon.GetVectorIcon().vector_icon(),
                                            kColorSidePanelEntryIcon,
                                            icon.GetVectorIcon().icon_size());
    }
    side_panel_header->panel_icon()->SetImage(icon);
  }

  side_panel_header->header_open_in_new_tab_button()->SetVisible(
      entry->SupportsNewTabButton() && entry->GetOpenInNewTabURL().is_valid());

  Profile* const profile = browser_view_->GetProfile();
  const bool current_pinned_state =
      side_panel_toolbar_pinning_controller_->GetPinnedStateFor(entry->key());
  side_panel_header->header_pin_button()->SetToggled(current_pinned_state);
  side_panel_header->header_pin_button()->SetVisible(
      !profile->IsIncognitoProfile() && !profile->IsGuestSession() &&
      action_item->GetProperty(actions::kActionItemPinnableKey) ==
          static_cast<int>(actions::ActionPinnableState::kPinnable));

  if (!current_pinned_state) {
    // Show IPH for side panel pinning icon.
    MaybeQueuePinPromo(entry->key().id());
  }

  side_panel_header->header_more_info_button()->SetVisible(
      entry->SupportsMoreInfoButton());
}

void SidePanelCoordinator::PopulateSidePanel(
    bool suppress_animations,
    const UniqueKey& unique_key,
    std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
    SidePanelEntry* entry,
    std::optional<std::unique_ptr<views::View>> content_view) {
  SidePanel* side_panel = browser_view_->contents_height_side_panel();

  entry->set_last_open_trigger(open_trigger);
  actions::ActionItem* const action_item =
      SidePanelUtil::GetActionItem(browser_view_->browser(), entry->key());
  action_item_controller_subscription_ = action_item->AddActionChangedCallback(
      base::BindRepeating(&SidePanelCoordinator::OnActionItemChanged,
                          base::Unretained(this), unique_key));

  UpdateSidePanelHeader(entry);
  side_panel->SetOutlineVisibility(entry->should_show_outline());

  auto* content_wrapper = side_panel->GetContentParentView();
  DCHECK(content_wrapper);
  // |content_wrapper| should have either no child views or one child view for
  // the currently hosted SidePanelEntry.
  DCHECK(content_wrapper->children().size() <= 1);

  content_wrapper->SetVisible(true);
  side_panel->Open(/*animated=*/!suppress_animations);

  SidePanelEntry* previous_entry =
      current_key() ? GetEntryForUniqueKey(*current_key()) : nullptr;

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
  if (auto* contextual_registry = GetActiveContextualRegistry()) {
    contextual_registry->ResetActiveEntryFor(
        SidePanelEntry::PanelType::kContent);
  }
  set_current_key(unique_key);
  if (browser_view_->toolbar()->pinned_toolbar_actions_container()) {
    side_panel_toolbar_pinning_controller_->UpdateActiveState(
        entry->key(), entry->should_show_ephemerally_in_toolbar());
    // Notify active state change only if the entry ids for the side panel are
    // different. This is to ensure extensions container isn't notified if we
    // switch between different extensions side panels or between global to
    // contextual side panel of the same extension.
    if (previous_entry && previous_entry->key().id() != entry->key().id()) {
      side_panel_toolbar_pinning_controller_->UpdateActiveState(
          previous_entry->key(), false);
    }
  }
  entry->OnEntryShown();
  if (previous_entry) {
    previous_entry->OnEntryHidden();
  } else {
    content->RequestFocus();
  }

  side_panel->UpdateWidthOnEntryChanged();

  shown_callback_list_.Notify();
}

void SidePanelCoordinator::ClearCachedEntryViews(
    SidePanelEntry::PanelType type) {
  window_registry_->ClearCachedEntryViews(type);
  TabStripModel* model = browser_view_->browser()->tab_strip_model();
  for (int index = 0; index < model->count(); ++index) {
    auto* tab =
        browser_view_->browser()->tab_strip_model()->GetTabAtIndex(index);
    tab->GetTabFeatures()->side_panel_registry()->ClearCachedEntryViews(type);
  }
}

void SidePanelCoordinator::MaybeQueuePinPromo(SidePanelEntryId id) {
  // Which feature is shown depends on the specific side panel that is showing.
  const base::Feature* const iph_feature =
      (id == SidePanelEntryId::kLensOverlayResults)
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
  if (iph_feature &&
      !BrowserUserEducationInterface::From(browser_view_->browser())
           ->CanShowFeaturePromo(*iph_feature)
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

  BrowserUserEducationInterface::From(browser_view_->browser())
      ->MaybeShowFeaturePromo(*pending_pin_promo_);
}

void SidePanelCoordinator::MaybeEndPinPromo(bool pinned) {
  if (!pending_pin_promo_) {
    return;
  }

  auto* const user_education =
      BrowserUserEducationInterface::From(browser_view_->browser());
  if (pinned) {
    user_education->NotifyFeaturePromoFeatureUsed(
        *pending_pin_promo_,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
    if (pending_pin_promo_ ==
        &feature_engagement::kIPHSidePanelLensOverlayPinnableFeature) {
      user_education->MaybeShowFeaturePromo(
          feature_engagement::kIPHSidePanelLensOverlayPinnableFollowupFeature);
    }
  } else {
    user_education->AbortFeaturePromo(*pending_pin_promo_);
  }

  pin_promo_timer_.Stop();
  pending_pin_promo_ = nullptr;
}

void SidePanelCoordinator::MaybeShowEntryOnTabStripModelChanged(
    SidePanelRegistry* old_contextual_registry,
    SidePanelRegistry* new_contextual_registry) {
  // Show an entry in the following fallback order: new contextual registry's
  // active entry > active global entry > none (close the side panel).
  if (IsSidePanelShowing() &&
      !browser_view_->contents_height_side_panel()->IsClosing()) {
    // Attempt to find a suitable entry to be shown after the tab switch and if
    // one is found, show it.
    if (std::optional<UniqueKey> unique_key = GetNewActiveKeyOnTabChanged()) {
      Show(unique_key.value(), SidePanelUtil::SidePanelOpenTrigger::kTabChanged,
           /*suppress_animations=*/true);
    } else {
      // If there is no suitable entry to be shown after the tab switch, cache
      // the view of the old contextual registry (if it was active), and close
      // the side panel.
      if (auto active_entry = old_contextual_registry
                                  ? old_contextual_registry->GetActiveEntryFor(
                                        SidePanelEntry::PanelType::kContent)
                                  : std::nullopt;
          active_entry.has_value() && current_key() &&
          current_key()->tab_handle &&
          (*active_entry)->key() == current_key()->key) {
        auto* content_wrapper =
            browser_view_->contents_height_side_panel()->GetContentParentView();
        DCHECK(content_wrapper->children().size() == 1);
        auto current_entry_view = content_wrapper->RemoveChildViewT(
            content_wrapper->children().front());
        (*active_entry)->CacheView(std::move(current_entry_view));
      }
      Close(/*suppress_animations=*/true);
    }
  } else if (auto active_entry =
                 new_contextual_registry
                     ? new_contextual_registry->GetActiveEntryFor(
                           SidePanelEntry::PanelType::kContent)
                     : std::nullopt;
             active_entry.has_value()) {
    Show({browser_view_->browser()->GetActiveTabInterface()->GetHandle(),
          (*active_entry)->key()},
         SidePanelUtil::SidePanelOpenTrigger::kTabChanged,
         /*suppress_animations=*/true);
  }
}

SidePanelEntry* SidePanelCoordinator::GetCurrentSidePanelEntryForTesting() {
  return GetEntryForUniqueKey(*current_key());
}

void SidePanelCoordinator::SetNoDelaysForTesting(bool no_delays_for_testing) {
  waiter_->SetNoDelaysForTesting(no_delays_for_testing);  // IN-TEST
}

void SidePanelCoordinator::OnActionItemChanged(const UniqueKey key) {
  if (key != current_key()) {
    return;
  }
  SidePanelEntry* entry = GetEntryForUniqueKey(key);
  if (!entry) {
    return;
  }
  UpdateSidePanelHeader(entry);
}

void SidePanelCoordinator::OnViewVisibilityChanged(views::View* observed_view,
                                                   views::View* starting_from,
                                                   bool visible) {
  SidePanel* side_panel = views::AsViewClass<SidePanel>(observed_view);
  CHECK(side_panel);

  SidePanelEntry::PanelType type =
      side_panel == browser_view_->contents_height_side_panel()
          ? SidePanelEntry::PanelType::kContent
          : SidePanelEntry::PanelType::kToolbar;

  // This method is called in 3 situations:
  //   (1) The SidePanel was previously invisible, and Show() is called. This is
  //   independent of the /*suppress_animations*/ parameter, and is re-entrant.
  //   (2) The SidePanel was previously visible and has finished becoming
  //   invisible. This is asynchronous if animated, and re-entrant if
  //   non-animated.
  //   (3) A parent view or widget changes its visibility state (e.g. window
  //   becomes visible).
  //   We currently only take action on (2). We use `current_key()` to
  //   distinguish (3) from (2). We use visibility to distinguish (1) from (2).
  if (observed_view->GetVisible() || !current_key()) {
    return;
  }

  // Reset current_key() first to prevent previous_entry->OnEntryHidden()
  // from calling multiple times. This could happen in the edge cases when
  // callback inside current_entry->OnEntryHidden() is calling Close() to
  // trigger race condition.
  SidePanelEntry* previous_entry = GetEntryForUniqueKey(*current_key());
  set_current_key(std::nullopt);
  if (previous_entry) {
    previous_entry->OnEntryHidden();
  }

  // Reset active entry values for all observed registries and clear cache for
  // everything except remaining active entries (i.e. if another tab has an
  // active contextual entry).
  if (auto* contextual_registry = GetActiveContextualRegistry()) {
    contextual_registry->ResetActiveEntryFor(type);
  }
  window_registry_->ResetActiveEntryFor(type);
  ClearCachedEntryViews(type);

  // `OnEntryWillDeregister` (triggered by calling `OnEntryHidden`) may
  // already have deleted the content container, so check that it still
  // exists.
  auto* content_wrapper = side_panel->GetContentParentView();
  if (!content_wrapper->children().empty()) {
    content_wrapper->RemoveChildViewT(content_wrapper->children().front());
  }
  SidePanelUtil::RecordSidePanelClosed(opened_timestamp_);
}

void SidePanelCoordinator::ClosePromoAndMaybeNotifyUsed(
    const base::Feature& promo_feature,
    SidePanelEntryId promo_id,
    SidePanelEntryId actual_id) {
  auto* const user_education =
      BrowserUserEducationInterface::From(browser_view_->browser());
  if (promo_id == actual_id) {
    user_education->NotifyFeaturePromoFeatureUsed(
        promo_feature, FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  } else {
    user_education->AbortFeaturePromo(promo_feature);
  }
}
