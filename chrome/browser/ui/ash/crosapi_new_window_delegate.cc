// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/crosapi_new_window_delegate.h"

#include "base/logging.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/window_util.h"
#include "chrome/browser/ash/fileapi/external_file_url_util.h"
#include "chrome/browser/ui/ash/chrome_new_window_client.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_util.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "components/exo/wm_helper.h"
#include "content/public/common/url_constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_observer.h"
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

// Observes the aura::Window instances created after the webui tab-drop
// has been requested. Its OnWindowPropertyChanged() override checks for
// a specific `window_id` to invoke the callback routine.
class CrosapiNewWindowDelegate::DetachedWindowObserver
    : public exo::WMHelper::ExoWindowObserver,
      public aura::WindowObserver {
 public:
  explicit DetachedWindowObserver(CrosapiNewWindowDelegate* owner,
                                  NewWindowForDetachingTabCallback callback);
  ~DetachedWindowObserver() override;

  // aura::WindowObserver overrides.
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroying(aura::Window* window) override;

  // WMHelper::ExoWindowObserver overrides.
  void OnExoWindowCreated(aura::Window* window) override;

  void SetWindowID(const std::string& window_id);

  void StopObserving();

 private:
  void RunCallbackAndExit(aura::Window* new_window);

  raw_ptr<CrosapiNewWindowDelegate> owner_;

  // Observes windows launched after window tab-drop request.
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      observed_windows_{this};

  // The callback must be called, even out of the scope of
  // OnWindowPropertyChanged(), since it owns ownership of its callee
  // (ash::TabDragDropDelegate).
  NewWindowForDetachingTabCallback callback_;

  // Stores the window id of the Lacros window created by
  // CrosapiNewWindowDelegate::NewWindowForDetachingTab().
  std::string window_id_;
};

CrosapiNewWindowDelegate::DetachedWindowObserver::DetachedWindowObserver(
    CrosapiNewWindowDelegate* owner,
    NewWindowForDetachingTabCallback callback)
    : owner_(owner), callback_(std::move(callback)) {
  // This object is instantiated before the relevant aura::Windows are created.
  exo::WMHelper::GetInstance()->AddExoWindowObserver(this);
}

CrosapiNewWindowDelegate::DetachedWindowObserver::~DetachedWindowObserver() {
  exo::WMHelper::GetInstance()->RemoveExoWindowObserver(this);

  if (!callback_.is_null()) {
    std::move(callback_).Run(/*new_window=*/nullptr);
  }
}

void CrosapiNewWindowDelegate::DetachedWindowObserver::OnExoWindowCreated(
    aura::Window* new_window) {
  if (observed_windows_.IsObservingSource(new_window))
    return;

  observed_windows_.AddObservation(new_window);
}

void CrosapiNewWindowDelegate::DetachedWindowObserver::
    OnWindowVisibilityChanged(aura::Window* window, bool visible) {
  if (!visible || callback_.is_null()) {
    return;
  }

  // If the window with `window_id_` got visible as expected, run the callback.
  if (crosapi::GetShellSurfaceWindow(window_id_) == window) {
    RunCallbackAndExit(window);
    return;
  }
}

void CrosapiNewWindowDelegate::DetachedWindowObserver::OnWindowDestroying(
    aura::Window* window) {
  observed_windows_.RemoveObservation(window);
}

void CrosapiNewWindowDelegate::DetachedWindowObserver::SetWindowID(
    const std::string& window_id) {
  window_id_ = window_id;

  // Handle the case where the window has been visible already.
  auto* window = crosapi::GetShellSurfaceWindow(window_id_);
  if (window && window->IsVisible()) {
    RunCallbackAndExit(window);
    return;
  }
}

void CrosapiNewWindowDelegate::DetachedWindowObserver::StopObserving() {
  RunCallbackAndExit(/*new_window=*/nullptr);
}

void CrosapiNewWindowDelegate::DetachedWindowObserver::RunCallbackAndExit(
    aura::Window* new_window) {
  std::move(callback_).Run(new_window);
  owner_->DestroyWindowObserver();
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
    NewWindowForDetachingTabCallback callback) {
  if (crosapi::browser_util::IsLacrosWindow(source_window)) {
    std::u16string tab_id_str;
    std::u16string group_id_str;
    if (!tab_strip_ui::ExtractTabData(drop_data, &tab_id_str, &group_id_str)) {
      std::move(callback).Run(/*new_window=*/nullptr);
      return;
    }

    window_observer_ =
        std::make_unique<DetachedWindowObserver>(this, std::move(callback));

    // The window will be created on lacros side.
    // Post-creation routines, like split view / window snap handling need be
    // performed later.
    auto window_id_callback = base::BindOnce(
        [](DetachedWindowObserver* observer,
           crosapi::mojom::CreationResult result,
           const std::string& window_id) { observer->SetWindowID(window_id); },
        window_observer_.get());

    crosapi::BrowserManager::Get()->NewWindowForDetachingTab(
        tab_id_str, group_id_str, std::move(window_id_callback));
    return;
  }

  delegate_->NewWindowForDetachingTab(source_window, drop_data,
                                      std::move(callback));
}

void CrosapiNewWindowDelegate::OpenUrl(const GURL& url,
                                       OpenUrlFrom from,
                                       Disposition disposition) {
  GURL url_to_open = url;

  // Lacros can't see externalfile:// URLs. Convert them to regular file://
  // URLs via Fusebox.
  if (url_to_open.SchemeIs(content::kExternalFileScheme)) {
    content::BrowserContext* browser_context =
        ash::BrowserContextHelper::Get()->GetBrowserContextByUser(
            user_manager::UserManager::Get()->GetPrimaryUser());
    if (browser_context) {
      url_to_open = ash::ExternalFileURLToFuseboxMonikerFileURL(
          browser_context, url, /*read_only=*/true,
          /*lifetime=*/base::Hours(20));
    }
  }

  crosapi::BrowserManager::Get()->OpenUrl(url_to_open, OpenUrlFromToMojom(from),
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

void CrosapiNewWindowDelegate::OpenCaptivePortalSignin(const GURL& url) {
  crosapi::BrowserManager::Get()->OpenCaptivePortalSignin(url);
}

void CrosapiNewWindowDelegate::OpenFile(const base::FilePath& file_path) {
  delegate_->OpenFile(file_path);
}

void CrosapiNewWindowDelegate::DestroyWindowObserver() {
  window_observer_.reset();
}
