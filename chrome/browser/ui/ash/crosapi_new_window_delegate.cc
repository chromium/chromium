// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/crosapi_new_window_delegate.h"

#include "base/logging.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ui/ash/chrome_new_window_client.h"
#include "ui/base/dragdrop/os_exchange_data.h"

CrosapiNewWindowDelegate::CrosapiNewWindowDelegate(
    ash::NewWindowDelegate* delegate)
    : delegate_(delegate) {}

CrosapiNewWindowDelegate::~CrosapiNewWindowDelegate() = default;

void CrosapiNewWindowDelegate::NewTab() {
  crosapi::BrowserManager::Get()->NewTab();
}

void CrosapiNewWindowDelegate::NewWindow(bool incognito,
                                         bool should_trigger_session_restore) {
  crosapi::BrowserManager::Get()->NewWindow(incognito);
}

void CrosapiNewWindowDelegate::NewWindowForWebUITabDrop(
    aura::Window* source_window,
    const ui::OSExchangeData& drop_data,
    NewWindowForWebUITabDropCallback closure) {
  delegate_->NewWindowForWebUITabDrop(source_window, drop_data,
                                      std::move(closure));
}

void CrosapiNewWindowDelegate::OpenUrl(const GURL& url,
                                       bool from_user_interaction) {
  crosapi::BrowserManager::Get()->OpenUrl(url);
}

void CrosapiNewWindowDelegate::OpenCalculator() {
  delegate_->OpenCalculator();
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

void CrosapiNewWindowDelegate::OpenFeedbackPage(
    FeedbackSource source,
    const std::string& description_template) {
  delegate_->OpenFeedbackPage(source, description_template);
}
