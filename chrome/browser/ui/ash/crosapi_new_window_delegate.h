// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CROSAPI_NEW_WINDOW_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_CROSAPI_NEW_WINDOW_DELEGATE_H_

#include "ash/public/cpp/new_window_delegate.h"

// Handles opening new tabs and windows on behalf on ash.
// Use crosapi to control Lacros Browser.
// Web browser unrelated operations are forwarded to the given delegate.
class CrosapiNewWindowDelegate : public ash::NewWindowDelegate {
 public:
  // CrosapiNewWindowDelegate forwards methods which are not related to
  // web browser to the given |delegate|.
  explicit CrosapiNewWindowDelegate(ash::NewWindowDelegate* delegate);
  CrosapiNewWindowDelegate(const CrosapiNewWindowDelegate&) = delete;
  const CrosapiNewWindowDelegate& operator=(const CrosapiNewWindowDelegate&) =
      delete;
  ~CrosapiNewWindowDelegate() override;

  // Overridden from ash::NewWindowDelegate:
  void NewTab() override;
  void NewTabWithUrl(const GURL& url, bool from_user_interaction) override;
  void NewWindow(bool incognito) override;
  void OpenFileManager() override;
  void OpenDownloadsFolder() override;
  void OpenCrosh() override;
  void OpenGetHelp() override;
  void RestoreTab() override;
  void ShowKeyboardShortcutViewer() override;
  void ShowTaskManager() override;
  void OpenDiagnostics() override;
  void OpenFeedbackPage(bool from_assistant) override;

 private:
  // Not owned. Practically, this should point to ChromeNewWindowClient in
  // production.
  ash::NewWindowDelegate* const delegate_;
};

#endif  // CHROME_BROWSER_UI_ASH_CROSAPI_NEW_WINDOW_DELEGATE_H_
