// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_proxy_backend.h"
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

void InvokeCallbackWithCombinedStatus(base::OnceCallback<void(bool)> completion,
                                      std::vector<bool> statuses) {
  std::move(completion).Run(base::ranges::all_of(statuses, base::identity()));
}

// Records the difference metrics between |main_result| and |backend_result|.
void RecordMetrics(const LoginsResult& main_result,
                   const LoginsResult& backend_result) {
  struct IsLess {
    bool operator()(const PasswordForm* lhs, const PasswordForm* rhs) const {
      return PasswordFormUniqueKey(*lhs) < PasswordFormUniqueKey(*rhs);
    }
  };

  auto main_logins = base::MakeFlatSet<const PasswordForm*, IsLess>(
      main_result, {}, &std::unique_ptr<PasswordForm>::get);
  auto shadow_logins = base::MakeFlatSet<const PasswordForm*, IsLess>(
      backend_result, {}, &std::unique_ptr<PasswordForm>::get);

  auto common_logins = [&] {
    std::vector<const PasswordForm*> vec;
    vec.reserve(main_logins.size());
    base::ranges::set_intersection(main_logins, shadow_logins,
                                   std::back_inserter(vec), IsLess());
    return base::flat_set<const PasswordForm*, IsLess>(std::move(vec));
  }();

  // The cardinalities from which we compute the metrics.
  size_t main_minus_shadow = main_logins.size() - common_logins.size();
  size_t shadow_minus_main = shadow_logins.size() - common_logins.size();
  size_t diff = main_minus_shadow + shadow_minus_main;
  size_t total = diff + common_logins.size();
  size_t inconsistent = base::ranges::count_if(common_logins, [&](auto* f) {
    auto lhs = main_logins.find(f);
    auto rhs = shadow_logins.find(f);
    DCHECK(lhs != main_logins.end());
    DCHECK(rhs != shadow_logins.end());
    return (*lhs)->password_value != (*rhs)->password_value;
  });

  // Emits a pair of absolute and relative metrics.
  auto Emit = [](base::StringPiece metric_infix, size_t nominator,
                 size_t denominator) {
    std::string prefix = base::StrCat(
        {"PasswordManager.PasswordStoreProxyBackend.GetAllLoginsAsync.",
         metric_infix, "."});
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
  Emit("InconsistentPasswords", inconsistent, common_logins.size());
}

// Records the metrics of a pair of GetAllLoginsAsync() calls to the main and
// the shadow backends once both calls are finished.
//
// The class is ref-counted because it is equally owned by the two parallel
// GetAllLoginsAsync() calls: it must outlive the first returning one and shall
// be destroyed after the second one returns.
class GetAllLoginsAsyncMetricsRecorder
    : public base::RefCounted<GetAllLoginsAsyncMetricsRecorder> {
 public:
  // Returns the unchanged |result| so it can be passed to the main handler.
  LoginsResult RecordMainResult(LoginsResult result) {
    if (!first_result_) {
      first_result_ = absl::make_optional<LoginsResult>();
      first_result_->reserve(result.size());
      for (const auto& login : result)
        first_result_->push_back(std::make_unique<PasswordForm>(*login));
    } else {
      RecordMetrics(/*main_result=*/result, /*shadow_result=*/*first_result_);
    }
    return result;
  }

  void RecordShadowResult(LoginsResult result) {
    if (!first_result_)
      first_result_ = std::move(result);
    else
      RecordMetrics(/*main_result=*/*first_result_, /*shadow_result=*/result);
  }

 private:
  friend class RefCounted<GetAllLoginsAsyncMetricsRecorder>;
  ~GetAllLoginsAsyncMetricsRecorder() = default;

  // Stores the result of the backend that returns first.
  absl::optional<LoginsResult> first_result_;
};

void InvokeCallbackIfShadowingAllowed(base::OnceClosure callback,
                                      bool sync_enabled) {
  if (sync_enabled && base::FeatureList::IsEnabled(
                          features::kUnifiedPasswordManagerShadowAndroid)) {
    std::move(callback).Run();
  }
}

}  // namespace

PasswordStoreProxyBackend::PasswordStoreProxyBackend(
    PasswordStoreBackend* main_backend,
    PasswordStoreBackend* shadow_backend)
    : main_backend_(main_backend), shadow_backend_(shadow_backend) {}

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

void PasswordStoreProxyBackend::GetAllLoginsAsync(LoginsReply callback) {
  scoped_refptr<GetAllLoginsAsyncMetricsRecorder> handler =
      base::MakeRefCounted<GetAllLoginsAsyncMetricsRecorder>();
  main_backend_->GetAllLoginsAsync(
      base::BindOnce(&GetAllLoginsAsyncMetricsRecorder::RecordMainResult,
                     handler)
          .Then(std::move(callback)));

  auto sync_status_callback = base::BindOnce(
      &PasswordStoreBackend::GetAllLoginsAsync,
      base::Unretained(shadow_backend_),
      base::BindOnce(&GetAllLoginsAsyncMetricsRecorder::RecordShadowResult,
                     handler));

  GetSyncStatus(base::BindOnce(&InvokeCallbackIfShadowingAllowed,
                               std::move(sync_status_callback)));
}

void PasswordStoreProxyBackend::GetAutofillableLoginsAsync(
    LoginsReply callback) {
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
  if (base::FeatureList::IsEnabled(
          features::kUnifiedPasswordManagerSyncUsingAndroidBackendOnly)) {
    // The shadow backend (PasswordStoreAndroidBackend) creates a controller
    // delegate that prevents sync from actually communicating with the sync
    // server using the built in SyncEngine.
    return shadow_backend_->CreateSyncControllerDelegate();
  }

  return main_backend_->CreateSyncControllerDelegate();
}

void PasswordStoreProxyBackend::GetSyncStatus(
    base::OnceCallback<void(bool)> callback) {
  return main_backend_->GetSyncStatus(std::move(callback));
}

}  // namespace password_manager
