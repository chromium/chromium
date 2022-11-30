// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_proxy_backend.h"
#include <memory>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/functional/callback_forward.h"
#include "base/functional/identity.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "components/password_manager/core/browser/field_info_table.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/browser/password_store_backend_error.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace password_manager {

namespace {

using sync_util::IsPasswordSyncEnabled;

bool ShouldExecuteModifyOperationsOnShadowBackend(PrefService* prefs,
                                                  bool is_syncing) {
  // TODO(crbug.com/1306001): Reenable or clean up for local-only users.
  return false;
}

bool ShouldExecuteReadOperationsOnShadowBackend(PrefService* prefs,
                                                bool is_syncing) {
  if (ShouldExecuteModifyOperationsOnShadowBackend(prefs, is_syncing)) {
    // Read operations are always allowed whenever modifications are allowed.
    // i.e. necessary migrations have happened and appropriate flags are set.
    return true;
  }

  if (!is_syncing)
    return false;

  if (!base::FeatureList::IsEnabled(features::kUnifiedPasswordManagerAndroid))
    return false;

  features::UpmExperimentVariation variation =
      features::kUpmExperimentVariationParam.Get();
  switch (variation) {
    case features::UpmExperimentVariation::kEnableForSyncingUsers:
    case features::UpmExperimentVariation::kEnableOnlyBackendForSyncingUsers:
    case features::UpmExperimentVariation::kEnableForAllUsers:
      return false;
    case features::UpmExperimentVariation::kShadowSyncingUsers:
      return true;
  }
  NOTREACHED() << "Define explicitly whether shadow traffic is recorded!";
  return false;
}

bool ShouldExecuteDeletionsOnShadowBackend(PrefService* prefs,
                                           bool is_syncing) {
  if (ShouldExecuteModifyOperationsOnShadowBackend(prefs, is_syncing))
    return true;

  if (!is_syncing)
    return false;

  if (!base::FeatureList::IsEnabled(features::kUnifiedPasswordManagerAndroid))
    return false;

  features::UpmExperimentVariation variation =
      features::kUpmExperimentVariationParam.Get();
  switch (variation) {
    case features::UpmExperimentVariation::kEnableForSyncingUsers:
    case features::UpmExperimentVariation::kEnableOnlyBackendForSyncingUsers:
      return true;
    case features::UpmExperimentVariation::kEnableForAllUsers:
      // All passwords are in the remote storage. There should not be a
      // shadow backend anymore.
      return false;
    case features::UpmExperimentVariation::kShadowSyncingUsers:
      return false;
  }
  NOTREACHED()
      << "Define explicitly whether deletions on both backends are required!";
  return false;
}

bool ShouldErrorResultInFallback(PasswordStoreBackendError error) {
  switch (error.recovery_type) {
    case PasswordStoreBackendErrorRecoveryType::kUnrecoverable:
    case PasswordStoreBackendErrorRecoveryType::kUnspecified:
      return true;
    case PasswordStoreBackendErrorRecoveryType::kRetriable:
    case PasswordStoreBackendErrorRecoveryType::kRecoverable:
      return false;
  }
}

bool ShouldFallbackOnUserAffectingOperations() {
  return password_manager::features::kFallbackOnUserAffectingReadOperations
      .Get();
}

bool ShouldFallbackOnNonUserAffectingOperations() {
  return password_manager::features::kFallbackOnNonUserAffectingReadOperations
      .Get();
}

bool ShouldFallbackOnModifyingOperations() {
  return password_manager::features::kFallbackOnModifyingOperations.Get();
}

bool ShouldFallbackOnRemoveOperations() {
  return password_manager::features::kFallbackOnRemoveOperations.Get();
}

bool IsBuiltInBackendSyncEnabled() {
  DCHECK(
      base::FeatureList::IsEnabled(features::kUnifiedPasswordManagerAndroid));

  features::UpmExperimentVariation variation =
      features::kUpmExperimentVariationParam.Get();
  switch (variation) {
    case features::UpmExperimentVariation::kEnableForSyncingUsers:
    case features::UpmExperimentVariation::kShadowSyncingUsers:
    case features::UpmExperimentVariation::kEnableOnlyBackendForSyncingUsers:
      return true;
    case features::UpmExperimentVariation::kEnableForAllUsers:
      return false;
  }
  NOTREACHED() << "Define which backend handles sync change callbacks!";
  return false;
}

void CallOnSyncEnabledOrDisabledForEnabledBackend(
    bool originates_from_android,
    base::RepeatingClosure sync_enabled_or_disabled_cb) {
  if (IsBuiltInBackendSyncEnabled()) {
    if (!originates_from_android) {
      sync_enabled_or_disabled_cb.Run();
    }
    return;
  }
  sync_enabled_or_disabled_cb.Run();
}

using MethodName = base::StrongAlias<struct MethodNameTag, std::string>;

struct LoginsResultImpl {
  using ResultType = LoginsResult;
  using ElementsType = LoginsResult;

