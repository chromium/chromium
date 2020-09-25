// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/media_router_action_controller.h"

#include "base/bind.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/media_router/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

MediaRouterActionController::MediaRouterActionController(Profile* profile)
    : MediaRouterActionController(
          profile,
          media_router::MediaRouterFactory::GetApiForBrowserContext(profile)) {}

MediaRouterActionController::~MediaRouterActionController() {
  DCHECK_EQ(dialog_count_, 0u);
}

// static
bool MediaRouterActionController::IsActionShownByPolicy(Profile* profile) {
  CHECK(profile);
  const PrefService::Preference* pref =
      profile->GetPrefs()->FindPreference(prefs::kShowCastIconInToolbar);
  bool show = false;
  if (pref->IsManaged())
    pref->GetValue()->GetAsBoolean(&show);
  return show;
}

// static
bool MediaRouterActionController::GetAlwaysShowActionPref(Profile* profile) {
  CHECK(profile);
  return profile->GetPrefs()->GetBoolean(prefs::kShowCastIconInToolbar);
}

// static
void MediaRouterActionController::SetAlwaysShowActionPref(Profile* profile,
                                                          bool always_show) {
  CHECK(profile);
  profile->GetPrefs()->SetBoolean(prefs::kShowCastIconInToolbar, always_show);
}

void MediaRouterActionController::OnIssue(const media_router::Issue& issue) {
  has_issue_ = true;
  MaybeAddOrRemoveAction();
}

void MediaRouterActionController::OnIssuesCleared() {
  has_issue_ = false;
  MaybeAddOrRemoveAction();
}

void MediaRouterActionController::OnRoutesUpdated(
    const std::vector<media_router::MediaRoute>& routes,
    const std::vector<media_router::MediaRoute::Id>& joinable_route_ids) {
  has_local_display_route_ =
      std::find_if(routes.begin(), routes.end(),
                   [](const media_router::MediaRoute& route) {
                     return route.is_local() && route.for_display();
                   }) != routes.end();

  MaybeAddOrRemoveAction();
}

void MediaRouterActionController::OnDialogShown() {
  dialog_count_++;
  MaybeAddOrRemoveAction();
  for (Observer& observer : observers_)
    observer.ActivateIcon();
}

void MediaRouterActionController::OnDialogHidden() {
  DCHECK_GT(dialog_count_, 0u);
  if (dialog_count_)
    dialog_count_--;
  if (dialog_count_ == 0) {
    for (Observer& observer : observers_)
      observer.DeactivateIcon();
    // Call MaybeAddOrRemoveAction() asynchronously, so that the action icon
    // doesn't get hidden until we have a chance to show a context menu.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaRouterActionController::MaybeAddOrRemoveAction,
                       weak_factory_.GetWeakPtr()));
  }
}

void MediaRouterActionController::OnContextMenuShown() {
  DCHECK(!context_menu_shown_);
  context_menu_shown_ = true;
  // Once the context menu is shown, we no longer need to keep track of the
  // mouse or touch press.
  keep_visible_for_right_click_or_hold_ = false;
  MaybeAddOrRemoveAction();
}

void MediaRouterActionController::OnContextMenuHidden() {
  DCHECK(context_menu_shown_);
  context_menu_shown_ = false;
  MaybeAddOrRemoveAction();
}

void MediaRouterActionController::KeepIconShownOnPressed() {
  DCHECK(!keep_visible_for_right_click_or_hold_);
  keep_visible_for_right_click_or_hold_ = true;
  MaybeAddOrRemoveAction();
}

void MediaRouterActionController::MaybeHideIconOnReleased() {
  keep_visible_for_right_click_or_hold_ = false;
  MaybeAddOrRemoveAction();
}

void MediaRouterActionController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MediaRouterActionController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool MediaRouterActionController::ShouldEnableAction() const {
  return shown_by_policy_ || has_local_display_route_ || has_issue_ ||
         dialog_count_ || context_menu_shown_ ||
         keep_visible_for_right_click_or_hold_ ||
         GetAlwaysShowActionPref(profile_);
}

MediaRouterActionController::MediaRouterActionController(
    Profile* profile,
    media_router::MediaRouter* router)
    : media_router::IssuesObserver(router->GetIssueManager()),
      media_router::MediaRoutesObserver(router),
      profile_(profile),
      shown_by_policy_(
          MediaRouterActionController::IsActionShownByPolicy(profile)) {
  CHECK(profile_);
  media_router::IssuesObserver::Init();
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kShowCastIconInToolbar,
      base::Bind(&MediaRouterActionController::MaybeAddOrRemoveAction,
                 base::Unretained(this)));
  if (profile_->IsRegularProfile()) {
    media_router::MediaRouterMetrics::RecordIconStateAtInit(
        MediaRouterActionController::GetAlwaysShowActionPref(profile_));
    media_router::MediaRouterMetrics::RecordCloudPrefAtInit(
        profile_->GetPrefs()->GetBoolean(
            media_router::prefs::kMediaRouterEnableCloudServices));
  }
}

void MediaRouterActionController::MaybeAddOrRemoveAction() {
  if (ShouldEnableAction()) {
    for (Observer& observer : observers_)
      observer.ShowIcon();
  } else {
    for (Observer& observer : observers_)
      observer.HideIcon();
  }
}
