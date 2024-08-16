// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/cast/cast_toolbar_button_controller.h"

#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/media_router/common/pref_names.h"
#include "components/media_router/common/providers/cast/cast_media_source.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

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
  if (pref->IsManaged() && pref->GetValue()->is_bool())
    show = pref->GetValue()->GetBool();
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
  MaybeAddOrRemoveAction();
}

void CastToolbarButtonController::OnIssuesCleared() {
  has_issue_ = false;
  MaybeAddOrRemoveAction();
}

void CastToolbarButtonController::OnRoutesUpdated(
    const std::vector<media_router::MediaRoute>& routes) {
  has_local_display_route_ =
      base::ranges::any_of(
          routes, [this](const media_router::MediaRoute& route) {
            // The Cast icon should be hidden if there only are
            // non-local and non-display routes.
            if (!route.is_local()) {
              return false;
            }
            // When this feature is disabled, we show the Cast icon
            // regardless of the media source.
            if (!media_router::GlobalMediaControlsCastStartStopEnabled(
                    this->profile_)) {
              return true;
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
  MaybeAddOrRemoveAction();
}

void CastToolbarButtonController::OnDialogShown() {
  dialog_count_++;
  MaybeAddOrRemoveAction();
  for (Observer& observer : observers_)
    observer.ActivateIcon();
}

void CastToolbarButtonController::OnDialogHidden() {
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
        base::BindOnce(&CastToolbarButtonController::MaybeAddOrRemoveAction,
                       weak_factory_.GetWeakPtr()));
  }
}

void CastToolbarButtonController::OnContextMenuShown() {
  DCHECK(!context_menu_shown_);
  context_menu_shown_ = true;
  // Once the context menu is shown, we no longer need to keep track of the
  // mouse or touch press.
  keep_visible_for_right_click_or_hold_ = false;
  MaybeAddOrRemoveAction();
}

void CastToolbarButtonController::OnContextMenuHidden() {
  DCHECK(context_menu_shown_);
  context_menu_shown_ = false;
  MaybeAddOrRemoveAction();
}

void CastToolbarButtonController::KeepIconShownOnPressed() {
  DCHECK(!keep_visible_for_right_click_or_hold_);
  keep_visible_for_right_click_or_hold_ = true;
  MaybeAddOrRemoveAction();
}

void CastToolbarButtonController::MaybeHideIconOnReleased() {
  keep_visible_for_right_click_or_hold_ = false;
  MaybeAddOrRemoveAction();
}

void CastToolbarButtonController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CastToolbarButtonController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
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
      base::BindRepeating(&CastToolbarButtonController::MaybeAddOrRemoveAction,
                          base::Unretained(this)));
}

void CastToolbarButtonController::MaybeAddOrRemoveAction() {
  if (ShouldEnableAction()) {
    for (Observer& observer : observers_)
      observer.ShowIcon();
  } else {
    for (Observer& observer : observers_)
      observer.HideIcon();
  }
}