  static LoginsResult* GetElements(LoginsResult& logins) { return &logins; }

  static std::unique_ptr<PasswordForm> Clone(
      const std::unique_ptr<PasswordForm>& login) {
    return std::make_unique<PasswordForm>(*login);
  }

  static bool IsLess(const std::unique_ptr<PasswordForm>& lhs,
                     const std::unique_ptr<PasswordForm>& rhs) {
    return PasswordFormUniqueKey(*lhs) < PasswordFormUniqueKey(*rhs);
  }

  static bool HaveInconsistentPasswords(
      const std::unique_ptr<PasswordForm>& lhs,
      const std::unique_ptr<PasswordForm>& rhs) {
    return lhs->password_value != rhs->password_value;
  }
};

struct LoginsResultOrErrorImpl {
  using ResultType = LoginsResultOrError;
  using ElementsType = LoginsResult;

  static LoginsResult* GetElements(LoginsResultOrError& logins_or_error) {
    return absl::holds_alternative<PasswordStoreBackendError>(logins_or_error)
               ? nullptr
               : &absl::get<LoginsResult>(logins_or_error);
  }

  static std::unique_ptr<PasswordForm> Clone(
      const std::unique_ptr<PasswordForm>& login) {
    return std::make_unique<PasswordForm>(*login);
  }

  static bool IsLess(const std::unique_ptr<PasswordForm>& lhs,
                     const std::unique_ptr<PasswordForm>& rhs) {
    return PasswordFormUniqueKey(*lhs) < PasswordFormUniqueKey(*rhs);
  }

  static bool HaveInconsistentPasswords(
      const std::unique_ptr<PasswordForm>& lhs,
      const std::unique_ptr<PasswordForm>& rhs) {
    return lhs->password_value != rhs->password_value;
  }
};

// An `ApiMethodImpl` for `ShadowTrafficMetricsRecorder` implementing support
// for the database modification methods returning `PasswordChangesOrError`.
struct PasswordChangesOrErrorImpl {
  using ResultType = PasswordChangesOrError;
  using ElementsType = PasswordStoreChangeList;

  static const PasswordStoreChangeList* GetElements(
      const PasswordChangesOrError& changelist_or_error) {
    if (absl::holds_alternative<PasswordStoreBackendError>(changelist_or_error))
      return nullptr;
    const PasswordChanges& changelist =
        absl::get<PasswordChanges>(changelist_or_error);

    return changelist.has_value() ? &changelist.value() : nullptr;
  }

  static PasswordStoreChange Clone(const PasswordStoreChange& change) {
    return change;
  }

  static bool IsLess(const PasswordStoreChange& lhs,
                     const PasswordStoreChange& rhs) {
    return std::forward_as_tuple(PasswordFormUniqueKey(lhs.form()),
                                 lhs.type()) <
           std::forward_as_tuple(PasswordFormUniqueKey(rhs.form()), rhs.type());
  }

