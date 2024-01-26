// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_PRIVATE_KEY_FACTORY_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_PRIVATE_KEY_FACTORY_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "components/enterprise/client_certificates/proto/client_certificates_database.pb.h"

namespace client_certificates {

struct EnumClassCompare {
  template <typename T>
  bool operator()(T value1, T value2) const {
    return static_cast<size_t>(value1) < static_cast<size_t>(value2);
  }
};

class PrivateKeyFactory {
 public:
  using PrivateKeyFactoriesMap =
      base::flat_map<PrivateKeySource,
                     std::unique_ptr<PrivateKeyFactory>,
                     EnumClassCompare>;
  using PrivateKeyCallback =
      base::OnceCallback<void(scoped_refptr<PrivateKey>)>;

  // Returns a factory instance given a set of all supported key sources and
  // their corresponding factories.
  static std::unique_ptr<PrivateKeyFactory> Create(
      PrivateKeyFactoriesMap sub_factories);

  virtual ~PrivateKeyFactory();

  // Will use the strongest supported key source to create a private key.
  // `callback` will be invoked with the resulting PrivateKey instance.
  virtual void CreatePrivateKey(PrivateKeyCallback callback) = 0;

  // Will use the data in `serialized_private_key` to load the private key into
  // a usable instance, and then invoke `callback` with it.
  virtual void LoadPrivateKey(
      const client_certificates_pb::PrivateKey& serialized_private_key,
      PrivateKeyCallback callback) = 0;

 protected:
  PrivateKeyFactory();
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_PRIVATE_KEY_FACTORY_H_
