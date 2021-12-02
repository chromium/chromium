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
#include "components/sync/model/proxy_model_type_controller_delegate.h"

namespace password_manager {

namespace {

using MethodName = base::StrongAlias<struct MethodNameTag, std::string>;

bool IsPasswordUniquePtrLess(const std::unique_ptr<PasswordForm>& lhs,
                             const std::unique_ptr<PasswordForm>& rhs) {
  return PasswordFormUniqueKey(*lhs) < PasswordFormUniqueKey(*rhs);
}

bool IsPasswordUniquePtrWithSameKeyInconsistent(
    const std::unique_ptr<PasswordForm>& lhs,
    const std::unique_ptr<PasswordForm>& rhs) {
  return lhs->password_value != rhs->password_value;
}

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
// the shadow backends once both calls are finished.
//
// The class is ref-counted because it is equally owned by the two parallel
// method calls : it must outlive the first returning one and shall  be
// destroyed after the second one returns.
class ShadowTrafficMetricsRecorder
    : public base::RefCounted<ShadowTrafficMetricsRecorder> {
 public:
  explicit ShadowTrafficMetricsRecorder(MethodName method_name)
      : method_name_(std::move(method_name)) {}

  // Returns the unchanged |result| so it can be passed to the main handler.
  LoginsResultOrError RecordMainLoginsResultOrError(
      LoginsResultOrError logins_or_error) {
    if (absl::holds_alternative<PasswordStoreBackendError>(logins_or_error)) {
      return logins_or_error;
    }

    LoginsResult logins = std::move(absl::get<LoginsResult>(logins_or_error));
    if (!first_result_) {
      first_result_ = absl::make_optional<LoginsResult>();
      first_result_->reserve(logins.size());
      for (const auto& login : logins)
        first_result_->push_back(std::make_unique<PasswordForm>(*login));
    } else {
      RecordMetrics(method_name_, /*main_result=*/logins,
                    /*shadow_result=*/*first_result_, &IsPasswordUniquePtrLess,
                    &IsPasswordUniquePtrWithSameKeyInconsistent);
    }

    return logins;
  }

  void RecordShadowLoginsResultOrError(LoginsResultOrError logins_or_error) {
    if (absl::holds_alternative<PasswordStoreBackendError>(logins_or_error)) {
      return;
    }

    LoginsResult logins = std::move(absl::get<LoginsResult>(logins_or_error));
    if (!first_result_)
      first_result_ = std::move(logins);
    else
      RecordMetrics(method_name_, /*main_result=*/*first_result_,
                    /*shadow_result=*/logins, &IsPasswordUniquePtrLess,
                    &IsPasswordUniquePtrWithSameKeyInconsistent);
  }

 private:
  friend class RefCounted<ShadowTrafficMetricsRecorder>;
  ~ShadowTrafficMetricsRecorder() = default;

  // Stores the result of the backend that returns first.
  absl::optional<LoginsResult> first_result_;
  const MethodName method_name_;
};

}  // namespace

PasswordStoreProxyBackend::PasswordStoreProxyBackend(
    PasswordStoreBackend* main_backend,
    PasswordStoreBackend* shadow_backend,
    base::RepeatingCallback<bool()> is_syncing_passwords_callback)
    : main_backend_(main_backend),
      shadow_backend_(shadow_backend),
      is_syncing_passwords_callback_(std::move(is_syncing_passwords_callback)) {
}

PasswordStoreProxyBackend::~PasswordStoreProxyBackend() = default;

base::WeakPtr<PasswordStoreBackend> PasswordStoreProxyBackend::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

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
  scoped_refptr<ShadowTrafficMetricsRecorder> handler =
      base::MakeRefCounted<ShadowTrafficMetricsRecorder>(
          MethodName("GetAllLoginsAsync"));
  main_backend_->GetAllLoginsAsync(
      base::BindOnce(
          &ShadowTrafficMetricsRecorder::RecordMainLoginsResultOrError, handler)
          .Then(std::move(callback)));

  if (is_syncing_passwords_callback_.Run() &&
      base::FeatureList::IsEnabled(
          features::kUnifiedPasswordManagerShadowAndroid)) {
    shadow_backend_->GetAllLoginsAsync(base::BindOnce(
        &ShadowTrafficMetricsRecorder::RecordShadowLoginsResultOrError,
        handler));
  }
}

void PasswordStoreProxyBackend::GetAutofillableLoginsAsync(
    LoginsOrErrorReply callback) {
  main_backend_->GetAutofillableLoginsAsync(std::move(callback));
  // TODO(crbug.com/1229655): Request shadow_backend_ and compare results.
}

void PasswordStoreProxyBackend::FillMatchingLoginsAsync(
    LoginsReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  main_backend_->FillMatchingLoginsAsync(std::move(callback), include_psl,
                                         forms);
  // TODO(crbug.com/1229655): Request shadow_backend_ and compare results.
}

void PasswordStoreProxyBackend::AddLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  main_backend_->AddLoginAsync(form, std::move(callback));
  // TODO(crbug.com/1229655): Request shadow_backend_ and compare results.
}

void PasswordStoreProxyBackend::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  main_backend_->UpdateLoginAsync(form, std::move(callback));
  // TODO(crbug.com/1229655): Request shadow_backend_ and compare results.
}

void PasswordStoreProxyBackend::RemoveLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  main_backend_->RemoveLoginAsync(form, std::move(callback));
  // TODO(crbug.com/1229655): Request shadow_backend_ and compare results.
}

void PasswordStoreProxyBackend::RemoveLoginsByURLAndTimeAsync(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordStoreChangeListReply callback) {
  main_backend_->RemoveLoginsByURLAndTimeAsync(
      url_filter, std::move(delete_begin), std::move(delete_end),
      std::move(sync_completion), std::move(callback));
  // TODO(crbug.com/1229655): Request shadow_backend_ and compare results.
}

void PasswordStoreProxyBackend::RemoveLoginsCreatedBetweenAsync(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordStoreChangeListReply callback) {
  main_backend_->RemoveLoginsCreatedBetweenAsync(
      std::move(delete_begin), std::move(delete_end), std::move(callback));
  // TODO(crbug.com/1229655): Request shadow_backend_ and compare results.
}

void PasswordStoreProxyBackend::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  main_backend_->DisableAutoSignInForOriginsAsync(origin_filter,
                                                  std::move(completion));
  // TODO(crbug.com/1229655): Request shadow_backend_ and compare results.
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

}  // namespace password_manager