  static bool HaveInconsistentPasswords(const PasswordStoreChange& lhs,
                                        const PasswordStoreChange& rhs) {
    // We never consider PasswordStoreChange having inconsistent passwords.
    return false;
  }
};

void InvokeCallbackWithCombinedStatus(base::OnceCallback<void(bool)> completion,
                                      std::vector<bool> statuses) {
  std::move(completion).Run(base::ranges::all_of(statuses, base::identity()));
}

// Records the difference metrics between |main_result| and |backend_result|
// when returned by |method_name|. |main_result| and |backend_result| must be
// vectors of type T. |is_less| can be used to compare two objects of type T.
// |is_inconsistent| can be used to compute if the two objects have inconsistent
// password values.
template <typename T, typename IsLess, typename IsInconsistent>
void RecordMetrics(const MethodName& method_name,
                   const std::vector<T>& main_result,
                   const std::vector<T>& backend_result,
                   IsLess is_less,
                   IsInconsistent is_inconsistent) {
  // Comparison is done by creating two sets that contain pointers to the
  // objects stored in |main_result| and |backend_result|. Using the passed
  // comparison methods, we compute and report metrics regarding the difference
  // between both result vectors.
  auto is_less_ptr = [is_less](const T* lhs, const T* rhs) {
    return is_less(*lhs, *rhs);
  };

  auto address_of = [](const T& object) { return &object; };

  auto main_elements =
      base::MakeFlatSet<const T*>(main_result, is_less_ptr, address_of);
  auto shadow_elements =
      base::MakeFlatSet<const T*>(backend_result, is_less_ptr, address_of);

  auto common_elements = [&] {
    std::vector<const T*> vec;
    vec.reserve(main_elements.size());
    base::ranges::set_intersection(main_elements, shadow_elements,
                                   std::back_inserter(vec), is_less_ptr);
    return base::flat_set<const T*, decltype(is_less_ptr)>(std::move(vec),
                                                           is_less_ptr);
  }();

  // The cardinalities from which we compute the metrics.
  size_t main_minus_shadow = main_elements.size() - common_elements.size();
  size_t shadow_minus_main = shadow_elements.size() - common_elements.size();
  size_t diff = main_minus_shadow + shadow_minus_main;
  size_t total = diff + common_elements.size();
  size_t inconsistent = base::ranges::count_if(common_elements, [&](auto* f) {
    auto lhs = main_elements.find(f);
    auto rhs = shadow_elements.find(f);
    DCHECK(lhs != main_elements.end());
    DCHECK(rhs != shadow_elements.end());
    return (*is_inconsistent)(**lhs, **rhs);
  });

  // Emits a pair of absolute and relative metrics.
  auto Emit = [&method_name](base::StringPiece metric_infix, size_t nominator,
                             size_t denominator) {
    std::string prefix =
        base::StrCat({"PasswordManager.PasswordStoreProxyBackend.",
                      method_name.value(), ".", metric_infix, "."});
    base::UmaHistogramCounts1M(prefix + "Abs", nominator);
    if (denominator != 0) {
      size_t ceiling_of_percentage =
          (nominator * 100 + denominator - 1) / denominator;
      base::UmaHistogramPercentage(prefix + "Rel", ceiling_of_percentage);
    }
  };
  Emit("Diff", diff, total);
  Emit("MainMinusShadow", main_minus_shadow, total);
  Emit("ShadowMinusMain", shadow_minus_main, total);
  Emit("InconsistentPasswords", inconsistent, common_elements.size());
}

// Records the metrics of a pair of MethodName calls to the main and
// the shadow backends once both calls are finished. MethodName() is expected to
// return an std::vector<ApiMethodImpl::ResultType>. ApiMethodImpl classes need
// to provide 4 methods:
// - GetElements(): returns the elements to be compared
// - Clone(): Returns a copy of an element, used to cache the main results.
// - IsLess(): to compare elements.
// - HaveInconsistentPasswords(): Whether elements have inconsistent passwords
//
// The class is ref-counted because it is equally owned by the two parallel
// method calls : it must outlive the first returning one and shall  be
// destroyed after the second one returns.
template <typename ApiMethodImpl>
class ShadowTrafficMetricsRecorder
    : public base::RefCounted<ShadowTrafficMetricsRecorder<ApiMethodImpl>> {
 public:
  explicit ShadowTrafficMetricsRecorder(MethodName method_name)
      : method_name_(std::move(method_name)) {}

  // Returns the unchanged |result| so it can be passed to the main handler.
  typename ApiMethodImpl::ResultType RecordMainResult(
      typename ApiMethodImpl::ResultType result) {
    if (auto* elements = ApiMethodImpl::GetElements(result)) {
      if (!first_result_) {
        first_result_ =
            absl::make_optional<typename ApiMethodImpl::ElementsType>();
        first_result_->reserve(elements->size());
        for (const auto& e : *elements)
          first_result_->push_back(ApiMethodImpl::Clone(e));
      } else {
        RecordMetrics(method_name_, /*main_result=*/*elements,
                      /*shadow_result=*/*first_result_, &ApiMethodImpl::IsLess,
                      &ApiMethodImpl::HaveInconsistentPasswords);
      }
    }

    return result;
  }

  void RecordShadowResult(typename ApiMethodImpl::ResultType result) {
    if (auto* elements = ApiMethodImpl::GetElements(result)) {
      if (!first_result_) {
        first_result_ = std::move(*elements);
      } else {
        RecordMetrics(method_name_,
                      /*main_result=*/*first_result_,
                      /*shadow_result=*/*elements, &ApiMethodImpl::IsLess,
                      &ApiMethodImpl::HaveInconsistentPasswords);
      }
    }
  }

 private:
  friend class base::RefCounted<ShadowTrafficMetricsRecorder<ApiMethodImpl>>;
  ~ShadowTrafficMetricsRecorder() = default;

  // Stores the result of the backend that returns first.
  absl::optional<typename ApiMethodImpl::ElementsType> first_result_;
  const MethodName method_name_;
};

std::string GetFallbackMetricNameForMethod(const MethodName& method_name) {
  return base::StrCat({"PasswordManager.PasswordStoreProxyBackend.",
                       method_name.value(), ".Fallback"});
}

}  // namespace

