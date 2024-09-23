// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_reauth_dialogs.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/profile_auth_data.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/oobe_dialog_size_utils.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/base_lock_dialog.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_captive_portal_dialog.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_network_dialog.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_reauth_handler.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_start_reauth_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/session_manager/core/session_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "ui/aura/window.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {

namespace {
LockScreenStartReauthDialog* g_dialog = nullptr;

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

void LockScreenStartReauthDialog::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  // This is required for accessing the camera for SAML logins.
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), nullptr /* extension */);
}

bool LockScreenStartReauthDialog::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  // This is required for accessing the camera for SAML logins.
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(render_frame_host, security_origin, type);
}

// static
void LockScreenStartReauthDialog::Show() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(session_manager::SessionManager::Get()->IsScreenLocked());
  if (g_dialog) {
    g_dialog->Focus();
    return;
  }
  g_dialog = new LockScreenStartReauthDialog();
}

// static
LockScreenStartReauthDialog* LockScreenStartReauthDialog::GetInstance() {
  return g_dialog;
}

void LockScreenStartReauthDialog::OnProfileInitialized(Profile* profile) {
  if (!profile) {
    // TODO(b/333278160): Create some generic way to show an error on
    // the lock screen.
    LOG(ERROR) << "Failed to load lockscreen profile";
    return;
  }

  profile_ = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ShowSystemDialogForBrowserContext(profile_);
  const NetworkStateInformer::State state = network_state_informer_->state();
  // Show network or captive portal screen if needed.
  // TODO(crbug.com/1237407): Handle other states in NetworkStateInformer
  // properly.
  if (state == NetworkStateInformer::OFFLINE) {
    ShowLockScreenNetworkDialog();
  } else if (state == NetworkStateInformer::CAPTIVE_PORTAL) {
    ShowLockScreenCaptivePortalDialog();
  }
}

// static
void LockScreenStartReauthDialog::Dismiss() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_dialog)
    g_dialog->Close();
}

// static
bool LockScreenStartReauthDialog::IsShown() {
  return g_dialog;
}

int LockScreenStartReauthDialog::GetDialogWidth() const {
  gfx::Size ret;
  GetDialogSize(&ret);
  return ret.width();
}

content::WebContents* LockScreenStartReauthDialog::GetWebContents() {
  auto* web_ui = webui();
  if (!web_ui)
    return nullptr;
  return web_ui->GetWebContents();
}

