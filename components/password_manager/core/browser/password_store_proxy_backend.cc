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
#include "base/ranges/algorithm.h"
#include "components/password_manager/core/browser/field_info_table.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"

namespace password_manager {

namespace {

void InvokeCallbackWithCombinedStatus(base::OnceCallback<void(bool)> completion,
                                      std::vector<bool> statuses) {
  std::move(completion).Run(base::ranges::all_of(statuses, base::identity()));
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
  main_backend_->GetAllLoginsAsync(std::move(callback));
  // TODO(crbug.com/1240927): Request shadow_backend_ and compare results.
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
PasswordStoreProxyBackend::CreateSyncControllerDelegateFactory() {
  return main_backend_->CreateSyncControllerDelegateFactory();
}

}  // namespace password_manager
