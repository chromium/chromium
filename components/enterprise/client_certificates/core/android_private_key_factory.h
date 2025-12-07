// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_ANDROID_PRIVATE_KEY_FACTORY_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_ANDROID_PRIVATE_KEY_FACTORY_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/enterprise/client_certificates/android/browser_binding/browser_key_store_android.h"
#include "components/enterprise/client_certificates/core/private_key_factory.h"
#include "components/enterprise/client_certificates/proto/client_certificates_database.pb.h"

namespace client_certificates {

class AndroidPrivateKeyFactory : public PrivateKeyFactory {
 public:
  static std::unique_ptr<AndroidPrivateKeyFactory> TryCreate();

  ~AndroidPrivateKeyFactory() override;

  // PrivateKeyFactory:
  void CreatePrivateKey(PrivateKeyCallback callback) override;
  void LoadPrivateKey(
      const client_certificates_pb::PrivateKey& serialized_private_key,
      PrivateKeyCallback callback) override;
  void LoadPrivateKeyFromDict(const base::Value::Dict& serialized_private_key,
                              PrivateKeyCallback callback) override;

 private:
  AndroidPrivateKeyFactory();

  base::WeakPtrFactory<AndroidPrivateKeyFactory> weak_factory_{this};
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_ANDROID_PRIVATE_KEY_FACTORY_H_
