// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/async_method_caller.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"

using chromeos::CryptohomeClient;

namespace cryptohome {

namespace {

AsyncMethodCaller* g_async_method_caller = NULL;

// The implementation of AsyncMethodCaller
class AsyncMethodCallerImpl : public AsyncMethodCaller,
                              public chromeos::CryptohomeClient::Observer {
 public:
  AsyncMethodCallerImpl() { CryptohomeClient::Get()->AddObserver(this); }

  ~AsyncMethodCallerImpl() override {
    CryptohomeClient::Get()->RemoveObserver(this);
  }

  void AsyncTpmAttestationCreateEnrollRequest(
      chromeos::attestation::PrivacyCAType pca_type,
      const DataCallback& callback) override {
    CryptohomeClient::Get()->AsyncTpmAttestationCreateEnrollRequest(
        pca_type,
        base::BindOnce(&AsyncMethodCallerImpl::RegisterAsyncDataCallback,
                       weak_ptr_factory_.GetWeakPtr(), callback,
                       "Couldn't initiate async attestation enroll request."));
  }

  void AsyncTpmAttestationEnroll(chromeos::attestation::PrivacyCAType pca_type,
                                 const std::string& pca_response,
                                 const Callback& callback) override {
    CryptohomeClient::Get()->AsyncTpmAttestationEnroll(
        pca_type, pca_response,
        base::BindOnce(&AsyncMethodCallerImpl::RegisterAsyncCallback,
                       weak_ptr_factory_.GetWeakPtr(), callback,
                       "Couldn't initiate async attestation enroll."));
  }

  void AsyncTpmAttestationCreateCertRequest(
      chromeos::attestation::PrivacyCAType pca_type,
      chromeos::attestation::AttestationCertificateProfile certificate_profile,
      const Identification& cryptohome_id,
      const std::string& request_origin,
      const DataCallback& callback) override {
    CryptohomeClient::Get()->AsyncTpmAttestationCreateCertRequest(
        pca_type, certificate_profile,
        CreateAccountIdentifierFromIdentification(cryptohome_id),
        request_origin,
        base::BindOnce(&AsyncMethodCallerImpl::RegisterAsyncDataCallback,
                       weak_ptr_factory_.GetWeakPtr(), callback,
                       "Couldn't initiate async attestation cert request."));
  }

  void AsyncTpmAttestationFinishCertRequest(
      const std::string& pca_response,
      chromeos::attestation::AttestationKeyType key_type,
      const Identification& cryptohome_id,
      const std::string& key_name,
      const DataCallback& callback) override {
    CryptohomeClient::Get()->AsyncTpmAttestationFinishCertRequest(
        pca_response, key_type,
        CreateAccountIdentifierFromIdentification(cryptohome_id), key_name,
        base::BindOnce(
            &AsyncMethodCallerImpl::RegisterAsyncDataCallback,
            weak_ptr_factory_.GetWeakPtr(), callback,
            "Couldn't initiate async attestation finish cert request."));
  }

  void TpmAttestationRegisterKey(
      chromeos::attestation::AttestationKeyType key_type,
      const Identification& cryptohome_id,
      const std::string& key_name,
      const Callback& callback) override {
    CryptohomeClient::Get()->TpmAttestationRegisterKey(
        key_type, CreateAccountIdentifierFromIdentification(cryptohome_id),
        key_name,
        base::BindOnce(&AsyncMethodCallerImpl::RegisterAsyncCallback,
                       weak_ptr_factory_.GetWeakPtr(), callback,
                       "Couldn't initiate async attestation register key."));
  }

  void TpmAttestationSignEnterpriseChallenge(
      chromeos::attestation::AttestationKeyType key_type,
      const Identification& cryptohome_id,
      const std::string& key_name,
      const std::string& domain,
      const std::string& device_id,
      chromeos::attestation::AttestationChallengeOptions options,
      const std::string& challenge,
      const std::string& key_name_for_spkac,
      const DataCallback& callback) override {
    CryptohomeClient::Get()->TpmAttestationSignEnterpriseChallenge(
        key_type, CreateAccountIdentifierFromIdentification(cryptohome_id),
        key_name, domain, device_id, options, challenge, key_name_for_spkac,
        base::BindOnce(
            &AsyncMethodCallerImpl::RegisterAsyncDataCallback,
            weak_ptr_factory_.GetWeakPtr(), callback,
            "Couldn't initiate async attestation enterprise challenge."));
  }

  void TpmAttestationSignSimpleChallenge(
      chromeos::attestation::AttestationKeyType key_type,
      const Identification& cryptohome_id,
      const std::string& key_name,
      const std::string& challenge,
      const DataCallback& callback) override {
    CryptohomeClient::Get()->TpmAttestationSignSimpleChallenge(
        key_type, CreateAccountIdentifierFromIdentification(cryptohome_id),
        key_name, challenge,
        base::BindOnce(
            &AsyncMethodCallerImpl::RegisterAsyncDataCallback,
            weak_ptr_factory_.GetWeakPtr(), callback,
            "Couldn't initiate async attestation simple challenge."));
  }

 private:
  struct CallbackElement {
    CallbackElement() = default;
    explicit CallbackElement(const AsyncMethodCaller::Callback& callback)
        : callback(callback),
          task_runner(base::ThreadTaskRunnerHandle::Get()) {}
    AsyncMethodCaller::Callback callback;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  };

  struct DataCallbackElement {
    DataCallbackElement() = default;
    explicit DataCallbackElement(
        const AsyncMethodCaller::DataCallback& callback)
        : data_callback(callback),
          task_runner(base::ThreadTaskRunnerHandle::Get()) {}
    AsyncMethodCaller::DataCallback data_callback;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  };

  typedef std::unordered_map<int, CallbackElement> CallbackMap;
  typedef std::unordered_map<int, DataCallbackElement> DataCallbackMap;

  // Handles the response for async calls.
  // Below is described how async calls work.
  // 1. CryptohomeClient::AsyncXXX returns "async ID".
  // 2. RegisterAsyncCallback registers the "async ID" with the user-provided
  //    callback.
  // 3. Cryptohome will return the result asynchronously as a signal with
  //    "async ID"
  // 4. "HandleAsyncResponse" handles the result signal and call the registered
  //    callback associated with the "async ID".
  void AsyncCallStatus(int async_id,
                       bool return_status,
                       int return_code) override {
    const CallbackMap::iterator it = callback_map_.find(async_id);
    if (it == callback_map_.end()) {
      LOG(ERROR) << "Received signal for unknown async_id " << async_id;
      return;
    }
    it->second.task_runner->PostTask(
        FROM_HERE, base::BindOnce(it->second.callback, return_status,
                                  static_cast<MountError>(return_code)));
    callback_map_.erase(it);
  }

  // Similar to HandleAsyncResponse but for signals with a raw data payload.
  void AsyncCallStatusWithData(int async_id,
                               bool return_status,
                               const std::string& return_data) override {
    const DataCallbackMap::iterator it = data_callback_map_.find(async_id);
    if (it == data_callback_map_.end()) {
      LOG(ERROR) << "Received signal for unknown async_id " << async_id;
      return;
    }
    it->second.task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(it->second.data_callback, return_status, return_data));
    data_callback_map_.erase(it);
  }

