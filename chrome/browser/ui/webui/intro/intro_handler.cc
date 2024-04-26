// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/intro_handler.h"

#include "base/cancelable_callback.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/webui/intro/intro_ui.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/core_account_id.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
policy::CloudPolicyStore* GetCloudPolicyStore() {
  auto* machine_level_manager = g_browser_process->browser_policy_connector()
                                    ->machine_level_user_cloud_policy_manager();

  return machine_level_manager ? machine_level_manager->core()->store()
                               : nullptr;
}

// PolicyStoreState will make it easier to handle all the states in a single
// callback.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PolicyStoreState {
  // Store was already loaded when we attached the observer.
  kSuccessAlreadyLoaded = 0,
  // Store has been loaded before the time delay ends.
  kSuccess = 1,
  // Store did not load in time.
  kTimeout = 2,
  // OnStoreError called.
  kStoreError = 3,
  // Store is null for a managed device.
  kStoreNull = 4,
  kMaxValue = kStoreNull,
};

void RecordDisclaimerMetrics(PolicyStoreState state,
                             base::TimeTicks start_time) {
  base::UmaHistogramEnumeration("ProfilePicker.FirstRun.PolicyStoreState",
                                state);
  if (state == PolicyStoreState::kSuccess) {
    base::UmaHistogramTimes(
        "ProfilePicker.FirstRun.OrganizationAvailableTiming",
        /*sample*/ base::TimeTicks::Now() - start_time);
  }
}

class PolicyStoreObserver : public policy::CloudPolicyStore::Observer {
 public:
  explicit PolicyStoreObserver(
      base::OnceCallback<void(std::string)> handle_policy_store_change)
      : handle_policy_store_change_(std::move(handle_policy_store_change)) {
    DCHECK(handle_policy_store_change_);
    start_time_ = base::TimeTicks::Now();

    // Update the disclaimer directly if the policy store is already loaded.
    auto* policy_store = GetCloudPolicyStore();

    // GetCloudPolicyStore will return nullptr for managed devices with
    // non-branded builds because the machine level cloud policy manager will be
    // null while the device is still managed. In that case, we show a generic
    // disclaimer.
    if (!policy_store) {
      // The device is not enrolled in Chrome Browser Cloud Management
      HandlePolicyStoreStatusChange(PolicyStoreState::kStoreNull);
      return;
    }

    if (policy_store->is_initialized()) {
      HandlePolicyStoreStatusChange(PolicyStoreState::kSuccessAlreadyLoaded);
      return;
    }

    policy_store_observation_.Observe(policy_store);
    // 2.5 is the chrome logo animation time which is 1.5s plus the maximum
    // delay of 1s that we are willing to wait for.
    constexpr auto kMaximumEnterpriseDisclaimerDelay = base::Seconds(2.5);
    on_organization_fetch_timeout_.Reset(
        base::BindOnce(&PolicyStoreObserver::OnOrganizationFetchTimeout,
                       base::Unretained(this)));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, on_organization_fetch_timeout_.callback(),
        kMaximumEnterpriseDisclaimerDelay);
  }

  ~PolicyStoreObserver() override = default;

  // policy::CloudPolicyStore::Observer:
  void OnStoreLoaded(policy::CloudPolicyStore* store) override {
    on_organization_fetch_timeout_.Cancel();
    policy_store_observation_.Reset();
    HandlePolicyStoreStatusChange(PolicyStoreState::kSuccess);
  }

  void OnStoreError(policy::CloudPolicyStore* store) override {
    on_organization_fetch_timeout_.Cancel();
    policy_store_observation_.Reset();
    HandlePolicyStoreStatusChange(PolicyStoreState::kStoreError);
  }

  // Called when the delay specified for the store to load has passed. We show a
  // generic disclaimer when this happens.
  void OnOrganizationFetchTimeout() {
    policy_store_observation_.Reset();
    HandlePolicyStoreStatusChange(PolicyStoreState::kTimeout);
  }

  void HandlePolicyStoreStatusChange(PolicyStoreState state) {
    RecordDisclaimerMetrics(state, start_time_);
    std::string managed_device_disclaimer;
    if (state == PolicyStoreState::kSuccess ||
        state == PolicyStoreState::kSuccessAlreadyLoaded) {
      std::optional<std::string> manager = chrome::GetDeviceManagerIdentity();
      managed_device_disclaimer =
          manager->empty()
              ? l10n_util::GetStringUTF8(IDS_FRE_MANAGED_DESCRIPTION)
              : l10n_util::GetStringFUTF8(IDS_FRE_MANAGED_BY_DESCRIPTION,
                                          base::UTF8ToUTF16(*manager));
    } else {
      managed_device_disclaimer =
          l10n_util::GetStringUTF8(IDS_FRE_MANAGED_DESCRIPTION);
    }
    std::move(handle_policy_store_change_).Run(managed_device_disclaimer);
  }

 private:
  base::ScopedObservation<policy::CloudPolicyStore,
                          policy::CloudPolicyStore::Observer>
      policy_store_observation_{this};
  base::OnceCallback<void(std::string)> handle_policy_store_change_;
  base::CancelableOnceCallback<void()> on_organization_fetch_timeout_;
  base::TimeTicks start_time_;
};
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class IdentityManagerObserver : public signin::IdentityManager::Observer {
 public:
  explicit IdentityManagerObserver(
      base::RepeatingCallback<void()> handle_identity_manager_change,
      signin::IdentityManager& identity_manager)
      : account_id_(identity_manager
                        .GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                        .account_id) {
    DCHECK(handle_identity_manager_change);

    identity_manager_observation_.Observe(&identity_manager);
    handle_identity_manager_change_ = std::move(handle_identity_manager_change);
  }

  void OnExtendedAccountInfoUpdated(const AccountInfo& account_info) override {
    if (account_info.account_id != account_id_) {
      return;
    }

    handle_identity_manager_change_.Run();
    if (!account_info.account_image.IsEmpty() && account_info.IsValid()) {
      identity_manager_observation_.Reset();
    }
  }

 private:
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  base::RepeatingCallback<void()> handle_identity_manager_change_;
  const CoreAccountId account_id_;
};

