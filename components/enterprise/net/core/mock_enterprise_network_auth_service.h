// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_MOCK_ENTERPRISE_NETWORK_AUTH_SERVICE_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_MOCK_ENTERPRISE_NETWORK_AUTH_SERVICE_H_

#include <vector>

#include "components/enterprise/net/core/enterprise_network_auth_service.h"
#include "components/enterprise/net/core/types.h"
#include "net/http/http_request_headers.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_net {

class MockEnterpriseNetworkAuthService : public EnterpriseNetworkAuthService {
 public:
  MockEnterpriseNetworkAuthService();
  ~MockEnterpriseNetworkAuthService() override;

  MOCK_METHOD(void,
              FetchAccessToken,
              (AuthScope scope, AccessTokenCallback callback),
              (override));
  MOCK_METHOD(net::HttpRequestHeaders,
              ResolveExtraHeaders,
              (const std::vector<ProxyExtraHeader>& extra_headers),
              (const, override));
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_MOCK_ENTERPRISE_NETWORK_AUTH_SERVICE_H_