PasswordStoreProxyBackend::PasswordStoreProxyBackend(
    PasswordStoreBackend* built_in_backend,
    PasswordStoreBackend* android_backend,
    PrefService* prefs)
    : built_in_backend_(built_in_backend),
      android_backend_(android_backend),
      prefs_(prefs) {}

PasswordStoreProxyBackend::~PasswordStoreProxyBackend() = default;

void PasswordStoreProxyBackend::InitBackend(
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  base::RepeatingCallback<void(bool)> pending_initialization_calls =
      base::BarrierCallback<bool>(
          /*num_callbacks=*/2, base::BindOnce(&InvokeCallbackWithCombinedStatus,
                                              std::move(completion)));

  // Both backends need to be initialized, so using the helpers for main/shadow
  // backend is unnecessary and won't work since the sync status may not be
  // available yet.
  built_in_backend_->InitBackend(
      base::BindRepeating(
          &PasswordStoreProxyBackend::OnRemoteFormChangesReceived,
          weak_ptr_factory_.GetWeakPtr(),
          CallbackOriginatesFromAndroidBackend(false),
          remote_form_changes_received),
      base::BindRepeating(&CallOnSyncEnabledOrDisabledForEnabledBackend,
                          /*originates_from_android=*/false,
                          sync_enabled_or_disabled_cb),
      base::BindOnce(pending_initialization_calls));

  android_backend_->InitBackend(
      base::BindRepeating(
          &PasswordStoreProxyBackend::OnRemoteFormChangesReceived,
          weak_ptr_factory_.GetWeakPtr(),
          CallbackOriginatesFromAndroidBackend(true),
          std::move(remote_form_changes_received)),
      base::BindRepeating(&CallOnSyncEnabledOrDisabledForEnabledBackend,
                          /*originates_from_android=*/true,
                          std::move(sync_enabled_or_disabled_cb)),
      base::BindOnce(pending_initialization_calls));
}

void PasswordStoreProxyBackend::Shutdown(base::OnceClosure shutdown_completed) {
  base::RepeatingClosure pending_shutdown_calls = base::BarrierClosure(
      /*num_closures=*/2, std::move(shutdown_completed));
  android_backend_->Shutdown(pending_shutdown_calls);
  built_in_backend_->Shutdown(pending_shutdown_calls);
}

void PasswordStoreProxyBackend::GetAllLoginsAsync(LoginsOrErrorReply callback) {
  auto handler = base::MakeRefCounted<
      ShadowTrafficMetricsRecorder<LoginsResultOrErrorImpl>>(
      MethodName("GetAllLoginsAsync"));

  LoginsOrErrorReply result_callback;
  if (UsesAndroidBackendAsMainBackend() &&
      ShouldFallbackOnNonUserAffectingOperations()) {
    auto execute_on_built_in_backend =
        base::BindOnce(&PasswordStoreBackend::GetAllLoginsAsync,
                       base::Unretained(built_in_backend_));
    result_callback = base::BindOnce(
        &PasswordStoreProxyBackend::MaybeRetryOperation<LoginsResultOrError>,
        weak_ptr_factory_.GetWeakPtr(), std::move(execute_on_built_in_backend),
        MethodName("GetAllLoginsAsync"), std::move(callback));
  } else {
    result_callback = std::move(callback);
  }

  main_backend()->GetAllLoginsAsync(
      base::BindOnce(&ShadowTrafficMetricsRecorder<
                         LoginsResultOrErrorImpl>::RecordMainResult,
                     handler)
          .Then(std::move(result_callback)));

  if (ShouldExecuteReadOperationsOnShadowBackend(
          prefs_, IsPasswordSyncEnabled(sync_service_))) {
    shadow_backend()->GetAllLoginsAsync(
        base::BindOnce(&ShadowTrafficMetricsRecorder<
                           LoginsResultOrErrorImpl>::RecordShadowResult,
                       handler));
  }
}