std::string GetPictureUrl(content::WebUI& web_ui,
                          const AccountInfo& account_info) {
  const int avatar_size = 100;
  const int avatar_icon_size = avatar_size * web_ui.GetDeviceScaleFactor();

  DCHECK(!account_info.IsEmpty());
  gfx::Image icon = account_info.account_image.IsEmpty()
                        ? ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                              profiles::GetPlaceholderAvatarIconResourceID())
                        : account_info.account_image;

  return webui::GetBitmapDataUrl(
      profiles::GetSizedAvatarIcon(icon, avatar_icon_size, avatar_icon_size)
          .AsBitmap());
}

std::string GetLacrosIntroWelcomeTitle(const AccountInfo& account_info) {
  const bool has_given_name = !account_info.given_name.empty();
  base::UmaHistogramBoolean("Profile.LacrosFre.WelcomeHasGivenName",
                            has_given_name);
  return has_given_name ? l10n_util::GetStringFUTF8(
                              IDS_PRIMARY_PROFILE_FIRST_RUN_TITLE,
                              base::UTF8ToUTF16(account_info.given_name))
                        : l10n_util::GetStringUTF8(
                              IDS_PRIMARY_PROFILE_FIRST_RUN_NO_NAME_TITLE);
}

std::string GetLacrosIntroManagementDisclaimer(
    const Profile& profile,
    const std::string& account_domain_name) {
  // TODO(crbug.com/40256886): Fix logic mismatch in device/account management
  // between Lacros and DICE.
  const bool is_managed_account =
      profile.GetProfilePolicyConnector()->IsManaged();
  if (!is_managed_account || account_domain_name == kNoHostedDomainFound) {
    return std::string();
  }
  return l10n_util::GetStringFUTF8(
      IDS_PRIMARY_PROFILE_FIRST_RUN_SESSION_MANAGED_BY_DESCRIPTION,
      base::UTF8ToUTF16(account_domain_name));
}

