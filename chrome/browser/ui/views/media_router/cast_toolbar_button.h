// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_TOOLBAR_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/toolbar/cast/cast_contextual_menu.h"
#include "chrome/browser/ui/toolbar/cast/cast_toolbar_button_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "components/media_router/browser/issues_observer.h"
#include "components/media_router/browser/mirroring_media_controller_host.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"

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
                          public CastToolbarButtonController::Observer,
                          public IssuesObserver,
                          public MediaRoutesObserver,
                          public MirroringMediaControllerHost::Observer {
  METADATA_HEADER(CastToolbarButton, ToolbarButton)

 public:
  static std::unique_ptr<CastToolbarButton> Create(Browser* browser);

  CastToolbarButton(Browser* browser,
                    MediaRouter* media_router,
                    std::unique_ptr<CastContextualMenu> context_menu);
  CastToolbarButton(const CastToolbarButton&) = delete;
  CastToolbarButton& operator=(const CastToolbarButton&) = delete;
  ~CastToolbarButton() override;

  // CastToolbarButtonController::Observer:
  void ShowIcon() override;
  void HideIcon() override;
  void ActivateIcon() override;
  void DeactivateIcon() override;

  // media_router::IssuesObserver:
  void OnIssue(const media_router::Issue& issue) override;
  void OnIssuesCleared() override;

  // media_router::MediaRoutesObserver:
  void OnRoutesUpdated(
      const std::vector<media_router::MediaRoute>& routes) override;

  // MirroringMediaControllerHost::Observer:
  void OnFreezeInfoChanged() override;

  // ToolbarButton:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnThemeChanged() override;
  void UpdateIcon() override;

  CastContextualMenu* context_menu_for_test() { return context_menu_.get(); }

 private:
  CastToolbarButtonController* GetActionController() const;

  // Updates insets per touch ui mode.
  void UpdateLayoutInsetDelta();

  void ButtonPressed();

  void LogIconChange(const gfx::VectorIcon* icon);

  void StopObservingMirroringMediaControllerHosts();

  const raw_ptr<Browser> browser_;
  const raw_ptr<Profile> profile_;

  // This value is set only when there is an outstanding issue.
  std::optional<media_router::IssueInfo::Severity> issue_severity_;

  std::unique_ptr<CastContextualMenu> context_menu_;

  bool has_local_route_ = false;

  raw_ptr<const gfx::VectorIcon> icon_ = nullptr;

  const raw_ptr<LoggerImpl> logger_;

  // The list of routes we are observing to see if mirroring pauses.
  std::vector<MediaRoute::Id> tracked_mirroring_routes_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_TOOLBAR_BUTTON_H_
