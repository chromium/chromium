// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_SECURE_MESSAGE_DELEGATE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_SECURE_MESSAGE_DELEGATE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/multidevice/secure_message_delegate.h"

namespace ash {

class EasyUnlockClient;

namespace multidevice {

// Concrete SecureMessageDelegate implementation.
// Note: Callbacks are guaranteed to *not* be invoked after
// SecureMessageDelegateImpl is destroyed.
class SecureMessageDelegateImpl : public SecureMessageDelegate {
 public:
  class Factory {
   public:
    static std::unique_ptr<SecureMessageDelegate> Create();
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<SecureMessageDelegate> CreateInstance() = 0;

   private:
    static Factory* test_factory_instance_;
  };

  SecureMessageDelegateImpl(const SecureMessageDelegateImpl&) = delete;
  SecureMessageDelegateImpl& operator=(const SecureMessageDelegateImpl&) =
      delete;

  ~SecureMessageDelegateImpl() override;

  // SecureMessageDelegate:
  void GenerateKeyPair(GenerateKeyPairCallback callback) override;
  void DeriveKey(const std::string& private_key,
                 const std::string& public_key,
                 DeriveKeyCallback callback) override;
  void CreateSecureMessage(const std::string& payload,
                           const std::string& key,
                           const CreateOptions& create_options,
                           CreateSecureMessageCallback callback) override;
  void UnwrapSecureMessage(const std::string& serialized_message,
                           const std::string& key,
                           const UnwrapOptions& unwrap_options,
                           UnwrapSecureMessageCallback callback) override;

 private:
  SecureMessageDelegateImpl();

  // Processes results returned from the dbus client, if necessary, and invokes
  // the SecureMessageDelegate callbacks with the processed results. Note: When
  // invoking dbus client methods, we bind these functions to a weak pointer to
  // ensure that these functions are not called after the
  // SecureMessageDelegateImpl object is destroyed.
  void OnGenerateKeyPairResult(GenerateKeyPairCallback callback,
                               const std::string& private_key,
                               const std::string& public_key);
  void OnDeriveKeyResult(DeriveKeyCallback callback,
                         const std::string& derived_key);
  void OnCreateSecureMessageResult(CreateSecureMessageCallback callback,
                                   const std::string& secure_message);
  void OnUnwrapSecureMessageResult(UnwrapSecureMessageCallback callback,
                                   const std::string& unwrap_result);

  // Not owned by this instance.
  raw_ptr<EasyUnlockClient> dbus_client_;

  base::WeakPtrFactory<SecureMessageDelegateImpl> weak_ptr_factory_{this};
};

}  // namespace multidevice
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_SECURE_MESSAGE_DELEGATE_IMPL_H_