void PasswordStoreProxyBackend::GetAutofillableLoginsAsync(
    LoginsOrErrorReply callback) {
  auto handler = base::MakeRefCounted<
      ShadowTrafficMetricsRecorder<LoginsResultOrErrorImpl>>(
      MethodName("GetAutofillableLoginsAsync"));

  LoginsOrErrorReply result_callback;
  if (UsesAndroidBackendAsMainBackend() &&
      ShouldFallbackOnNonUserAffectingOperations()) {
    auto execute_on_built_in_backend =
        base::BindOnce(&PasswordStoreBackend::GetAutofillableLoginsAsync,
                       base::Unretained(built_in_backend_));
    result_callback = base::BindOnce(
        &PasswordStoreProxyBackend::MaybeRetryOperation<LoginsResultOrError>,
        weak_ptr_factory_.GetWeakPtr(), std::move(execute_on_built_in_backend),
        MethodName("GetAutofillableLoginsAsync"), std::move(callback));
  } else {
    result_callback = std::move(callback);
  }

  main_backend()->GetAutofillableLoginsAsync(
      base::BindOnce(&ShadowTrafficMetricsRecorder<
                         LoginsResultOrErrorImpl>::RecordMainResult,
                     handler)
          .Then(std::move(result_callback)));

  if (ShouldExecuteReadOperationsOnShadowBackend(
          prefs_, IsPasswordSyncEnabled(sync_service_))) {
    shadow_backend()->GetAutofillableLoginsAsync(
        base::BindOnce(&ShadowTrafficMetricsRecorder<
                           LoginsResultOrErrorImpl>::RecordShadowResult,
                       handler));
  }
}

void PasswordStoreProxyBackend::GetAllLoginsForAccountAsync(
    absl::optional<std::string> account,
    LoginsOrErrorReply callback) {
  NOTREACHED();
}

void PasswordStoreProxyBackend::FillMatchingLoginsAsync(
    LoginsOrErrorReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  auto handler = base::MakeRefCounted<
      ShadowTrafficMetricsRecorder<LoginsResultOrErrorImpl>>(
      MethodName("FillMatchingLoginsAsync"));

  LoginsOrErrorReply result_callback;
  if (UsesAndroidBackendAsMainBackend() &&
      ShouldFallbackOnUserAffectingOperations()) {
    // Lambda is used to reorder |FillMatchingLoginsAsync| arguments so all but
    // the |reply_callback| could be binded.
    auto execute_on_built_in_backend = base::BindOnce(
        [](PasswordStoreBackend* backend, bool include_psl,
           const std::vector<PasswordFormDigest>& forms,
           LoginsOrErrorReply reply_callback) {
          backend->FillMatchingLoginsAsync(std::move(reply_callback),
                                           include_psl, forms);
        },
        base::Unretained(built_in_backend_), include_psl, forms);

    result_callback = base::BindOnce(
        &PasswordStoreProxyBackend::MaybeRetryOperation<LoginsResultOrError>,
        weak_ptr_factory_.GetWeakPtr(), std::move(execute_on_built_in_backend),
        MethodName("FillMatchingLoginsAsync"), std::move(callback));
  } else {
    result_callback = std::move(callback);
  }

  main_backend()->FillMatchingLoginsAsync(
      base::BindOnce(&ShadowTrafficMetricsRecorder<
                         LoginsResultOrErrorImpl>::RecordMainResult,
                     handler)
          .Then(std::move(result_callback)),
      include_psl, forms);

  if (ShouldExecuteReadOperationsOnShadowBackend(
          prefs_, IsPasswordSyncEnabled(sync_service_))) {
    shadow_backend()->FillMatchingLoginsAsync(
        base::BindOnce(&ShadowTrafficMetricsRecorder<
                           LoginsResultOrErrorImpl>::RecordShadowResult,
                       handler),
        include_psl, forms);
  }
}

