// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_waiter.h"
#include "chrome/browser/ui/views/side_panel/side_panel_header.h"
#include "chrome/browser/ui/views/side_panel/side_panel_header_controller.h"
#include "chrome/browser/ui/views/side_panel/side_panel_toolbar_pinning_controller.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "ui/views/view.h"

SidePanelCoordinator::SidePanelCoordinator(BrowserView* browser_view)
    : SidePanelUIBase(browser_view->browser()), browser_view_(browser_view) {
  side_panel_toolbar_pinning_controller_ =
      std::make_unique<SidePanelToolbarPinningController>(browser_view_);
}

SidePanelCoordinator::~SidePanelCoordinator() = default;

void SidePanelCoordinator::Init(Browser* browser) {
  SidePanelUtil::PopulateGlobalEntries(browser, window_registry_.get());
}

void SidePanelCoordinator::TearDownPreBrowserWindowDestruction() {
  Close(/*suppress_animations=*/true);
  side_panel_toolbar_pinning_controller_.reset();
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
    SidePanelEntry* entry = GetEntryForKey(key);
    if (entry && waiter(entry->type())->loading_entry() == entry) {
      waiter(entry->type())->ResetLoadingEntryIfNecessary();
      Close();
      return;
    }
  }

  std::optional<UniqueKey> unique_key = GetUniqueKeyForKey(key);
  if (unique_key.has_value()) {
    Show(unique_key.value(), open_trigger, /*suppress_animations=*/false);
  }
}

void SidePanelCoordinator::ShowFrom(SidePanelEntryKey entry_key,
                                    gfx::Rect starting_bounds) {
  // TODO(crbug.com/445453126): Trigger an animation to show from the provided
  // starting_bounds.
  SidePanelUI::Show(entry_key);
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

SidePanelEntry* SidePanelCoordinator::GetLoadingEntryForTesting(
    SidePanelEntry::PanelType type) const {
  return waiter(type)->loading_entry();
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
    SetOpenedTimestamp(base::TimeTicks::Now());
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
    waiter(entry->type())->ResetLoadingEntryIfNecessary();

    // If the side panel is in the process of closing, show it instead.
    if (browser_view_->contents_height_side_panel()->state() ==
        SidePanel::State::kClosing) {
      browser_view_->contents_height_side_panel()->Open(/*animated=*/true);
      side_panel_toolbar_pinning_controller_->UpdateActiveState(
          entry->key(), entry->should_show_ephemerally_in_toolbar());
      entry->OnEntryHideCancelled();
    }
    return;
  }

  SidePanelUtil::RecordEntryShowTriggeredMetrics(
      browser_view_->browser(), entry->key().id(), open_trigger);

  waiter(entry->type())
      ->WaitForEntry(entry,
                     base::BindOnce(&SidePanelCoordinator::PopulateSidePanel,
                                    base::Unretained(this), suppress_animations,
                                    input, open_trigger));
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
      (!suppress_animations &&
       browser_view_->contents_height_side_panel()->IsClosing())) {
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
}

SidePanelEntry* SidePanelCoordinator::GetEntryForKey(
    const SidePanelEntry::Key& entry_key) const {
  if (auto* contextual_entry = GetActiveContextualEntryForKey(entry_key)) {
    return contextual_entry;
  }

  return window_registry_->GetEntryForKey(entry_key);
}

void SidePanelCoordinator::PopulateSidePanel(
    bool suppress_animations,
    const UniqueKey& unique_key,
    std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
    SidePanelEntry* entry,
    std::optional<std::unique_ptr<views::View>> content_view) {
  SidePanel* side_panel = browser_view_->contents_height_side_panel();

  entry->set_last_open_trigger(open_trigger);
  side_panel->SetOutlineVisibility(entry->should_show_outline());

  if (entry->should_show_header()) {
    side_panel->AddHeaderView(std::make_unique<SidePanelHeader>(
        std::make_unique<SidePanelHeaderController>(
            browser_view_->browser(),
            side_panel_toolbar_pinning_controller_.get(), entry)));
  } else {
    side_panel->RemoveHeaderView();
  }
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
  SetCurrentKey(unique_key);
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

  NotifyShownCallbacksFor(entry->type());
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
  waiter(SidePanelEntry::PanelType::kContent)
      ->SetNoDelaysForTesting(no_delays_for_testing);  // IN-TEST
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
  SetCurrentKey(std::nullopt);
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
  side_panel->RemoveHeaderView();
  SidePanelUtil::RecordSidePanelClosed(opened_timestamp());
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
