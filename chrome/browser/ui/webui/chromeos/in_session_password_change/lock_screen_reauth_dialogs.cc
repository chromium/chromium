// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_reauth_dialogs.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/json/json_writer.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager_factory.h"
#include "chrome/browser/ash/login/ui/oobe_dialog_size_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/base_lock_dialog.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/confirm_password_change_handler.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_captive_portal_dialog.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_network_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/aura/window.h"
#include "ui/strings/grit/ui_strings.h"

namespace chromeos {

namespace {
LockScreenStartReauthDialog* g_dialog = nullptr;

InSessionPasswordSyncManager* GetInSessionPasswordSyncManager() {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);

  return InSessionPasswordSyncManagerFactory::GetForProfile(profile);
}

bool IsDialogLoaded(bool is_loaded,
                    base::OnceClosure& on_loaded_callback,
                    base::OnceClosure callback) {
  if (is_loaded)
    return true;
  DCHECK(!on_loaded_callback);
  on_loaded_callback = std::move(callback);
  return false;
}

void OnDialogLoaded(bool& is_loaded, base::OnceClosure& on_loaded_callback) {
  if (is_loaded)
    return;
  is_loaded = true;
  if (on_loaded_callback) {
    std::move(on_loaded_callback).Run();
  }
}

}  // namespace

// Cleans up the delegate for a WebContentsModalDialogManager on destruction, or
// on WebContents destruction, whichever comes first.
class LockScreenStartReauthDialog::ModalDialogManagerCleanup
    : public content::WebContentsObserver {
 public:
  // This constructor automatically observes |web_contents| for its lifetime.
  explicit ModalDialogManagerCleanup(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ModalDialogManagerCleanup(const ModalDialogManagerCleanup&) = delete;
  ModalDialogManagerCleanup& operator=(const ModalDialogManagerCleanup&) =
      delete;
  ~ModalDialogManagerCleanup() override { ResetDelegate(); }

  // content::WebContentsObserver:
  void WebContentsDestroyed() override { ResetDelegate(); }

  void ResetDelegate() {
    if (!web_contents())
      return;
    web_modal::WebContentsModalDialogManager::FromWebContents(web_contents())
        ->SetDelegate(nullptr);
  }
};

// static
gfx::Size LockScreenStartReauthDialog::CalculateLockScreenReauthDialogSize(
    bool is_new_layout_enabled) {
  if (!is_new_layout_enabled) {
    return kBaseLockDialogSize;
  }

  // LockscreenReauth Dialog size should match OOBE Dialog size.
  return CalculateOobeDialogSizeForPrimaryDisplay();
}

void LockScreenStartReauthDialog::Show() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_dialog) {
    g_dialog->Focus();
    return;
  }
  g_dialog = this;
  g_browser_process->profile_manager()->CreateProfileAsync(
      ProfileHelper::GetLockScreenProfileDir(),
      base::BindRepeating(&LockScreenStartReauthDialog::OnProfileCreated,
                          weak_factory_.GetWeakPtr()));
}

void LockScreenStartReauthDialog::OnProfileCreated(
    Profile* profile,
    Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED) {
    profile_ = profile;
    g_dialog->ShowSystemDialogForBrowserContext(
        profile->GetPrimaryOTRProfile(/*create_if_needed=*/true));
    const NetworkStateInformer::State state = network_state_informer_->state();
    // Show network or captive portal screen if needed.
    // TODO(crbug.com/1237407): Handle other states in NetworkStateInformer
    // properly.
    if (state == NetworkStateInformer::OFFLINE) {
      ShowLockScreenNetworkDialog();
    } else if (state == NetworkStateInformer::CAPTIVE_PORTAL) {
      ShowLockScreenCaptivePortalDialog();
    }
  } else if (status != Profile::CREATE_STATUS_CREATED) {
    // TODO(mohammedabdon): Create some generic way to show an error on the lock
    // screen.
    LOG(ERROR) << "Failed to load lockscreen profile";
  }
}

void LockScreenStartReauthDialog::Dismiss() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_dialog)
    g_dialog->Close();
}

bool LockScreenStartReauthDialog::IsRunning() {
  return g_dialog;
}

int LockScreenStartReauthDialog::GetDialogWidth() {
  gfx::Size ret;
  GetDialogSize(&ret);
  return ret.width();
}

void LockScreenStartReauthDialog::DeleteLockScreenNetworkDialog() {
  if (!lock_screen_network_dialog_)
    return;
  lock_screen_network_dialog_.reset();
  if (is_network_dialog_visible_) {
    is_network_dialog_visible_ = false;
    auto* password_sync_manager = GetInSessionPasswordSyncManager();
    password_sync_manager->DismissDialog();
  }
}

void LockScreenStartReauthDialog::OnDialogShown(content::WebUI* webui) {
  BaseLockDialog::OnDialogShown(webui);

  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      webui->GetWebContents());
  web_modal::WebContentsModalDialogManager::FromWebContents(
      webui->GetWebContents())
      ->SetDelegate(this);
  modal_dialog_manager_cleanup_ =
      std::make_unique<ModalDialogManagerCleanup>(webui->GetWebContents());
}

