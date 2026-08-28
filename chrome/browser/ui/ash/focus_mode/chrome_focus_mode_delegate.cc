// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/focus_mode/chrome_focus_mode_delegate.h"

#include "ash/shell.h"
#include "ash/system/focus_mode/sounds/youtube_music/request_signer.h"
#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_client.h"
#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/focus_mode/signature_builder.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/signin/identity_manager_provider.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_id.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace {

// Force Focus Mode to report an old version string for testing.
BASE_FEATURE(kFocusModeOldVersion, base::FEATURE_DISABLED_BY_DEFAULT);

// Amount of time between now and certificate expiration which will trigger a
// refresh. Certificates are generally issued with lifetimes of about 1 year so
// this won't trigger too often but will refresh a certificate well in advance
// of it expiring.
constexpr base::TimeDelta kCertificateBuffer = base::Hours(36);

// This will be used as a callback that is passed to the youtube music client to
// generate `google_apis::RequestSender`. The request sender class implements
// the OAuth 2.0 protocol, so we do not need to do anything extra for
// authentication and authorization.
//   `account_id`:
//     The fixed user this YouTubeMusicClient instance was created for; bound
//     by CreateYouTubeMusicClient() below.
//   `traffic_annotation_tag`:
//     It documents the network request for system admins and regulators.
std::unique_ptr<google_apis::RequestSender> CreateRequestSenderForYouTubeMusic(
    const AccountId& account_id,
    const net::NetworkTrafficAnnotationTag& traffic_annotation_tag) {
  signin::IdentityManager* identity_manager =
      ash::IdentityManagerProvider::Get().Find(account_id);
  Profile& profile = CHECK_DEREF(Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          account_id)));
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      profile.GetURLLoaderFactory();

  std::unique_ptr<google_apis::AuthService> auth_service =
      std::make_unique<google_apis::AuthService>(
          identity_manager,
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
          url_loader_factory,
          signin::OAuthConsumerId::kYouTubeMusic);
  return std::make_unique<google_apis::RequestSender>(
      std::move(auth_service), url_loader_factory,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(),
           /* `USER_VISIBLE` is because the requested/returned data is visible
              to the user on System UI surfaces. */
           base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
      /*custom_user_agent=*/std::string(), traffic_annotation_tag);
}

class RequestSignerImpl : public ash::RequestSigner {
 public:
  explicit RequestSignerImpl(const AccountId& account_id,
                             const std::string& device_id)
      : account_id_(account_id), device_id_(device_id) {
    certificate_manager_ =
        CertificateManager::Create(account_id, kCertificateBuffer);
    signature_builder_ =
        std::make_unique<SignatureBuilder>(certificate_manager_.get());
  }
  ~RequestSignerImpl() override = default;

  bool GenerateHeaders(base::span<const uint8_t> data,
                       HeadersCallback callback) override {
    signature_builder_->SetPayload(
        std::vector<uint8_t>(data.begin(), data.end()));
    PrepareDeviceInfo();

    if (!signature_builder_->BuildHeaders(std::move(callback))) {
      LOG(WARNING) << "Unable to start request signing";
      return false;
    }
    return true;
  }

  std::string DeviceInfoHeader() override {
    PrepareDeviceInfo();
    return signature_builder_->DeviceInfoHeader();
  }

 private:
  void PrepareDeviceInfo() {
    signature_builder_->SetBrand("ChromeOS");
    signature_builder_->SetModel("Chromebook");
    if (!base::FeatureList::IsEnabled(kFocusModeOldVersion)) {
      signature_builder_->SetSoftwareVersion(version_info::GetVersionNumber());
    } else {
      // Report a previous stable version to verify minimum version behavior.
      signature_builder_->SetSoftwareVersion("129.0.6668.112");
    }
    signature_builder_->SetDeviceId(device_id_);
  }

  const AccountId account_id_;
  const std::string device_id_;

  std::unique_ptr<CertificateManager> certificate_manager_;
  std::unique_ptr<SignatureBuilder> signature_builder_;
};

}  // namespace

ChromeFocusModeDelegate::ChromeFocusModeDelegate() = default;

ChromeFocusModeDelegate::~ChromeFocusModeDelegate() = default;

std::unique_ptr<ash::youtube_music::YouTubeMusicClient>
ChromeFocusModeDelegate::CreateYouTubeMusicClient(
    const AccountId& account_id,
    const std::string& device_id) {
  return std::make_unique<ash::youtube_music::YouTubeMusicClient>(
      base::BindRepeating(&CreateRequestSenderForYouTubeMusic, account_id),
      std::make_unique<RequestSignerImpl>(account_id, device_id));
}

const std::string& ChromeFocusModeDelegate::GetLocale() {
  return g_browser_process->GetApplicationLocale();
}

bool ChromeFocusModeDelegate::IsMinorUser() {
  // TODO(crbug.com/546860700): ash::FocusModeDelegate::IsMinorUser() takes no
  // AccountId, so this assumes the active user; plumb an AccountId through
  // from the caller instead once one is available there.
  user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (!active_user) {
    return false;
  }
  const AccountId& account_id = active_user->GetAccountId();
  signin::IdentityManager* identity_manager =
      ash::IdentityManagerProvider::Get().Find(account_id);
  if (!identity_manager) {
    // Identity manager is not available (e.g:guest mode).
    return false;
  }

  GaiaId gaia_id = account_id.GetGaiaId();
  const AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByGaiaId(gaia_id);
  // TODO(b/366042251): Update minor targeting to use a better signal.
  return account_info.GetAccountCapabilities().can_use_manta_service() !=
         signin::Tribool::kTrue;
}
