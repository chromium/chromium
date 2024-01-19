// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_UNEXPORTABLE_PRIVATE_KEY_FACTORY_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_UNEXPORTABLE_PRIVATE_KEY_FACTORY_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/enterprise/client_certificates/core/private_key_factory.h"

namespace client_certificates {

class UnexportablePrivateKeyFactory : public PrivateKeyFactory {
 public:
  // Will return a factory instance only if the creation of
  // crypto::UnexportableSigningKeys is supported on the current device (e.g. a
  // TPM is present on Windows). Otherwise, will return nullptr.
  static std::unique_ptr<UnexportablePrivateKeyFactory> TryCreate();

  ~UnexportablePrivateKeyFactory() override;

  // PrivateKeyFactory:
  void CreatePrivateKey(PrivateKeyCallback callback) override;

 private:
  UnexportablePrivateKeyFactory();

  base::WeakPtrFactory<UnexportablePrivateKeyFactory> weak_factory_{this};
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_UNEXPORTABLE_PRIVATE_KEY_FACTORY_H_