  // Registers a callback which is called when the result for AsyncXXX is ready.
  void RegisterAsyncCallback(Callback callback,
                             const char* error,
                             base::Optional<int> async_id) {
    if (!async_id.has_value()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(callback,
                                    false,  // return status
                                    cryptohome::MOUNT_ERROR_FATAL));
      return;
    }

    if (async_id.value() == 0) {
      LOG(ERROR) << error;
      return;
    }
    VLOG(1) << "Adding handler for " << async_id.value();
    DCHECK_EQ(callback_map_.count(async_id.value()), 0U);
    DCHECK_EQ(data_callback_map_.count(async_id.value()), 0U);
    callback_map_[async_id.value()] = CallbackElement(callback);
  }

  // Registers a callback which is called when the result for AsyncXXX is ready.
  void RegisterAsyncDataCallback(DataCallback callback,
                                 const char* error,
                                 base::Optional<int> async_id) {
    if (!async_id.has_value()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(callback,
                                    false,  // return status
                                    std::string()));
      return;
    }
    if (async_id.value() == 0) {
      LOG(ERROR) << error;
      return;
    }
    VLOG(1) << "Adding handler for " << async_id.value();
    DCHECK_EQ(callback_map_.count(async_id.value()), 0U);
    DCHECK_EQ(data_callback_map_.count(async_id.value()), 0U);
    data_callback_map_[async_id.value()] = DataCallbackElement(callback);
  }

  CallbackMap callback_map_;
  DataCallbackMap data_callback_map_;
  base::WeakPtrFactory<AsyncMethodCallerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AsyncMethodCallerImpl);
};

}  // namespace

// static
void AsyncMethodCaller::Initialize() {
  if (g_async_method_caller) {
    LOG(WARNING) << "AsyncMethodCaller was already initialized";
    return;
  }
  g_async_method_caller = new AsyncMethodCallerImpl();
  VLOG(1) << "AsyncMethodCaller initialized";
}

// static
void AsyncMethodCaller::InitializeForTesting(
    AsyncMethodCaller* async_method_caller) {
  if (g_async_method_caller) {
    LOG(WARNING) << "AsyncMethodCaller was already initialized";
    return;
  }
  g_async_method_caller = async_method_caller;
  VLOG(1) << "AsyncMethodCaller initialized";
}

// static
void AsyncMethodCaller::Shutdown() {
  if (!g_async_method_caller) {
    LOG(WARNING) << "AsyncMethodCaller::Shutdown() called with NULL manager";
    return;
  }
  delete g_async_method_caller;
  g_async_method_caller = NULL;
  VLOG(1) << "AsyncMethodCaller Shutdown completed";
}

// static
AsyncMethodCaller* AsyncMethodCaller::GetInstance() {
  return g_async_method_caller;
}

}  // namespace cryptohome
