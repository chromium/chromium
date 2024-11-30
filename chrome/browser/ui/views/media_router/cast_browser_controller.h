// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_BROWSER_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/toolbar/cast/cast_toolbar_button_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "components/media_router/browser/issues_observer.h"
#include "components/media_router/browser/mirroring_media_controller_host.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"

class Browser;
class ToolbarButton;

namespace media_router {

class MediaRouter;
class LoggerImpl;

// TODO(crbug.com/376495209): Add comments for methods and members of this class.
// Controller for the Cast toolbar icon. There should be one instance of this
// class per browser.
class CastBrowserController :
                          public IssuesObserver,
                          public MediaRoutesObserver,
                          public MirroringMediaControllerHost::Observer {

 public:
  explicit CastBrowserController(Browser* browser);
  CastBrowserController(Browser* browser,
                    MediaRouter* media_router);
  CastBrowserController(const CastBrowserController&) = delete;
  CastBrowserController& operator=(const CastBrowserController&) = delete;
  ~CastBrowserController() override;

  // Opens/closes the cast dialog.
  void ToggleDialog();
  // Updates the icon.
  void UpdateIcon();

  // media_router::IssuesObserver:
  void OnIssue(const media_router::Issue& issue) override;
  void OnIssuesCleared() override;

  // media_router::MediaRoutesObserver:
  void OnRoutesUpdated(
      const std::vector<media_router::MediaRoute>& routes) override;

  // MirroringMediaControllerHost::Observer:
  void OnFreezeInfoChanged() override;

 private:
  CastToolbarButtonController* GetActionController() const;

  ToolbarButton* GetToolbarButton() const;

  void LogIconChange(const gfx::VectorIcon* icon);

  void StopObservingMirroringMediaControllerHosts();

  const raw_ptr<Browser> browser_;

  // This value is set only when there is an outstanding issue.
  std::optional<media_router::IssueInfo::Severity> issue_severity_;

  bool has_local_route_ = false;

  const raw_ptr<LoggerImpl> logger_;

  // The list of routes we are observing to see if mirroring pauses.
  std::vector<MediaRoute::Id> tracked_mirroring_routes_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_BROWSER_CONTROLLER_H_
