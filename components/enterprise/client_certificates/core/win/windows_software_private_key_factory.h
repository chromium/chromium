// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_WIN_WINDOWS_SOFTWARE_PRIVATE_KEY_FACTORY_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_WIN_WINDOWS_SOFTWARE_PRIVATE_KEY_FACTORY_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/enterprise/client_certificates/core/private_key_factory.h"

namespace client_certificates {

class WindowsSoftwarePrivateKeyFactory : public PrivateKeyFactory {
 public:
  // Will return a factory instance only if the creation of
  // software keys via the OS is supported on the current device.
  // Otherwise, will return nullptr.
  static std::unique_ptr<WindowsSoftwarePrivateKeyFactory> TryCreate();

  ~WindowsSoftwarePrivateKeyFactory() override;

  // PrivateKeyFactory:
  void CreatePrivateKey(PrivateKeyCallback callback) override;
  void LoadPrivateKey(
      const client_certificates_pb::PrivateKey& serialized_private_key,
      PrivateKeyCallback callback) override;
  void LoadPrivateKeyFromDict(const base::Value::Dict& serialized_private_key,
                              PrivateKeyCallback callback) override;

 private:
  WindowsSoftwarePrivateKeyFactory();

  base::WeakPtrFactory<WindowsSoftwarePrivateKeyFactory> weak_factory_{this};
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_WIN_WINDOWS_SOFTWARE_PRIVATE_KEY_FACTORY_H_