void PasswordStoreProxyBackend::AddLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  auto handler = base::MakeRefCounted<
      ShadowTrafficMetricsRecorder<PasswordChangesOrErrorImpl>>(
      MethodName("AddLoginAsync"));

  PasswordChangesOrErrorReply result_callback;
  if (UsesAndroidBackendAsMainBackend() &&
      ShouldFallbackOnModifyingOperations()) {
    auto execute_on_built_in_backend =
        base::BindOnce(&PasswordStoreBackend::AddLoginAsync,
                       base::Unretained(built_in_backend_), form);
    result_callback = base::BindOnce(
        &PasswordStoreProxyBackend::MaybeRetryOperation<PasswordChangesOrError>,
        weak_ptr_factory_.GetWeakPtr(), std::move(execute_on_built_in_backend),
        MethodName("AddLoginAsync"), std::move(callback));
  } else {
    result_callback = std::move(callback);
  }

  main_backend()->AddLoginAsync(
      form, base::BindOnce(&ShadowTrafficMetricsRecorder<
                               PasswordChangesOrErrorImpl>::RecordMainResult,
                           handler)
                .Then(std::move(result_callback)));
  if (ShouldExecuteModifyOperationsOnShadowBackend(
          prefs_, IsPasswordSyncEnabled(sync_service_))) {
    shadow_backend()->AddLoginAsync(
        form,
        base::BindOnce(&ShadowTrafficMetricsRecorder<
                           PasswordChangesOrErrorImpl>::RecordShadowResult,
                       handler));
  }
}

void PasswordStoreProxyBackend::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  auto handler = base::MakeRefCounted<
      ShadowTrafficMetricsRecorder<PasswordChangesOrErrorImpl>>(
      MethodName("UpdateLoginAsync"));

  PasswordChangesOrErrorReply result_callback;
  if (UsesAndroidBackendAsMainBackend() &&
      ShouldFallbackOnModifyingOperations()) {
    auto execute_on_built_in_backend =
        base::BindOnce(&PasswordStoreBackend::UpdateLoginAsync,
                       base::Unretained(built_in_backend_), form);
    result_callback = base::BindOnce(
        &PasswordStoreProxyBackend::MaybeRetryOperation<PasswordChangesOrError>,
        weak_ptr_factory_.GetWeakPtr(), std::move(execute_on_built_in_backend),
        MethodName("UpdateLoginAsync"), std::move(callback));
  } else {
    result_callback = std::move(callback);
  }

  main_backend()->UpdateLoginAsync(
      form, base::BindOnce(&ShadowTrafficMetricsRecorder<
                               PasswordChangesOrErrorImpl>::RecordMainResult,
                           handler)
                .Then(std::move(result_callback)));
  if (ShouldExecuteModifyOperationsOnShadowBackend(
          prefs_, IsPasswordSyncEnabled(sync_service_))) {
    shadow_backend()->UpdateLoginAsync(
        form,
        base::BindOnce(&ShadowTrafficMetricsRecorder<
                           PasswordChangesOrErrorImpl>::RecordShadowResult,
                       handler));
  }
}

void PasswordStoreProxyBackend::RemoveLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  auto handler = base::MakeRefCounted<
      ShadowTrafficMetricsRecorder<PasswordChangesOrErrorImpl>>(
      MethodName("RemoveLoginAsync"));

  PasswordChangesOrErrorReply result_callback;
  if (UsesAndroidBackendAsMainBackend() && ShouldFallbackOnRemoveOperations()) {
    auto execute_on_built_in_backend =
        base::BindOnce(&PasswordStoreBackend::RemoveLoginAsync,
                       base::Unretained(built_in_backend_), form);
    result_callback = base::BindOnce(
        &PasswordStoreProxyBackend::MaybeRetryOperation<PasswordChangesOrError>,
        weak_ptr_factory_.GetWeakPtr(), std::move(execute_on_built_in_backend),
        MethodName("RemoveLoginAsync"), std::move(callback));
  } else {
    result_callback = std::move(callback);
  }

  main_backend()->RemoveLoginAsync(
      form, base::BindOnce(&ShadowTrafficMetricsRecorder<
                               PasswordChangesOrErrorImpl>::RecordMainResult,
                           handler)
                .Then(std::move(result_callback)));
  if (ShouldExecuteDeletionsOnShadowBackend(
          prefs_, IsPasswordSyncEnabled(sync_service_))) {
    shadow_backend()->RemoveLoginAsync(
        form,
        base::BindOnce(&ShadowTrafficMetricsRecorder<
                           PasswordChangesOrErrorImpl>::RecordShadowResult,
                       handler));
  }
}

