// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_TOOLBAR_BUTTON_H_

#include "chrome/browser/ui/toolbar/media_router_action_controller.h"
#include "chrome/browser/ui/toolbar/media_router_contextual_menu.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "components/media_router/browser/issues_observer.h"
#include "ui/events/event.h"
#include "ui/views/metadata/metadata_header_macros.h"

class Browser;

namespace media_router {

class MediaRouter;
class LoggerImpl;

// Cast icon shown in the trusted area of toolbar. Its lifetime is tied to that
// of its parent ToolbarView. The icon is made visible in following situations:
// - The icon is pinned by the user or an admin policy.
// - The cast dialog is shown.
// - There is an active local cast session.
// - There is an outstanding issue.
class CastToolbarButton : public ToolbarButton,
                          public MediaRouterActionController::Observer,
                          public IssuesObserver,
                          public MediaRoutesObserver {
 public:
  METADATA_HEADER(CastToolbarButton);

  static std::unique_ptr<CastToolbarButton> Create(Browser* browser);

  CastToolbarButton(Browser* browser,
                    MediaRouter* media_router,
                    std::unique_ptr<MediaRouterContextualMenu> context_menu);
  CastToolbarButton(const CastToolbarButton&) = delete;
  CastToolbarButton& operator=(const CastToolbarButton&) = delete;
  ~CastToolbarButton() override;

  // MediaRouterActionController::Observer:
  void ShowIcon() override;
  void HideIcon() override;
  void ActivateIcon() override;
  void DeactivateIcon() override;

  // media_router::IssuesObserver:
  void OnIssue(const media_router::Issue& issue) override;
  void OnIssuesCleared() override;

  // media_router::MediaRoutesObserver:
  void OnRoutesUpdated(const std::vector<media_router::MediaRoute>& routes,
                       const std::vector<media_router::MediaRoute::Id>&
                           joinable_route_ids) override;

  // ToolbarButton:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void UpdateIcon() override;

  MediaRouterContextualMenu* context_menu_for_test() {
    return context_menu_.get();
  }

 private:
  MediaRouterActionController* GetActionController() const;

  // Updates insets per touch ui mode.
  void UpdateLayoutInsetDelta();

  void ButtonPressed();

  void LogIconChange(const gfx::VectorIcon* icon);

  Browser* const browser_;
  Profile* const profile_;

  // This value is set only when there is an outstanding issue.
  std::unique_ptr<media_router::IssueInfo> current_issue_;

  std::unique_ptr<MediaRouterContextualMenu> context_menu_;

  bool has_local_display_route_ = false;

  const gfx::VectorIcon* icon_ = nullptr;

  LoggerImpl* const logger_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_TOOLBAR_BUTTON_H_
