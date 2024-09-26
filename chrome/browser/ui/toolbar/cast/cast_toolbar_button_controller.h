// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_CAST_CAST_TOOLBAR_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOOLBAR_CAST_CAST_TOOLBAR_BUTTON_CONTROLLER_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/toolbar/cast/cast_contextual_menu.h"
#include "components/media_router/browser/issues_observer.h"
#include "components/media_router/browser/media_routes_observer.h"
#include "components/prefs/pref_change_registrar.h"

// Controller for the Cast toolbar icon that determines when to show and hide
// icon. There should be one instance of this class per profile, and it should
// only be used on the UI thread.
class CastToolbarButtonController : public media_router::IssuesObserver,
                                    public media_router::MediaRoutesObserver,
                                    public CastContextualMenu::Observer {
 public:
  // TODO(takumif): CastToolbarIcon is the only Observer implementation.
  // Observer should be renamed to make it clear that it is responsible for
  // changing icon states when its methods are called.
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void ShowIcon() {}
    virtual void HideIcon() {}
    // TODO(crbug.com/40588598): Use the common code path to show and hide
    // the icon's inkdrop.
    // This is called when the icon should enter pressed state.
    virtual void ActivateIcon() {}
    // This is called when the icon should enter unpressed state.
    virtual void DeactivateIcon() {}
  };

  explicit CastToolbarButtonController(Profile* profile);
  // Constructor for injecting dependencies in tests.
  CastToolbarButtonController(Profile* profile,
                              media_router::MediaRouter* router);

  CastToolbarButtonController(const CastToolbarButtonController&) = delete;
  CastToolbarButtonController& operator=(const CastToolbarButtonController&) =
      delete;

  ~CastToolbarButtonController() override;

  // Whether the media router action is shown by an administrator policy.
  static bool IsActionShownByPolicy(Profile* profile);

  // Gets and sets the preference for whether the media router action should be
  // pinned to the toolbar/overflow menu.
  static bool GetAlwaysShowActionPref(Profile* profile);
  static void SetAlwaysShowActionPref(Profile* profile, bool always_show);

  // media_router::IssuesObserver:
  void OnIssue(const media_router::Issue& issue) override;
  void OnIssuesCleared() override;

  // media_router::MediaRoutesObserver:
  void OnRoutesUpdated(
      const std::vector<media_router::MediaRoute>& routes) override;

  // Called when a Media Router dialog is shown or hidden, and updates the
  // visibility of the action icon. Overridden in tests.
  virtual void OnDialogShown();
  virtual void OnDialogHidden();

  // CastContextualMenu::Observer:
  void OnContextMenuShown() override;
  void OnContextMenuHidden() override;

  // On Windows (with a right click) and Chrome OS (with touch), pressing the
  // toolbar icon makes the dialog disappear, but the context menu does not
  // appear until mouse/touch release. These methods ensure that the icon is
  // still shown at mouse/touch release so that the context menu can be shown.
  void KeepIconShownOnPressed();
  void MaybeHideIconOnReleased();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns |true| if the Media Router action should be present on the toolbar
  // or the overflow menu.
  bool ShouldEnableAction() const;

 private:
  friend class CastToolbarButtonControllerUnitTest;
  FRIEND_TEST_ALL_PREFIXES(CastToolbarButtonControllerUnitTest,
                           EphemeralIconForIssues);
  FRIEND_TEST_ALL_PREFIXES(CastToolbarButtonControllerUnitTest,
                           EphemeralIconForDialog);

  // Adds or removes the Cast icon to/from the toolbar if necessary,
  // depending on whether or not we have issues, local routes or a dialog.
  virtual void MaybeToggleIconVisibility();

  // The profile |this| is associated with. There should be one instance of this
  // class per profile.
  const raw_ptr<Profile> profile_;

  bool has_issue_ = false;
  bool has_local_display_route_ = false;

  // Whether the media router action is shown by an administrator policy.
  bool shown_by_policy_;

  // The number of dialogs that are currently open.
  size_t dialog_count_ = 0;

  // Whether the Cast toolbar icon is showing a context menu. The toolbar icon
  // should not be hidden while a context menu is shown.
  bool context_menu_shown_ = false;

  // On Windows (with the right mouse button) and on Chrome OS (with touch),
  // when the user presses the toolbar icon, the dialog gets hidden, but the
  // context menu is not shown until mouse/touch release. If the icon is
  // ephemeral, it gets hidden on press and can't show the context menu. So we
  // must keep the icon shown while the right click or touch is held.
  bool keep_visible_for_right_click_or_hold_ = false;

  PrefChangeRegistrar pref_change_registrar_;

  base::ObserverList<Observer>::Unchecked observers_;

  base::WeakPtrFactory<CastToolbarButtonController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_CAST_CAST_TOOLBAR_BUTTON_CONTROLLER_H_