void PasswordStoreProxyBackend::RemoveLoginsByURLAndTimeAsync(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordChangesOrErrorReply callback) {
  auto handler = base::MakeRefCounted<
      ShadowTrafficMetricsRecorder<PasswordChangesOrErrorImpl>>(
      MethodName("RemoveLoginsByURLAndTimeAsync"));

  // Sync completion callback is only used by the LocalDatabase backend and is
  // ignored by the Android backend.
  base::OnceCallback<void(bool)> sync_completion_callback;
  PasswordChangesOrErrorReply result_callback;
  if (UsesAndroidBackendAsMainBackend() && ShouldFallbackOnRemoveOperations()) {
    sync_completion_callback = base::DoNothing();
    auto execute_on_built_in_backend =
        base::BindOnce(&PasswordStoreBackend::RemoveLoginsByURLAndTimeAsync,
                       base::Unretained(built_in_backend_), url_filter,
                       delete_begin, delete_end, std::move(sync_completion));
    result_callback = base::BindOnce(
        &PasswordStoreProxyBackend::MaybeRetryOperation<PasswordChangesOrError>,
        weak_ptr_factory_.GetWeakPtr(), std::move(execute_on_built_in_backend),
        MethodName("RemoveLoginsByURLAndTimeAsync"), std::move(callback));
  } else {
    sync_completion_callback = std::move(sync_completion);
    result_callback = std::move(callback);
  }

  main_backend()->RemoveLoginsByURLAndTimeAsync(
      url_filter, delete_begin, delete_end, std::move(sync_completion_callback),
      base::BindOnce(&ShadowTrafficMetricsRecorder<
                         PasswordChangesOrErrorImpl>::RecordMainResult,
                     handler)
          .Then(std::move(result_callback)));
  if (ShouldExecuteDeletionsOnShadowBackend(
          prefs_, IsPasswordSyncEnabled(sync_service_))) {
    shadow_backend()->RemoveLoginsByURLAndTimeAsync(
        url_filter, std::move(delete_begin), std::move(delete_end),
        base::OnceCallback<void(bool)>(),
        base::BindOnce(&ShadowTrafficMetricsRecorder<
                           PasswordChangesOrErrorImpl>::RecordShadowResult,
                       handler));
  }
}

void PasswordStoreProxyBackend::RemoveLoginsCreatedBetweenAsync(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordChangesOrErrorReply callback) {
  auto handler = base::MakeRefCounted<
      ShadowTrafficMetricsRecorder<PasswordChangesOrErrorImpl>>(
      MethodName("RemoveLoginsCreatedBetweenAsync"));

  PasswordChangesOrErrorReply result_callback;
  if (UsesAndroidBackendAsMainBackend() && ShouldFallbackOnRemoveOperations()) {
    auto execute_on_built_in_backend = base::BindOnce(
        &PasswordStoreBackend::RemoveLoginsCreatedBetweenAsync,
        base::Unretained(built_in_backend_), delete_begin, delete_end);
    result_callback = base::BindOnce(
        &PasswordStoreProxyBackend::MaybeRetryOperation<PasswordChangesOrError>,
        weak_ptr_factory_.GetWeakPtr(), std::move(execute_on_built_in_backend),
        MethodName("RemoveLoginsCreatedBetweenAsync"), std::move(callback));
  } else {
    result_callback = std::move(callback);
  }

  main_backend()->RemoveLoginsCreatedBetweenAsync(
      delete_begin, delete_end,
      base::BindOnce(&ShadowTrafficMetricsRecorder<
                         PasswordChangesOrErrorImpl>::RecordMainResult,
                     handler)
          .Then(std::move(result_callback)));
  if (ShouldExecuteDeletionsOnShadowBackend(
          prefs_, IsPasswordSyncEnabled(sync_service_))) {
    shadow_backend()->RemoveLoginsCreatedBetweenAsync(
        std::move(delete_begin), std::move(delete_end),
        base::BindOnce(&ShadowTrafficMetricsRecorder<
                           PasswordChangesOrErrorImpl>::RecordShadowResult,
                       handler));
  }
}

void PasswordStoreProxyBackend::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  // TODO(https://crbug.com/1278807): Implement error handling, when actual
  // store changes will be received from the store.

  main_backend()->DisableAutoSignInForOriginsAsync(origin_filter,
                                                   std::move(completion));
  if (ShouldExecuteModifyOperationsOnShadowBackend(
          prefs_, IsPasswordSyncEnabled(sync_service_))) {
    shadow_backend()->DisableAutoSignInForOriginsAsync(
        origin_filter,
        /*completion=*/base::DoNothing());
  }
}

