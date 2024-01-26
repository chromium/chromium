// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_EC_PRIVATE_KEY_FACTORY_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_EC_PRIVATE_KEY_FACTORY_H_

#include "base/memory/weak_ptr.h"
#include "components/enterprise/client_certificates/core/private_key_factory.h"

namespace client_certificates {

class ECPrivateKeyFactory : public PrivateKeyFactory {
 public:
  ECPrivateKeyFactory();
  ~ECPrivateKeyFactory() override;

  // PrivateKeyFactory:
  void CreatePrivateKey(PrivateKeyCallback callback) override;
  void LoadPrivateKey(
      const client_certificates_pb::PrivateKey& serialized_private_key,
      PrivateKeyCallback callback) override;

 private:
  base::WeakPtrFactory<ECPrivateKeyFactory> weak_factory_{this};
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_EC_PRIVATE_KEY_FACTORY_H_
