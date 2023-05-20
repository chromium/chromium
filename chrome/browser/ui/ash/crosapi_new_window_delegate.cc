// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/crosapi_new_window_delegate.h"

#include "base/logging.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/window_util.h"
#include "chrome/browser/ui/ash/chrome_new_window_client.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/dragdrop/os_exchange_data.h"

namespace {

crosapi::mojom::OpenUrlFrom OpenUrlFromToMojom(
    ash::NewWindowDelegate::OpenUrlFrom from) {
  switch (from) {
    case ash::NewWindowDelegate::OpenUrlFrom::kUnspecified:
    case ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction:
      return crosapi::mojom::OpenUrlFrom::kUnspecified;
    case ash::NewWindowDelegate::OpenUrlFrom::kArc:
      return crosapi::mojom::OpenUrlFrom::kArc;
  }
}

crosapi::mojom::OpenUrlParams::WindowOpenDisposition DispositionToMojom(
    ash::NewWindowDelegate::Disposition disposition) {
  switch (disposition) {
    case ash::NewWindowDelegate::Disposition::kNewForegroundTab:
      return crosapi::mojom::OpenUrlParams::WindowOpenDisposition::
          kNewForegroundTab;
    case ash::NewWindowDelegate::Disposition::kNewWindow:
      return crosapi::mojom::OpenUrlParams::WindowOpenDisposition::kNewWindow;
    case ash::NewWindowDelegate::Disposition::kOffTheRecord:
      return crosapi::mojom::OpenUrlParams::WindowOpenDisposition::
          kOffTheRecord;
    case ash::NewWindowDelegate::Disposition::kSwitchToTab:
      return crosapi::mojom::OpenUrlParams::WindowOpenDisposition::kSwitchToTab;
  }
}

}  // namespace

CrosapiNewWindowDelegate::WindowObserver::WindowObserver(
    CrosapiNewWindowDelegate* owner,
    NewWindowForDetachingTabCallback closure)
    : owner_(owner), closure_(std::move(closure)) {
  // This object is instantiated before the relevant aura::Windows are created.
  exo::WMHelper::GetInstance()->AddExoWindowObserver(this);
}

CrosapiNewWindowDelegate::WindowObserver::~WindowObserver() {
  windows_committed_prior_to_window_id_.clear();
  exo::WMHelper::GetInstance()->RemoveExoWindowObserver(this);

  if (!closure_.is_null())
    std::move(closure_).Run(/*new_window=*/nullptr);
}

void CrosapiNewWindowDelegate::WindowObserver::OnExoWindowCreated(
    aura::Window* new_window) {
  if (observed_windows_.IsObservingSource(new_window))
    return;

  observed_windows_.AddObservation(new_window);
}

void CrosapiNewWindowDelegate::WindowObserver::OnWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  if (!window || !visible || closure_.is_null())
    return;

  // In case the |window_id_| has not been set yet, record the all window for
  // future iteration.
  if (window_id_.empty()) {
    windows_committed_prior_to_window_id_.insert(window);
    return;
  }

  if (crosapi::GetShellSurfaceWindow(window_id_) == window) {
    std::move(closure_).Run(window);
    owner_->DestroyWindowObserver();
    return;
  }
}

void CrosapiNewWindowDelegate::WindowObserver::OnWindowDestroying(
    aura::Window* window) {
  observed_windows_.RemoveObservation(window);
}

void CrosapiNewWindowDelegate::WindowObserver::SetWindowID(
    const std::string& window_id) {
  window_id_ = window_id;

  // Handle the scenario where the exo::kSurfacePendingCommitKey property
  // is set prior to the |window_id_|.
  if (windows_committed_prior_to_window_id_.empty())
    return;

  auto* window = crosapi::GetShellSurfaceWindow(window_id_);
  for (auto* it : windows_committed_prior_to_window_id_) {
    if (window == it) {
      std::move(closure_).Run(window);
      owner_->DestroyWindowObserver();
      return;
    }
  }
}

CrosapiNewWindowDelegate::CrosapiNewWindowDelegate(
    ash::NewWindowDelegate* delegate)
    : delegate_(delegate) {}

CrosapiNewWindowDelegate::~CrosapiNewWindowDelegate() = default;

void CrosapiNewWindowDelegate::NewTab() {
  crosapi::BrowserManager::Get()->NewTab();
}

void CrosapiNewWindowDelegate::NewWindow(bool incognito,
                                         bool should_trigger_session_restore) {
  crosapi::BrowserManager::Get()->NewWindow(incognito,
                                            should_trigger_session_restore);
}

void CrosapiNewWindowDelegate::NewWindowForDetachingTab(
    aura::Window* source_window,
    const ui::OSExchangeData& drop_data,
    NewWindowForDetachingTabCallback closure) {
  if (crosapi::browser_util::IsLacrosWindow(source_window)) {
    std::u16string tab_id_str;
    std::u16string group_id_str;
    if (!tab_strip_ui::ExtractTabData(drop_data, &tab_id_str, &group_id_str)) {
      std::move(closure).Run(/*new_window=*/nullptr);
      return;
    }

    window_observer_ =
        std::make_unique<WindowObserver>(this, std::move(closure));

    // The window will be created on lacros side.
    // Post-creation routines, like split view / window snap handling need be
    // performed later.
    auto callback = base::BindOnce(
        [](WindowObserver* observer, crosapi::mojom::CreationResult result,
           const std::string& window_id) { observer->SetWindowID(window_id); },
        window_observer_.get());

    crosapi::BrowserManager::Get()->NewWindowForDetachingTab(
        tab_id_str, group_id_str, std::move(callback));
    return;
  }

  delegate_->NewWindowForDetachingTab(source_window, drop_data,
                                      std::move(closure));
}

void CrosapiNewWindowDelegate::OpenUrl(const GURL& url,
                                       OpenUrlFrom from,
                                       Disposition disposition) {
  crosapi::BrowserManager::Get()->OpenUrl(url, OpenUrlFromToMojom(from),
                                          DispositionToMojom(disposition));
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

void CrosapiNewWindowDelegate::ShowShortcutCustomizationApp() {
  delegate_->ShowShortcutCustomizationApp();
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

void CrosapiNewWindowDelegate::OpenPersonalizationHub() {
  delegate_->OpenPersonalizationHub();
}

void CrosapiNewWindowDelegate::DestroyWindowObserver() {
  window_observer_.reset();
}
