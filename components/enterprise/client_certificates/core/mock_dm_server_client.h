// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_DM_SERVER_CLIENT_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_DM_SERVER_CLIENT_H_

#include "base/functional/callback.h"
#include "components/enterprise/client_certificates/core/dm_server_client.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace client_certificates {

class MockDMServerClient : public DMServerClient {
 public:
  MockDMServerClient();
  ~MockDMServerClient() override;

  MOCK_METHOD(void,
              SendRequest,
              (const GURL&,
               std::string_view,
               const enterprise_management::DeviceManagementRequest&,
               SendRequestCallback),
              (override));
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_DM_SERVER_CLIENT_H_
