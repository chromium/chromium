// Copyright 2021 The Chromium Authors. All rights reserved.
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
#include "base/functional/identity.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "components/password_manager/core/browser/field_info_table.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"

namespace password_manager {

namespace {

bool ShouldExecuteModifyOperationsOnShadowBackend(PrefService* prefs,
                                                  bool is_syncing) {
  if (!base::FeatureList::IsEnabled(
          features::kUnifiedPasswordManagerShadowWriteOperationsAndroid)) {
    return false;
  }
  if (is_syncing)
    return false;
  if (features::kMigrationVersion.Get() >
      prefs->GetInteger(
          prefs::kCurrentMigrationVersionToGoogleMobileServices)) {
    // If initial migration isn't completed yet, we shouldn't modify the shadow
    // backend.
    return false;
  }
  return true;
}

bool ShouldExecuteReadOperationsOnShadowBackend(PrefService* prefs,
                                                bool is_syncing) {
  if (ShouldExecuteModifyOperationsOnShadowBackend(prefs, is_syncing)) {
    // Read operations are always allowed whenever modifications are allowed.
    // i.e. necessary migrations have happened and appropriate flags are set.
    return true;
  }
  return is_syncing && base::FeatureList::IsEnabled(
                           features::kUnifiedPasswordManagerShadowAndroid);
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

struct PasswordStoreChangeListImpl {
  using ResultType = absl::optional<PasswordStoreChangeList>;
  using ElementsType = PasswordStoreChangeList;

  static PasswordStoreChangeList* GetElements(
      absl::optional<PasswordStoreChangeList>& changelist) {
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

}  // namespace

PasswordStoreProxyBackend::PasswordStoreProxyBackend(
    PasswordStoreBackend* main_backend,
    PasswordStoreBackend* shadow_backend,
    PrefService* prefs,
    SyncDelegate* sync_delegate)
    : main_backend_(main_backend),
      shadow_backend_(shadow_backend),
      prefs_(prefs),
      sync_delegate_(sync_delegate) {}

PasswordStoreProxyBackend::~PasswordStoreProxyBackend() = default;

void PasswordStoreProxyBackend::InitBackend(
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  base::RepeatingCallback<void(bool)> pending_initialization_calls =
      base::BarrierCallback<bool>(
          /*num_callbacks=*/2, base::BindOnce(&InvokeCallbackWithCombinedStatus,
                                              std::move(completion)));

  main_backend_->InitBackend(std::move(remote_form_changes_received),
                             std::move(sync_enabled_or_disabled_cb),
                             base::BindOnce(pending_initialization_calls));
  shadow_backend_->InitBackend(base::DoNothing(), base::DoNothing(),
                               base::BindOnce(pending_initialization_calls));
}

void PasswordStoreProxyBackend::Shutdown(base::OnceClosure shutdown_completed) {
  base::RepeatingClosure pending_shutdown_calls = base::BarrierClosure(
      /*num_closures=*/2, std::move(shutdown_completed));
  main_backend_->Shutdown(pending_shutdown_calls);
  shadow_backend_->Shutdown(pending_shutdown_calls);
}

void PasswordStoreProxyBackend::GetAllLoginsAsync(LoginsOrErrorReply callback) {
  auto handler = base::MakeRefCounted<
      ShadowTrafficMetricsRecorder<LoginsResultOrErrorImpl>>(
      MethodName("GetAllLoginsAsync"));
  main_backend_->GetAllLoginsAsync(
      base::BindOnce(&ShadowTrafficMetricsRecorder<
                         LoginsResultOrErrorImpl>::RecordMainResult,
                     handler)
          .Then(std::move(callback)));

  if (ShouldExecuteReadOperationsOnShadowBackend(
          prefs_, sync_delegate_->IsSyncingPasswordsEnabled())) {
    shadow_backend_->GetAllLoginsAsync(
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
  main_backend_->GetAutofillableLoginsAsync(
      base::BindOnce(&ShadowTrafficMetricsRecorder<
                         LoginsResultOrErrorImpl>::RecordMainResult,
                     handler)
          .Then(std::move(callback)));

  if (ShouldExecuteReadOperationsOnShadowBackend(
          prefs_, sync_delegate_->IsSyncingPasswordsEnabled())) {
    shadow_backend_->GetAutofillableLoginsAsync(
        base::BindOnce(&ShadowTrafficMetricsRecorder<
                           LoginsResultOrErrorImpl>::RecordShadowResult,
                       handler));
  }
}

void PasswordStoreProxyBackend::FillMatchingLoginsAsync(
    LoginsReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  auto handler =
      base::MakeRefCounted<ShadowTrafficMetricsRecorder<LoginsResultImpl>>(
          MethodName("FillMatchingLoginsAsync"));
  main_backend_->FillMatchingLoginsAsync(
      base::BindOnce(
          &ShadowTrafficMetricsRecorder<LoginsResultImpl>::RecordMainResult,
          handler)
          .Then(std::move(callback)),
      include_psl, forms);

  if (ShouldExecuteReadOperationsOnShadowBackend(
          prefs_, sync_delegate_->IsSyncingPasswordsEnabled())) {
    shadow_backend_->FillMatchingLoginsAsync(
        base::BindOnce(
            &ShadowTrafficMetricsRecorder<LoginsResultImpl>::RecordShadowResult,
            handler),
        include_psl, forms);
  }
}

void PasswordStoreProxyBackend::AddLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  auto handler = base::MakeRefCounted<
      ShadowTrafficMetricsRecorder<PasswordStoreChangeListImpl>>(
      MethodName("AddLoginAsync"));

  main_backend_->AddLoginAsync(
      form, base::BindOnce(&ShadowTrafficMetricsRecorder<
                               PasswordStoreChangeListImpl>::RecordMainResult,
                           handler)
                .Then(std::move(callback)));
  if (ShouldExecuteModifyOperationsOnShadowBackend(
          prefs_, sync_delegate_->IsSyncingPasswordsEnabled())) {
    shadow_backend_->AddLoginAsync(
        form,
        base::BindOnce(&ShadowTrafficMetricsRecorder<
                           PasswordStoreChangeListImpl>::RecordShadowResult,
                       handler));
  }
}

void PasswordStoreProxyBackend::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  auto handler = base::MakeRefCounted<
      ShadowTrafficMetricsRecorder<PasswordStoreChangeListImpl>>(
      MethodName("UpdateLoginAsync"));

  main_backend_->UpdateLoginAsync(
      form, base::BindOnce(&ShadowTrafficMetricsRecorder<
                               PasswordStoreChangeListImpl>::RecordMainResult,
                           handler)
                .Then(std::move(callback)));
  if (ShouldExecuteModifyOperationsOnShadowBackend(
          prefs_, sync_delegate_->IsSyncingPasswordsEnabled())) {
    shadow_backend_->UpdateLoginAsync(
        form,
        base::BindOnce(&ShadowTrafficMetricsRecorder<
                           PasswordStoreChangeListImpl>::RecordShadowResult,
                       handler));
  }
}

void PasswordStoreProxyBackend::RemoveLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  auto handler = base::MakeRefCounted<
      ShadowTrafficMetricsRecorder<PasswordStoreChangeListImpl>>(
      MethodName("RemoveLoginAsync"));

  main_backend_->RemoveLoginAsync(
      form, base::BindOnce(&ShadowTrafficMetricsRecorder<
                               PasswordStoreChangeListImpl>::RecordMainResult,
                           handler)
                .Then(std::move(callback)));
  if (ShouldExecuteModifyOperationsOnShadowBackend(
          prefs_, sync_delegate_->IsSyncingPasswordsEnabled())) {
    shadow_backend_->RemoveLoginAsync(
        form,
        base::BindOnce(&ShadowTrafficMetricsRecorder<
                           PasswordStoreChangeListImpl>::RecordShadowResult,
                       handler));
  }
}

void PasswordStoreProxyBackend::RemoveLoginsByURLAndTimeAsync(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordStoreChangeListReply callback) {
  auto handler = base::MakeRefCounted<
      ShadowTrafficMetricsRecorder<PasswordStoreChangeListImpl>>(
      MethodName("RemoveLoginsByURLAndTimeAsync"));

  main_backend_->RemoveLoginsByURLAndTimeAsync(
      url_filter, delete_begin, delete_end, std::move(sync_completion),
      base::BindOnce(&ShadowTrafficMetricsRecorder<
                         PasswordStoreChangeListImpl>::RecordMainResult,
                     handler)
          .Then(std::move(callback)));
  if (ShouldExecuteModifyOperationsOnShadowBackend(
          prefs_, sync_delegate_->IsSyncingPasswordsEnabled())) {
    shadow_backend_->RemoveLoginsByURLAndTimeAsync(
        url_filter, std::move(delete_begin), std::move(delete_end),
        base::OnceCallback<void(bool)>(),
        base::BindOnce(&ShadowTrafficMetricsRecorder<
                           PasswordStoreChangeListImpl>::RecordShadowResult,
                       handler));
  }
}

void PasswordStoreProxyBackend::RemoveLoginsCreatedBetweenAsync(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordStoreChangeListReply callback) {
  auto handler = base::MakeRefCounted<
      ShadowTrafficMetricsRecorder<PasswordStoreChangeListImpl>>(
      MethodName("RemoveLoginsCreatedBetweenAsync"));

  main_backend_->RemoveLoginsCreatedBetweenAsync(
      delete_begin, delete_end,
      base::BindOnce(&ShadowTrafficMetricsRecorder<
                         PasswordStoreChangeListImpl>::RecordMainResult,
                     handler)
          .Then(std::move(callback)));
  if (ShouldExecuteModifyOperationsOnShadowBackend(
          prefs_, sync_delegate_->IsSyncingPasswordsEnabled())) {
    shadow_backend_->RemoveLoginsCreatedBetweenAsync(
        std::move(delete_begin), std::move(delete_end),
        base::BindOnce(&ShadowTrafficMetricsRecorder<
                           PasswordStoreChangeListImpl>::RecordShadowResult,
                       handler));
  }
}

void PasswordStoreProxyBackend::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  main_backend_->DisableAutoSignInForOriginsAsync(origin_filter,
                                                  std::move(completion));
  if (ShouldExecuteModifyOperationsOnShadowBackend(
          prefs_, sync_delegate_->IsSyncingPasswordsEnabled())) {
    shadow_backend_->DisableAutoSignInForOriginsAsync(
        origin_filter,
        /*completion=*/base::DoNothing());
  }
}

SmartBubbleStatsStore* PasswordStoreProxyBackend::GetSmartBubbleStatsStore() {
  return main_backend_->GetSmartBubbleStatsStore();
}

FieldInfoStore* PasswordStoreProxyBackend::GetFieldInfoStore() {
  return main_backend_->GetFieldInfoStore();
}

std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
PasswordStoreProxyBackend::CreateSyncControllerDelegate() {
  return main_backend_->CreateSyncControllerDelegate();
}

void PasswordStoreProxyBackend::ClearAllLocalPasswords() {
  NOTIMPLEMENTED();
}

void PasswordStoreProxyBackend::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  NOTIMPLEMENTED();
}

}  // namespace password_manager
