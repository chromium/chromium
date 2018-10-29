// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_H_
#define CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/media/router/issues_observer.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/media_router_contextual_menu.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_observer.h"

class Browser;
class TabStripModel;

namespace gfx {
struct VectorIcon;
}

namespace media_router {
class MediaRouterDialogControllerImplBase;
}  // namespace media_router

// The class for the Media Router component action that will be shown in
// the toolbar.
class MediaRouterAction : public ToolbarActionViewController,
                          public media_router::IssuesObserver,
                          public media_router::MediaRoutesObserver,
                          public TabStripModelObserver,
                          public ToolbarActionsBarObserver {
 public:
  MediaRouterAction(Browser* browser, ToolbarActionsBar* toolbar_actions_bar);
  ~MediaRouterAction() override;

  static SkColor GetIconColor(const gfx::VectorIcon& icon_id);

  // ToolbarActionViewController implementation.
  std::string GetId() const override;
  void SetDelegate(ToolbarActionViewDelegate* delegate) override;
  gfx::Image GetIcon(content::WebContents* web_contents,
                     const gfx::Size& size) override;
  base::string16 GetActionName() const override;
  base::string16 GetAccessibleName(content::WebContents* web_contents)
      const override;
  base::string16 GetTooltip(content::WebContents* web_contents)
      const override;
  bool IsEnabled(content::WebContents* web_contents) const override;
  bool WantsToRun(content::WebContents* web_contents) const override;
  bool HasPopup(content::WebContents* web_contents) const override;
  void HidePopup() override;
  gfx::NativeView GetPopupNativeView() override;
  ui::MenuModel* GetContextMenu() override;
  void OnContextMenuClosed() override;
  bool ExecuteAction(bool by_user) override;
  void UpdateState() override;
  bool DisabledClickOpensMenu() const override;

  // media_router::IssuesObserver:
  void OnIssue(const media_router::Issue& issue) override;
  void OnIssuesCleared() override;

  // media_router::MediaRoutesObserver:
  void OnRoutesUpdated(const std::vector<media_router::MediaRoute>& routes,
                       const std::vector<media_router::MediaRoute::Id>&
                           joinable_route_ids) override;

  // ToolbarStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // ToolbarActionsBarObserver:
  void OnToolbarActionsBarAnimationEnded() override;

  void OnDialogHidden();
  void OnDialogShown();

  void set_skip_close_overflow_menu_for_testing(bool val) {
    skip_close_overflow_menu_for_testing_ = val;
  }

 private:
  // Registers |this| with the MediaRouterDialogControllerImplBase associated
  // with |delegate_|'s current WebContents if |this| is not shown in overflow
  // mode.
  void RegisterWithDialogController();

  // Called when a new browser window is opened or when |delegate_| is swapped
  // out to be non-null and has a valid WebContents.
  // This updates the pressed/unpressed state of the icon, which is different
  // on a per-tab basis.
  void UpdateDialogState();

  // Returns a reference to the MediaRouterDialogControllerImplBase associated
  // with |delegate_|'s current WebContents. Guaranteed to be non-null.
  // |delegate_| and its current WebContents must not be null.
  // Marked virtual for tests.
  virtual media_router::MediaRouterDialogControllerImplBase*
  GetMediaRouterDialogController();

  // Checks if the current icon of MediaRouterAction has changed. If so,
  // updates |current_icon_|.
  void MaybeUpdateIcon();

  const gfx::VectorIcon& GetCurrentIcon() const;

  void DestroyContextMenu();

  // The current icon to show. This is updated based on the current issues and
  // routes since |this| is an IssueObserver and MediaRoutesObserver.
  const gfx::VectorIcon* current_icon_;

  // The current issue shown in the Media Router WebUI, set in OnIssue() and
  // cleared in OnIssuesCleared().
  std::unique_ptr<media_router::IssueInfo> current_issue_;

  // Whether a local displayable active route exists.
  bool has_local_display_route_;

  // Whether the Media Router dialog is shown in the current tab.
  // This should only be updated in OnDialogShown() and OnDialogHidden().
  bool has_dialog_;

  ToolbarActionViewDelegate* delegate_;

  Browser* const browser_;
  ToolbarActionsBar* const toolbar_actions_bar_;

  std::unique_ptr<MediaRouterContextualMenu> contextual_menu_;

  ScopedObserver<TabStripModel, TabStripModelObserver>
      tab_strip_model_observer_;
  ScopedObserver<ToolbarActionsBar, ToolbarActionsBarObserver>
      toolbar_actions_bar_observer_;

  bool skip_close_overflow_menu_for_testing_;

  base::WeakPtrFactory<MediaRouterAction> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterAction);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_H_
