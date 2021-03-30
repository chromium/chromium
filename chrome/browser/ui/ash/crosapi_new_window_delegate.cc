// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/crosapi_new_window_delegate.h"

#include "base/logging.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ui/ash/chrome_new_window_client.h"

CrosapiNewWindowDelegate::CrosapiNewWindowDelegate(
    ash::NewWindowDelegate* delegate)
    : delegate_(delegate) {}

CrosapiNewWindowDelegate::~CrosapiNewWindowDelegate() = default;

void CrosapiNewWindowDelegate::NewTab() {
  crosapi::BrowserManager::Get()->NewTab();
}

void CrosapiNewWindowDelegate::NewTabWithUrl(const GURL& url,
                                             bool from_user_interaction) {
  // TODO(crbug.com/1188020): Forward to register browser via crosapi.
  LOG(WARNING)
      << "CrosapiNewWindowDelegate::NewTabWithUrl is currently forwarded "
      << "to ash-chrome";
  delegate_->NewTabWithUrl(url, from_user_interaction);
}

void CrosapiNewWindowDelegate::NewWindow(bool incognito) {
  crosapi::BrowserManager::Get()->NewWindow(incognito);
}

void CrosapiNewWindowDelegate::OpenFileManager() {
  delegate_->OpenFileManager();
}

void CrosapiNewWindowDelegate::OpenDownloadsFolder() {
  delegate_->OpenDownloadsFolder();
}

void CrosapiNewWindowDelegate::OpenCrosh() {
  delegate_->OpenCrosh();
}

void CrosapiNewWindowDelegate::OpenGetHelp() {
  delegate_->OpenGetHelp();
}

void CrosapiNewWindowDelegate::RestoreTab() {
  crosapi::BrowserManager::Get()->RestoreTab();
}

void CrosapiNewWindowDelegate::ShowKeyboardShortcutViewer() {
  delegate_->ShowKeyboardShortcutViewer();
}

void CrosapiNewWindowDelegate::ShowTaskManager() {
  delegate_->ShowTaskManager();
}

void CrosapiNewWindowDelegate::OpenDiagnostics() {
  delegate_->OpenDiagnostics();
}

void CrosapiNewWindowDelegate::OpenFeedbackPage(bool from_assistant) {
  delegate_->OpenFeedbackPage(from_assistant);
}
