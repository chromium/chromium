// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_SIGNING_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_SIGNING_SERVICE_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "components/policy/policy_export.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

// Data signing interface.
class POLICY_EXPORT SigningService {
 public:
  using SigningCallback =
      base::OnceCallback<void(bool success,
                              enterprise_management::SignedData signed_data)>;

  virtual ~SigningService() = default;

  // Signs |data| and calls |callback| with the signed data.
  virtual void SignData(const std::string& data, SigningCallback callback) = 0;
};

} // namespace policy

#endif // COMPONENTS_POLICY_CORE_COMMON_CLOUD_SIGNING_SERVICE_H_