SmartBubbleStatsStore* PasswordStoreProxyBackend::GetSmartBubbleStatsStore() {
  return main_backend()->GetSmartBubbleStatsStore();
}

FieldInfoStore* PasswordStoreProxyBackend::GetFieldInfoStore() {
  return main_backend()->GetFieldInfoStore();
}

std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
PasswordStoreProxyBackend::CreateSyncControllerDelegate() {
  switch (features::kUpmExperimentVariationParam.Get()) {
    case features::UpmExperimentVariation::kEnableForSyncingUsers:
    case features::UpmExperimentVariation::kEnableOnlyBackendForSyncingUsers:
    case features::UpmExperimentVariation::kShadowSyncingUsers:
      DCHECK(!base::FeatureList::IsEnabled(
          features::kUnifiedPasswordManagerSyncUsingAndroidBackendOnly))
          << "Without support for local passwords, use legacy sync controller";
      return built_in_backend_->CreateSyncControllerDelegate();
    case features::UpmExperimentVariation::kEnableForAllUsers:
      return base::FeatureList::IsEnabled(
                 features::kUnifiedPasswordManagerSyncUsingAndroidBackendOnly)
                 ? android_backend_->CreateSyncControllerDelegate()
                 : built_in_backend_->CreateSyncControllerDelegate();
  }
  NOTREACHED() << "Define which backend creates the sync delegate.";
  return nullptr;
}

void PasswordStoreProxyBackend::ClearAllLocalPasswords() {
  NOTIMPLEMENTED();
}

void PasswordStoreProxyBackend::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  sync_service_ = sync_service;
  android_backend_->OnSyncServiceInitialized(sync_service);
}

template <typename ResultT>
void PasswordStoreProxyBackend::MaybeRetryOperation(
    base::OnceCallback<void(base::OnceCallback<void(ResultT)> callback)>
        retry_callback,
    const MethodName& method_name,
    base::OnceCallback<void(ResultT)> result_callback,
    ResultT result) {
  if (absl::holds_alternative<PasswordStoreBackendError>(result) &&
      ShouldErrorResultInFallback(
          absl::get<PasswordStoreBackendError>(result))) {
    base::UmaHistogramBoolean(GetFallbackMetricNameForMethod(method_name),
                              true);
    std::move(retry_callback).Run(std::move(result_callback));
  } else {
    std::move(result_callback).Run(std::move(result));
  }
}

PasswordStoreBackend* PasswordStoreProxyBackend::main_backend() {
  return UsesAndroidBackendAsMainBackend() ? android_backend_
                                           : built_in_backend_;
}

PasswordStoreBackend* PasswordStoreProxyBackend::shadow_backend() {
  return UsesAndroidBackendAsMainBackend() ? built_in_backend_
                                           : android_backend_;
}

void PasswordStoreProxyBackend::OnRemoteFormChangesReceived(
    CallbackOriginatesFromAndroidBackend originates_from_android,
    RemoteChangesReceived remote_form_changes_received,
    absl::optional<PasswordStoreChangeList> changes) {
  // `remote_form_changes_received` is used to inform observers about changes in
  // the backend. This check guarantees observers are informed only about
  // changes in the main backend.
  if (originates_from_android.value() == UsesAndroidBackendAsMainBackend()) {
    remote_form_changes_received.Run(std::move(changes));
  }
}

bool PasswordStoreProxyBackend::UsesAndroidBackendAsMainBackend() {
  if (prefs_->GetBoolean(prefs::kUnenrolledFromGoogleMobileServicesDueToErrors))
    return false;

  if (!IsPasswordSyncEnabled(sync_service_))
    return false;

  if (!base::FeatureList::IsEnabled(features::kUnifiedPasswordManagerAndroid))
    return false;

  features::UpmExperimentVariation variation =
      features::kUpmExperimentVariationParam.Get();
  switch (variation) {
    case features::UpmExperimentVariation::kEnableForSyncingUsers:
    case features::UpmExperimentVariation::kEnableOnlyBackendForSyncingUsers:
    case features::UpmExperimentVariation::kEnableForAllUsers:
      return true;
    case features::UpmExperimentVariation::kShadowSyncingUsers:
      return false;
  }
  NOTREACHED() << "Define explicitly whether Android is the main backend!";
  return false;
}

}  // namespace password_manager