base::Value::Dict GetProfileInfoValue(content::WebUI& web_ui) {
  auto* profile = Profile::FromWebUI(&web_ui);

  const auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  const CoreAccountInfo core_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(
          core_account_info.account_id);

  if (account_info.email.empty()) {
    return base::Value::Dict();
  }

  return base::Value::Dict()
      .Set("pictureUrl", GetPictureUrl(web_ui, account_info))
      .Set("managementDisclaimer", GetLacrosIntroManagementDisclaimer(
                                       *profile, account_info.hosted_domain))
      .Set("title", GetLacrosIntroWelcomeTitle(account_info))
      .Set("subtitle",
           l10n_util::GetStringFUTF8(IDS_PRIMARY_PROFILE_FIRST_RUN_SUBTITLE,
                                     base::UTF8ToUTF16(account_info.email)));
}
#endif
}  // namespace

IntroHandler::IntroHandler(
    base::RepeatingCallback<void(IntroChoice)> intro_callback,
    base::OnceCallback<void(DefaultBrowserChoice)> default_browser_callback,
    bool is_device_managed)
    : intro_callback_(std::move(intro_callback)),
      default_browser_callback_(std::move(default_browser_callback)),
      is_device_managed_(is_device_managed) {
  DCHECK(intro_callback_);
  DCHECK(default_browser_callback_);
}

IntroHandler::~IntroHandler() = default;

void IntroHandler::RegisterMessages() {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  web_ui()->RegisterMessageCallback(
      "continueWithoutAccount",
      base::BindRepeating(&IntroHandler::HandleContinueWithoutAccount,
                          base::Unretained(this)));
#endif
  web_ui()->RegisterMessageCallback(
      "continueWithAccount",
      base::BindRepeating(&IntroHandler::HandleContinueWithAccount,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initializeMainView",
      base::BindRepeating(&IntroHandler::HandleInitializeMainView,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setAsDefaultBrowser",
      base::BindRepeating(&IntroHandler::HandleSetAsDefaultBrowser,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "skipDefaultBrowser",
      base::BindRepeating(&IntroHandler::HandleSkipDefaultBrowser,
                          base::Unretained(this)));
}

void IntroHandler::OnJavascriptAllowed() {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (!is_device_managed_) {
    return;
  }
  policy_store_observer_ = std::make_unique<PolicyStoreObserver>(base::BindOnce(
      &IntroHandler::FireManagedDisclaimerUpdate, base::Unretained(this)));
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  identity_manager_observer_ = std::make_unique<IdentityManagerObserver>(
      base::BindRepeating(&IntroHandler::UpdateProfileInfo,
                          base::Unretained(this)),
      *IdentityManagerFactory::GetForProfile(Profile::FromWebUI(web_ui())));

  UpdateProfileInfo();
#endif
}

void IntroHandler::HandleContinueWithAccount(const base::Value::List& args) {
  CHECK(args.empty());
  intro_callback_.Run(IntroChoice::kContinueWithAccount);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void IntroHandler::HandleContinueWithoutAccount(const base::Value::List& args) {
  CHECK(args.empty());
  intro_callback_.Run(IntroChoice::kContinueWithoutAccount);
}

void IntroHandler::ResetIntroButtons() {
  if (IsJavascriptAllowed()) {
    FireWebUIListener("reset-intro-buttons");
  }
}
#endif

void IntroHandler::HandleInitializeMainView(const base::Value::List& args) {
  CHECK(args.empty());
  AllowJavascript();
}

void IntroHandler::HandleSetAsDefaultBrowser(const base::Value::List& args) {
  CHECK(args.empty());
  if (default_browser_callback_) {
    std::move(default_browser_callback_)
        .Run(DefaultBrowserChoice::kClickSetAsDefault);
  }
}

void IntroHandler::HandleSkipDefaultBrowser(const base::Value::List& args) {
  CHECK(args.empty());
  if (default_browser_callback_) {
    std::move(default_browser_callback_).Run(DefaultBrowserChoice::kSkip);
  }
}

void IntroHandler::ResetDefaultBrowserButtons() {
  if (IsJavascriptAllowed()) {
    FireWebUIListener("reset-default-browser-buttons");
  }
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void IntroHandler::UpdateProfileInfo() {
  DCHECK(IsJavascriptAllowed());
  FireWebUIListener("on-profile-info-changed", GetProfileInfoValue(*web_ui()));
}
#endif

void IntroHandler::FireManagedDisclaimerUpdate(std::string disclaimer) {
  DCHECK(is_device_managed_);
  if (IsJavascriptAllowed()) {
    FireWebUIListener("managed-device-disclaimer-updated",
                      base::Value(std::move(disclaimer)));
  }
}