void LockScreenStartReauthDialog::DeleteLockScreenNetworkDialog() {
  if (!lock_screen_network_dialog_)
    return;
  lock_screen_network_dialog_.reset();
  if (is_network_dialog_visible_) {
    is_network_dialog_visible_ = false;
    // We end up here when user manually closes the network dialog - in this
    // case we should also close reauth dialog as it doesn't make sense to show
    // it without a network connection
    Dismiss();
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
  // `BaseLockDialog::OnDialogClosed` will delete this so we need to
  // move callback to a local variable here for it to be executed.
  base::ScopedClosureRunner closure(
      std::move(on_dialog_closed_callback_for_testing_));
  BaseLockDialog::OnDialogClosed(json_retval);
}

void LockScreenStartReauthDialog::DismissLockScreenNetworkDialog() {
  if (is_network_dialog_visible_ && lock_screen_network_dialog_) {
    is_network_dialog_visible_ = false;
    lock_screen_network_dialog_->Dismiss();
  }
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
      std::make_unique<LockScreenNetworkDialog>(base::BindOnce(
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
      network_state_informer_(base::MakeRefCounted<NetworkStateInformer>()) {
  network_state_informer_->Init();
  scoped_observation_.Observe(network_state_informer_.get());

  HttpAuthDialog::AddObserver(this);

  enable_ash_httpauth_ = HttpAuthDialog::Enable();

  g_browser_process->profile_manager()->CreateProfileAsync(
      ProfileHelper::GetLockScreenProfileDir(),
      base::BindOnce(&LockScreenStartReauthDialog::OnProfileInitialized,
                     weak_factory_.GetWeakPtr()));
}

LockScreenStartReauthDialog::~LockScreenStartReauthDialog() {
  DCHECK_EQ(this, g_dialog);
  HttpAuthDialog::RemoveObserver(this);
  scoped_observation_.Reset();
  DeleteLockScreenNetworkDialog();
  g_dialog = nullptr;
}

void LockScreenStartReauthDialog::OnWebviewLoadAborted() {
  UpdateState(NetworkError::ERROR_REASON_FRAME_ERROR);
}

void LockScreenStartReauthDialog::UpdateState(
    NetworkError::ErrorReason reason) {
  if (is_proxy_auth_in_progress_)
    return;

  const NetworkStateInformer::State state = network_state_informer_->state();

  // If frame didn't load but we believe that we are online then we want to show
  // the network screen (mimicking behaviour of `ErrorScreen` on signin screen).
  if (reason == NetworkError::ERROR_REASON_FRAME_ERROR &&
      state == NetworkStateInformer::ONLINE) {
    ShowLockScreenNetworkDialog();
    return;
  }

  if (state == NetworkStateInformer::OFFLINE) {
    ShowLockScreenNetworkDialog();
  } else if (state == NetworkStateInformer::CAPTIVE_PORTAL) {
    ShowLockScreenCaptivePortalDialog();
  } else if (state == NetworkStateInformer::PROXY_AUTH_REQUIRED) {
    if (is_network_dialog_visible_) {
      should_reload_gaia_ = true;
    }
  } else {
    DismissLockScreenCaptivePortalDialog();
    DismissLockScreenNetworkDialog();
  }
  if (should_reload_gaia_) {
    DismissLockScreenNetworkDialog();
    LockScreenReauthHandler* reauth_handler =
        static_cast<LockScreenStartReauthUI*>(webui()->GetController())
            ->GetMainHandler();
    if (reauth_handler->IsAuthenticatorLoaded({})) {
      reauth_handler->ReloadGaia();
      should_reload_gaia_ = false;
    }
  }
}

bool LockScreenStartReauthDialog::IsLoadedForTesting(
    base::OnceClosure callback) {
  if (is_dialog_loaded_for_testing_)
    return true;
  DCHECK(!on_dialog_loaded_callback_for_testing_);
  on_dialog_loaded_callback_for_testing_ = std::move(callback);
  return false;
}

bool LockScreenStartReauthDialog::IsClosedForTesting(
    base::OnceClosure callback) {
  if (!is_dialog_loaded_for_testing_)
    return true;
  DCHECK(!on_dialog_closed_callback_for_testing_);
  on_dialog_closed_callback_for_testing_ = std::move(callback);
  return false;
}

void LockScreenStartReauthDialog::OnReadyForTesting() {
  if (is_dialog_loaded_for_testing_)
    return;
  is_dialog_loaded_for_testing_ = true;
  if (on_dialog_loaded_callback_for_testing_) {
    std::move(on_dialog_loaded_callback_for_testing_).Run();
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

void LockScreenStartReauthDialog::TransferHttpAuthCaches() {
  content::StoragePartition* webview_storage_partition =
      login::SigninPartitionManager::Factory::GetForBrowserContext(profile_)
          ->GetCurrentStoragePartition();
  if (webview_storage_partition) {
    // Transfer auth cache to system network context. This allows to preserve
    // proxy credentials between different unlock attempts.
    webview_storage_partition->GetNetworkContext()
        ->SaveHttpAuthCacheProxyEntries(base::BindOnce(
            &TransferHttpAuthCacheToSystemNetworkContext, base::DoNothing()));

    const user_manager::User* user =
        user_manager::UserManager::Get()->GetPrimaryUser();
    Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
    // Transfer auth cache to the active user's profile so that there is no need
    // to enter them again after unlocking the device.
    ProfileAuthData::TransferHttpAuthCacheProxyEntries(
        base::DoNothing(), webview_storage_partition,
        profile->GetDefaultStoragePartition());
  }
}

void LockScreenStartReauthDialog::HttpAuthDialogShown(
    content::WebContents* web_contents) {
  if (!Matches(web_contents)) {
    return;
  }
  is_proxy_auth_in_progress_ = true;
}

void LockScreenStartReauthDialog::HttpAuthDialogCancelled(
    content::WebContents* web_contents) {
  if (!Matches(web_contents)) {
    return;
  }
  ReenableNetworkUpdates();
  should_reload_gaia_ = true;
  // If proxy authentication is canceled we disconnect from current network
  // and it triggers offline state which leads to us showing network screen
  // through `LockScreenStartReauthDialog::UpdateState`.
  const std::string network_path =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork()->path();
  NetworkHandler::Get()->network_connection_handler()->DisconnectNetwork(
      network_path, base::DoNothing(), network_handler::ErrorCallback());
}

void LockScreenStartReauthDialog::HttpAuthDialogSupplied(
    content::WebContents* web_contents) {
  if (!Matches(web_contents)) {
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LockScreenStartReauthDialog::ReenableNetworkUpdates,
                     weak_factory_.GetWeakPtr()),
      kProxyAuthTimeout);

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LockScreenStartReauthDialog::TransferHttpAuthCaches,
                     weak_factory_.GetWeakPtr()),
      kAuthCacheTransferDelayMs);
  Focus();
}

bool LockScreenStartReauthDialog::Matches(content::WebContents* web_contents) {
  // Check that notification source is related to this dialog's web contents.
  // Otherwise we might falsely react to notifications from chrome tabs which
  // are open in the user's active session. We use NavigationController objects
  // for comparison because `LoginHandler` uses them as the source of
  // proxy-related notifications.
  return base::Contains(webui()->GetWebContents()->GetInnerWebContents(),
                        web_contents);
}

void LockScreenStartReauthDialog::ReenableNetworkUpdates() {
  is_proxy_auth_in_progress_ = false;
}

}  // namespace ash
