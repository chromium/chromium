// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ownership/owner_settings_service.h"

#include <cryptohi.h>
#include <keyhi.h>
#include <stdint.h>

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/values.h"
#include "components/ownership/owner_key_util.h"
#include "crypto/scoped_nss_types.h"

namespace em = enterprise_management;

namespace ownership {

namespace {

using ScopedSGNContext = std::unique_ptr<
    SGNContext,
    crypto::NSSDestroyer1<SGNContext, SGN_DestroyContext, PR_TRUE>>;

// |public_key| is included in the |policy|
// if the ChromeSideOwnerKeyGeneration Feature is enabled. |private_key|
// actually signs the |policy| (must belong to the same key pair as
// |public_key|).
std::unique_ptr<em::PolicyFetchResponse> AssembleAndSignPolicy(
    std::unique_ptr<em::PolicyData> policy,
    scoped_refptr<ownership::PublicKey> public_key,
    scoped_refptr<ownership::PrivateKey> private_key) {
  DCHECK(private_key->key());

  // Assemble the policy.
  std::unique_ptr<em::PolicyFetchResponse> policy_response(
      new em::PolicyFetchResponse());

  if (base::FeatureList::IsEnabled(ownership::kChromeSideOwnerKeyGeneration)) {
    policy_response->set_new_public_key(public_key->data().data(),
                                        public_key->data().size());
  }

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

}  // namespace

BASE_FEATURE(kChromeSideOwnerKeyGeneration,
             "ChromeSideOwnerKeyGeneration",
             base::FeatureState::FEATURE_DISABLED_BY_DEFAULT);

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

void OwnerSettingsService::IsOwnerAsync(IsOwnerCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (private_key_.get()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), IsOwner()));
  } else {
    pending_is_owner_callbacks_.push_back(std::move(callback));
  }
}

bool OwnerSettingsService::AssembleAndSignPolicyAsync(
    base::TaskRunner* task_runner,
    std::unique_ptr<em::PolicyData> policy,
    AssembleAndSignPolicyAsyncCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!task_runner || !IsOwner())
    return false;
  // |public_key_| is explicitly forwarded down to
  // |OwnerSettingsServiceAsh::OnSignedPolicyStored()| to make sure that only
  // the key that was actually included in a policy gets marked as persisted
  // (theoretically a different key can be re-assigned to |public_key_| in
  // between the async calls).
  return task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AssembleAndSignPolicy, std::move(policy), public_key_,
                     private_key_),
      base::BindOnce(std::move(callback), public_key_));
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

void OwnerSettingsService::RunPendingIsOwnerCallbacksForTesting(bool is_owner) {
  std::vector<IsOwnerCallback> is_owner_callbacks;
  is_owner_callbacks.swap(pending_is_owner_callbacks_);
  for (auto& callback : is_owner_callbacks)
    std::move(callback).Run(is_owner);
}

bool OwnerSettingsService::SetString(const std::string& setting,
                                     const std::string& value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::Value in_value(value);
  return Set(setting, in_value);
}

void OwnerSettingsService::ReloadKeypair() {
  ReloadKeypairImpl(
      base::BindOnce(&OwnerSettingsService::OnKeypairLoaded, as_weak_ptr()));
}

void OwnerSettingsService::OnKeypairLoaded(
    scoped_refptr<PublicKey> public_key,
    scoped_refptr<PrivateKey> private_key) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // The pointers themself should not be null to indicate that the keys finished
  // loading (even if unsuccessfully). Absence of the actual data inside can
  // indicate that the keys are unavailable.
  public_key_ =
      public_key ? public_key
                 : base::MakeRefCounted<ownership::PublicKey>(
                       /*is_persisted=*/false, /*data=*/std::vector<uint8_t>());
  private_key_ = private_key
                     ? private_key
                     : base::MakeRefCounted<ownership::PrivateKey>(nullptr);

  std::vector<IsOwnerCallback> is_owner_callbacks;
  is_owner_callbacks.swap(pending_is_owner_callbacks_);

  const bool is_owner = IsOwner();
  for (auto& callback : is_owner_callbacks)
    std::move(callback).Run(is_owner);

  OnPostKeypairLoadedActions();
}

}  // namespace ownership
