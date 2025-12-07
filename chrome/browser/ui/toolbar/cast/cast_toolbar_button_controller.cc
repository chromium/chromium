// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/cast/cast_toolbar_button_controller.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/media_router/cast_browser_controller.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/media_router/common/pref_names.h"
#include "components/media_router/common/providers/cast/cast_media_source.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/actions/actions.h"

CastToolbarButtonController::CastToolbarButtonController(Profile* profile)
    : CastToolbarButtonController(
          profile,
          media_router::MediaRouterFactory::GetApiForBrowserContext(profile)) {}

CastToolbarButtonController::~CastToolbarButtonController() = default;

// static
bool CastToolbarButtonController::IsActionShownByPolicy(Profile* profile) {
  CHECK(profile);
  const PrefService::Preference* pref =
      profile->GetPrefs()->FindPreference(prefs::kShowCastIconInToolbar);
  bool show = false;
  if (pref->IsManaged() && pref->GetValue()->is_bool()) {
    show = pref->GetValue()->GetBool();
  }
  return show;
}

// static
bool CastToolbarButtonController::GetAlwaysShowActionPref(Profile* profile) {
  CHECK(profile);
  return profile->GetPrefs()->GetBoolean(prefs::kShowCastIconInToolbar);
}

// static
void CastToolbarButtonController::SetAlwaysShowActionPref(Profile* profile,
                                                          bool always_show) {
  CHECK(profile);
  profile->GetPrefs()->SetBoolean(prefs::kShowCastIconInToolbar, always_show);
}

void CastToolbarButtonController::OnIssue(const media_router::Issue& issue) {
  // Don't show the toolbar button if it receives a permission rejected issue.
  has_issue_ = !issue.is_permission_rejected_issue();
  MaybeToggleIconVisibility();
}

void CastToolbarButtonController::OnIssuesCleared() {
  has_issue_ = false;
  MaybeToggleIconVisibility();
}

void CastToolbarButtonController::OnRoutesUpdated(
    const std::vector<media_router::MediaRoute>& routes) {
  has_local_display_route_ =
      std::ranges::any_of(routes, [](const media_router::MediaRoute& route) {
        // The Cast icon should be hidden if there only are
        // non-local and non-display routes.
        if (!route.is_local()) {
          return false;
        }
        // When the feature is enabled, presentation routes are
        // controlled through the global media controls most of the
        // time. So we do not request to show the Cast icon when
        // there only are presentation routes.
        // In other words, the Cast icon is shown when there are
        // mirroring or local file sources.
        return route.media_source().IsTabMirroringSource() ||
               route.media_source().IsDesktopMirroringSource();
      });
  MaybeToggleIconVisibility();
}

void CastToolbarButtonController::OnDialogShown() {
  dialog_count_++;
  MaybeToggleIconVisibility();
}

void CastToolbarButtonController::OnDialogHidden() {
  DCHECK_GT(dialog_count_, 0u);
  if (dialog_count_) {
    dialog_count_--;
  }
  if (dialog_count_ == 0) {
    // Call MaybeToggleIconVisibility() asynchronously, so that the action icon
    // doesn't get hidden until we have a chance to show a context menu.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&CastToolbarButtonController::MaybeToggleIconVisibility,
                       weak_factory_.GetWeakPtr()));
  }
}

void CastToolbarButtonController::UpdateIcon() {
  for (Browser* browser : chrome::FindAllBrowsersWithProfile(profile_)) {
    browser->browser_window_features()->cast_browser_controller()->UpdateIcon();
  }
}

void CastToolbarButtonController::KeepIconShownOnPressed() {
  DCHECK(!keep_visible_for_right_click_or_hold_);
  keep_visible_for_right_click_or_hold_ = true;
  MaybeToggleIconVisibility();
}

void CastToolbarButtonController::MaybeHideIconOnReleased() {
  keep_visible_for_right_click_or_hold_ = false;
  MaybeToggleIconVisibility();
}

bool CastToolbarButtonController::ShouldEnableAction() const {
  return shown_by_policy_ || has_local_display_route_ || has_issue_ ||
         dialog_count_ || context_menu_shown_ ||
         keep_visible_for_right_click_or_hold_ ||
         GetAlwaysShowActionPref(profile_);
}

CastToolbarButtonController::CastToolbarButtonController(
    Profile* profile,
    media_router::MediaRouter* router)
    : media_router::IssuesObserver(router->GetIssueManager()),
      media_router::MediaRoutesObserver(router),
      profile_(profile),
      shown_by_policy_(
          CastToolbarButtonController::IsActionShownByPolicy(profile)) {
  CHECK(profile_);
  media_router::IssuesObserver::Init();
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kShowCastIconInToolbar,
      base::BindRepeating(
          &CastToolbarButtonController::MaybeToggleIconVisibility,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      media_router::prefs::kMediaRouterMediaRemotingEnabled,
      base::BindRepeating(
          &CastToolbarButtonController::UpdateToggleMediaRouterRemotingAction,
          base::Unretained(this)));
}

void CastToolbarButtonController::MaybeToggleIconVisibility() {
  bool shown_by_policy = IsActionShownByPolicy(profile_);
  // Pin media router if it should be pinned based on enterprise policy.
  if (shown_by_policy) {
    PinnedToolbarActionsModel* const actions_model =
        PinnedToolbarActionsModel::Get(profile_);
    actions_model->UpdatePinnedState(kActionRouteMedia, true);
  }

  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [this, shown_by_policy](BrowserWindowInterface* browser) {
        if (browser->GetProfile() != profile_) {
          return true;
        }
        auto* action_item = actions::ActionManager::Get().FindAction(
            kActionRouteMedia, browser->GetActions()->root_action_item());
        // Update the action item's pinnable state based on the enterprise
        // policy.
        if (shown_by_policy) {
          action_item->SetProperty(
              actions::kActionItemPinnableKey,
              static_cast<std::underlying_type_t<actions::ActionPinnableState>>(
                  actions::ActionPinnableState::kEnterpriseControlled));
        } else {
          action_item->SetProperty(
              actions::kActionItemPinnableKey,
              static_cast<std::underlying_type_t<actions::ActionPinnableState>>(
                  actions::ActionPinnableState::kPinnable));
        }
        // Update the toolbar button's visibility.
        // WebUIBrowser does not have a BrowserView.
        // TODO(webium): make an pinned toolbar actions container for
        // WebUIBrowser.
        if (auto* controller =
                browser->GetFeatures().pinned_toolbar_actions_controller()) {
          controller->ShowActionEphemerallyInToolbar(kActionRouteMedia,
                                                     ShouldEnableAction());
        }
        return true;
      });
}

void CastToolbarButtonController::UpdateToggleMediaRouterRemotingAction() {
  bool checked = profile_->GetPrefs()->GetBoolean(
      media_router::prefs::kMediaRouterMediaRemotingEnabled);
  for (Browser* browser : chrome::FindAllBrowsersWithProfile(profile_)) {
    actions::ActionManager::Get()
        .FindAction(kActionMediaRouterToggleMediaRemoting,
                    browser->browser_actions()->root_action_item())
        ->SetChecked(checked);
  }
}
