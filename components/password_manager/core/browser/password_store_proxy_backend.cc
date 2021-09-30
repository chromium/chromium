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

void DeleteBackendAndInvokeCallback(
    std::unique_ptr<PasswordStoreBackend> backend_to_delete,
    base::OnceClosure completion) {
  backend_to_delete.reset();
  std::move(completion).Run();
}

}  // namespace

PasswordStoreProxyBackend::PasswordStoreProxyBackend(
    std::unique_ptr<PasswordStoreBackend> main_backend,
    std::unique_ptr<PasswordStoreBackend> shadow_backend)
    : main_backend_(std::move(main_backend)),
      shadow_backend_(std::move(shadow_backend)) {}

PasswordStoreProxyBackend::~PasswordStoreProxyBackend() = default;

void PasswordStoreProxyBackend::InitBackend(
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  DCHECK(!pending_initialization_calls_);
  pending_initialization_calls_ = base::BarrierCallback<bool>(
      /*num_callbacks=*/2,
      base::BindOnce(&InvokeCallbackWithCombinedStatus, std::move(completion)));

  main_backend_->InitBackend(std::move(remote_form_changes_received),
                             std::move(sync_enabled_or_disabled_cb),
                             base::BindOnce(pending_initialization_calls_));
  shadow_backend_->InitBackend(base::DoNothing(), base::DoNothing(),
                               base::BindOnce(pending_initialization_calls_));
}

void PasswordStoreProxyBackend::Shutdown(base::OnceClosure shutdown_completed) {
  DCHECK(!pending_shutdown_calls_);
  pending_shutdown_calls_ = base::BarrierClosure(
      /*num_closures=*/2, std::move(shutdown_completed));
  PasswordStoreBackend* backend = main_backend_.get();
  backend->Shutdown(base::BindOnce(&DeleteBackendAndInvokeCallback,
                                   std::move(main_backend_),
                                   pending_shutdown_calls_));
  backend = shadow_backend_.get();
  backend->Shutdown(base::BindOnce(&DeleteBackendAndInvokeCallback,
                                   std::move(shadow_backend_),
                                   pending_shutdown_calls_));
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
