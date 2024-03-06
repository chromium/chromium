// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CROSAPI_NEW_WINDOW_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_CROSAPI_NEW_WINDOW_DELEGATE_H_

#include "ash/public/cpp/new_window_delegate.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"

// Handles opening new tabs and windows on behalf on ash.
// Use crosapi to control Lacros Browser.
// Web browser unrelated operations are forwarded to the given delegate.
class CrosapiNewWindowDelegate : public ash::NewWindowDelegate {
 public:
  // CrosapiNewWindowDelegate forwards methods which are not related to
  // web browser to the given `delegate`.
  explicit CrosapiNewWindowDelegate(ash::NewWindowDelegate* delegate);
  CrosapiNewWindowDelegate(const CrosapiNewWindowDelegate&) = delete;
  const CrosapiNewWindowDelegate& operator=(const CrosapiNewWindowDelegate&) =
      delete;
  ~CrosapiNewWindowDelegate() override;

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
  void ShowKeyboardShortcutViewer() override;
  void ShowShortcutCustomizationApp() override;
  void ShowTaskManager() override;
  void OpenDiagnostics() override;
  void OpenFeedbackPage(FeedbackSource source,
                        const std::string& description_template) override;
  void OpenPersonalizationHub() override;
  void OpenCaptivePortalSignin(const GURL& url) override;
  void OpenFile(const base::FilePath& file_path) override;

 private:
  class DetachedWindowObserver;

  // Destroys the DetachedWindowObserver once the "WebUI tab-drop callback
  // routine" has been invoked.
  void DestroyWindowObserver();

  // Not owned. Practically, this should point to ChromeNewWindowClient in
  // production.
  const raw_ptr<ash::NewWindowDelegate> delegate_;

  std::unique_ptr<DetachedWindowObserver> window_observer_;
};

#endif  // CHROME_BROWSER_UI_ASH_CROSAPI_NEW_WINDOW_DELEGATE_H_
