// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ownership/owner_settings_service.h"

#include <cryptohi.h>
#include <keyhi.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/ownership/owner_key_util.h"
#include "crypto/scoped_nss_types.h"

namespace em = enterprise_management;

namespace ownership {

namespace {

using ScopedSGNContext = std::unique_ptr<
    SGNContext,
    crypto::NSSDestroyer1<SGNContext, SGN_DestroyContext, PR_TRUE>>;

std::unique_ptr<em::PolicyFetchResponse> AssembleAndSignPolicy(
    std::unique_ptr<em::PolicyData> policy,
    scoped_refptr<ownership::PrivateKey> private_key) {
  DCHECK(private_key->key());

  // Assemble the policy.
  std::unique_ptr<em::PolicyFetchResponse> policy_response(
      new em::PolicyFetchResponse());
  if (!policy->SerializeToString(policy_response->mutable_policy_data())) {
    LOG(ERROR) << "Failed to encode policy payload.";
    return nullptr;
  }

  ScopedSGNContext sign_context(SGN_NewContext(
      SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION, private_key->key()));
  if (!sign_context) {
    NOTREACHED();
    return nullptr;
  }

  SECItem signature_item;
  if (SGN_Begin(sign_context.get()) != SECSuccess ||
      SGN_Update(sign_context.get(),
                 reinterpret_cast<const uint8_t*>(
                     policy_response->policy_data().c_str()),
                 policy_response->policy_data().size()) != SECSuccess ||
      SGN_End(sign_context.get(), &signature_item) != SECSuccess) {
    LOG(ERROR) << "Failed to create policy signature.";
    return nullptr;
  }

  policy_response->mutable_policy_data_signature()->assign(
      reinterpret_cast<const char*>(signature_item.data), signature_item.len);
  SECITEM_FreeItem(&signature_item, PR_FALSE);

  return policy_response;
}

}  // namepace

OwnerSettingsService::OwnerSettingsService(
    const scoped_refptr<ownership::OwnerKeyUtil>& owner_key_util)
    : owner_key_util_(owner_key_util) {}

OwnerSettingsService::~OwnerSettingsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void OwnerSettingsService::AddObserver(Observer* observer) {
  if (observer && !observers_.HasObserver(observer))
    observers_.AddObserver(observer);
}

void OwnerSettingsService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool OwnerSettingsService::IsReady() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return private_key_.get();
}

bool OwnerSettingsService::IsOwner() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return private_key_.get() && private_key_->key();
}

void OwnerSettingsService::IsOwnerAsync(const IsOwnerCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (private_key_.get()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback, IsOwner()));
  } else {
    pending_is_owner_callbacks_.push_back(callback);
  }
}

bool OwnerSettingsService::AssembleAndSignPolicyAsync(
    base::TaskRunner* task_runner,
    std::unique_ptr<em::PolicyData> policy,
    const AssembleAndSignPolicyAsyncCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!task_runner || !IsOwner())
    return false;
  return base::PostTaskAndReplyWithResult(
      task_runner, FROM_HERE,
      base::Bind(&AssembleAndSignPolicy, base::Passed(&policy), private_key_),
      callback);
}

bool OwnerSettingsService::SetBoolean(const std::string& setting, bool value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::Value in_value(value);
  return Set(setting, in_value);
}

bool OwnerSettingsService::SetInteger(const std::string& setting, int value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::Value in_value(value);
  return Set(setting, in_value);
}

bool OwnerSettingsService::SetDouble(const std::string& setting, double value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::Value in_value(value);
  return Set(setting, in_value);
}

bool OwnerSettingsService::SetString(const std::string& setting,
                                     const std::string& value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::Value in_value(value);
  return Set(setting, in_value);
}

void OwnerSettingsService::ReloadKeypair() {
  ReloadKeypairImpl(
      base::Bind(&OwnerSettingsService::OnKeypairLoaded, as_weak_ptr()));
}

void OwnerSettingsService::OnKeypairLoaded(
    const scoped_refptr<PublicKey>& public_key,
    const scoped_refptr<PrivateKey>& private_key) {
  DCHECK(thread_checker_.CalledOnValidThread());

  public_key_ = public_key;
  private_key_ = private_key;

  const bool is_owner = IsOwner();
  std::vector<IsOwnerCallback> is_owner_callbacks;
  is_owner_callbacks.swap(pending_is_owner_callbacks_);
  for (std::vector<IsOwnerCallback>::iterator it(is_owner_callbacks.begin());
       it != is_owner_callbacks.end();
       ++it) {
    it->Run(is_owner);
  }

  OnPostKeypairLoadedActions();
}

}  // namespace ownership
