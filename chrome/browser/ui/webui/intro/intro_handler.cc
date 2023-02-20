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
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/webui/intro/intro_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

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
      // TODO(crbug.com/1409028): Remove `.value_or` when we modify
      // `GetDeviceManagerIdentity()` to return an empty string instead of a
      // nullopt when we know that the device is managed.
      absl::optional<std::string> manager =
          chrome::GetDeviceManagerIdentity().value_or(std::string());
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
}  // namespace

IntroHandler::IntroHandler(base::RepeatingCallback<void(IntroChoice)> callback,
                           bool is_device_managed)
    : callback_(std::move(callback)), is_device_managed_(is_device_managed) {
  DCHECK(callback_);
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
}

void IntroHandler::OnJavascriptAllowed() {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (!is_device_managed_) {
    return;
  }
  policy_store_observer_ = std::make_unique<PolicyStoreObserver>(base::BindOnce(
      &IntroHandler::FireManagedDisclaimerUpdate, base::Unretained(this)));
#endif
}

void IntroHandler::HandleContinueWithAccount(const base::Value::List& args) {
  CHECK(args.empty());
  callback_.Run(IntroChoice::kContinueWithAccount);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void IntroHandler::HandleContinueWithoutAccount(const base::Value::List& args) {
  CHECK(args.empty());
  callback_.Run(IntroChoice::kContinueWithoutAccount);
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

void IntroHandler::FireManagedDisclaimerUpdate(std::string disclaimer) {
  DCHECK(is_device_managed_);
  if (IsJavascriptAllowed()) {
    FireWebUIListener("managed-device-disclaimer-updated",
                      base::Value(std::move(disclaimer)));
  }
}
