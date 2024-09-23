// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_NEW_WINDOW_CHROME_NEW_WINDOW_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_NEW_WINDOW_CHROME_NEW_WINDOW_CLIENT_H_

#include <memory>
#include <string>

#include "ash/public/cpp/new_window_delegate.h"
#include "components/arc/intent_helper/control_camera_app_delegate.h"
#include "url/gurl.h"

// Handles opening new tabs and windows on behalf of ash (over mojo) and the
// ARC bridge (via a delegate in the browser process).
class ChromeNewWindowClient : public ash::NewWindowDelegate,
                              public arc::ControlCameraAppDelegate {
 public:
  ChromeNewWindowClient();

  ChromeNewWindowClient(const ChromeNewWindowClient&) = delete;
  ChromeNewWindowClient& operator=(const ChromeNewWindowClient&) = delete;

  ~ChromeNewWindowClient() override;

  static ChromeNewWindowClient* Get();

  // Overridden from ash::NewWindowDelegate:
  void NewTab() override;
  void NewWindow(bool incognito, bool should_trigger_session_restore) override;
  void NewWindowForDetachingTab(
      aura::Window* source_window,
      const ui::OSExchangeData& drop_data,
      NewWindowForDetachingTabCallback closure) override;
  void OpenUrl(const GURL& url,
               OpenUrlFrom from,
               Disposition disposition) override;
  void OpenCalculator() override;
  void OpenFileManager() override;
  void OpenDownloadsFolder() override;
  void OpenCrosh() override;
  void OpenGetHelp() override;
  void RestoreTab() override;
  void ShowShortcutCustomizationApp() override;
  void ShowTaskManager() override;
  void OpenDiagnostics() override;
  void OpenFeedbackPage(FeedbackSource source,
                        const std::string& description_template) override;
  void OpenPersonalizationHub() override;
  void OpenCaptivePortalSignin(const GURL& url) override;
  void OpenFile(const base::FilePath& file_path) override;

  // arc::ControlCameraAppDelegate:
  void LaunchCameraApp(const std::string& queries, int32_t task_id) override;
  void CloseCameraApp() override;
  bool IsCameraAppEnabled() override;

 private:
  class TabRestoreHelper;

  std::unique_ptr<TabRestoreHelper> tab_restore_helper_;
};

#endif  // CHROME_BROWSER_UI_ASH_NEW_WINDOW_CHROME_NEW_WINDOW_CLIENT_H_
