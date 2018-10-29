// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_CONTROLLER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/media/router/issues_observer.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/toolbar/media_router_contextual_menu.h"
#include "components/prefs/pref_change_registrar.h"

class ComponentActionDelegate;

// Controller for the Cast toolbar icon that determines when to show and hide
// icon. There should be one instance of this class per profile, and it should
// only be used on the UI thread.
// TODO(takumif): Rename this class to CastToolbarIconController.
class MediaRouterActionController : public media_router::IssuesObserver,
                                    public media_router::MediaRoutesObserver,
                                    public MediaRouterContextualMenu::Observer {
 public:
  class Observer {
   public:
    virtual void ShowIcon() = 0;
    virtual void HideIcon() = 0;
    // TODO(https://crbug.com/872392): Use the common code path to show and hide
    // the icon's inkdrop.
    // This is called when the icon should enter pressed state.
    virtual void ActivateIcon() = 0;
    // This is called when the icon should enter unpressed state.
    virtual void DeactivateIcon() = 0;
  };

  explicit MediaRouterActionController(Profile* profile);
  // Constructor for injecting dependencies in tests.
  MediaRouterActionController(
      Profile* profile,
      media_router::MediaRouter* router,
      ComponentActionDelegate* component_action_delegate);
  ~MediaRouterActionController() override;

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
  void OnRoutesUpdated(const std::vector<media_router::MediaRoute>& routes,
                       const std::vector<media_router::MediaRoute::Id>&
                           joinable_route_ids) override;

  // Called when a Media Router dialog is shown or hidden, and updates the
  // visibility of the action icon. Overridden in tests.
  virtual void OnDialogShown();
  virtual void OnDialogHidden();

  // MediaRouterContextualMenu::Observer:
  void OnContextMenuShown() override;
  void OnContextMenuHidden() override;

  // On Windows, when the user right-clicks on the toolbar icon, the context
  // menu appears on mouse release. Since the dialog, if shown, disappears on
  // mouse press, we need to make sure that the icon does not get hidden between
  // mouse press and mouse release. These methods ensure that.
  void KeepIconOnRightMousePressed();
  void MaybeHideIconOnRightMouseReleased();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns |true| if the Media Router action should be present on the toolbar
  // or the overflow menu.
  bool ShouldEnableAction() const;

 private:
  friend class MediaRouterActionControllerUnitTest;
  FRIEND_TEST_ALL_PREFIXES(MediaRouterActionControllerUnitTest,
                           EphemeralIconForRoutesAndIssues);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterActionControllerUnitTest,
                           EphemeralIconForDialog);

  // Adds or removes the Media Router action icon to/from
  // |component_action_delegate_| if necessary, depending on whether or not
  // we have issues, local routes or a dialog.
  virtual void MaybeAddOrRemoveAction();

  // Implementation-specific helper methods for MaybeAddOrRemoveAction().
  // TODO(takumif): Remove MaybeAddOrRemoveComponentAction() once the trusted
  // area icon is completely rolled out.
  void MaybeAddOrRemoveComponentAction();
  void MaybeAddOrRemoveTrustedAreaIcon();

  // The profile |this| is associated with. There should be one instance of this
  // class per profile.
  Profile* const profile_;

  // The delegate that is responsible for showing and hiding the icon on the
  // toolbar. It outlives |this|.
  ComponentActionDelegate* const component_action_delegate_;

  bool has_issue_ = false;
  bool has_local_display_route_ = false;

  // Whether the media router action is shown by an administrator policy.
  bool shown_by_policy_;

  // The number of dialogs that are currently open.
  size_t dialog_count_ = 0;

  // Whether the Cast toolbar icon is showing a context menu. The toolbar icon
  // should not be hidden while a context menu is shown.
  bool context_menu_shown_ = false;

  // Whether the right mouse button is pressed on the toolbar icon. On Windows,
  // when the user right clicks on the toolbar icon, the dialog gets hidden on
  // mouse press, and the context menu gets shown on mouse release. If the icon
  // is ephemeral, it gets hidden on mouse press and can't show the context
  // menu. So we must keep the icon shown while right mouse button is pressed.
  bool keep_visible_for_right_mouse_button_ = false;

  PrefChangeRegistrar pref_change_registrar_;

  base::ObserverList<Observer>::Unchecked observers_;

  base::WeakPtrFactory<MediaRouterActionController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterActionController);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_CONTROLLER_H_