void LockScreenStartReauthDialog::OnDialogClosed(
    const std::string& json_retval) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
  auto* password_sync_manager =
      InSessionPasswordSyncManagerFactory::GetForProfile(profile);
  password_sync_manager->ResetDialog();
}

void LockScreenStartReauthDialog::DismissLockScreenNetworkDialog() {
  if (lock_screen_network_dialog_)
    lock_screen_network_dialog_->Dismiss();
}

void LockScreenStartReauthDialog::DismissLockScreenCaptivePortalDialog() {
  if (captive_portal_dialog_)
    captive_portal_dialog_->Dismiss();
}

void LockScreenStartReauthDialog::ShowLockScreenNetworkDialog() {
  if (lock_screen_network_dialog_)
    return;
  DCHECK(profile_);
  is_network_dialog_visible_ = true;
  lock_screen_network_dialog_ =
      std::make_unique<chromeos::LockScreenNetworkDialog>(base::BindOnce(
          &LockScreenStartReauthDialog::DeleteLockScreenNetworkDialog,
          base::Unretained(this)));
  lock_screen_network_dialog_->Show(profile_);
}

void LockScreenStartReauthDialog::ShowLockScreenCaptivePortalDialog() {
  if (!captive_portal_dialog_) {
    captive_portal_dialog_ = std::make_unique<LockScreenCaptivePortalDialog>();
    OnCaptivePortalDialogReadyForTesting();
  }
  captive_portal_dialog_->Show(*profile_);
}

bool LockScreenStartReauthDialog::IsNetworkDialogLoadedForTesting(
    base::OnceClosure callback) {
  return IsDialogLoaded(is_network_dialog_loaded_for_testing_,
                        on_network_dialog_loaded_callback_for_testing_,
                        std::move(callback));
}

bool LockScreenStartReauthDialog::IsCaptivePortalDialogLoadedForTesting(
    base::OnceClosure callback) {
  return IsDialogLoaded(is_captive_portal_dialog_loaded_for_testing_,
                        on_captive_portal_dialog_loaded_callback_for_testing_,
                        std::move(callback));
}

void LockScreenStartReauthDialog::OnNetworkDialogReadyForTesting() {
  OnDialogLoaded(is_network_dialog_loaded_for_testing_,
                 on_network_dialog_loaded_callback_for_testing_);
}

void LockScreenStartReauthDialog::OnCaptivePortalDialogReadyForTesting() {
  OnDialogLoaded(is_captive_portal_dialog_loaded_for_testing_,
                 on_captive_portal_dialog_loaded_callback_for_testing_);
}

LockScreenStartReauthDialog::LockScreenStartReauthDialog()
    : BaseLockDialog(GURL(chrome::kChromeUILockScreenStartReauthURL),
                     CalculateLockScreenReauthDialogSize(
                         features::IsNewLockScreenReauthLayoutEnabled())),
      network_state_informer_(
          base::MakeRefCounted<chromeos::NetworkStateInformer>()) {
  network_state_informer_->Init();
  scoped_observation_.Observe(network_state_informer_.get());
}

LockScreenStartReauthDialog::~LockScreenStartReauthDialog() {
  DCHECK_EQ(this, g_dialog);
  scoped_observation_.Reset();
  DeleteLockScreenNetworkDialog();
  g_dialog = nullptr;
}

void LockScreenStartReauthDialog::UpdateState(
    NetworkError::ErrorReason reason) {
  const NetworkStateInformer::State state = network_state_informer_->state();

  if (state == NetworkStateInformer::OFFLINE) {
    ShowLockScreenNetworkDialog();
  } else if (state == NetworkStateInformer::CAPTIVE_PORTAL) {
    ShowLockScreenCaptivePortalDialog();
  } else {
    DismissLockScreenCaptivePortalDialog();
    if (is_network_dialog_visible_ && lock_screen_network_dialog_) {
      is_network_dialog_visible_ = false;
      lock_screen_network_dialog_->Close();
    }
  }
}

web_modal::WebContentsModalDialogHost*
LockScreenStartReauthDialog::GetWebContentsModalDialogHost() {
  return this;
}

gfx::Size LockScreenStartReauthDialog::GetMaximumDialogSize() {
  gfx::Size size;
  GetDialogSize(&size);
  return size;
}

gfx::NativeView LockScreenStartReauthDialog::GetHostView() const {
  return dialog_window();
}

gfx::Point LockScreenStartReauthDialog::GetDialogPosition(
    const gfx::Size& size) {
  gfx::Size host_size = GetHostView()->bounds().size();

  // Show all sub-dialogs at center-top.
  return gfx::Point(std::max(0, (host_size.width() - size.width()) / 2), 0);
}

void LockScreenStartReauthDialog::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  modal_dialog_host_observer_list_.AddObserver(observer);
}

void LockScreenStartReauthDialog::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  modal_dialog_host_observer_list_.RemoveObserver(observer);
}

}  // namespace chromeos
