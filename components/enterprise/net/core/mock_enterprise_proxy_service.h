// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_MOCK_ENTERPRISE_PROXY_SERVICE_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_MOCK_ENTERPRISE_PROXY_SERVICE_H_

#include "base/observer_list.h"
#include "components/enterprise/net/core/enterprise_proxy_service.h"
#include "net/proxy_resolution/proxy_config.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_net {

class MockEnterpriseProxyService : public EnterpriseProxyService {
 public:
  MockEnterpriseProxyService();
  ~MockEnterpriseProxyService() override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void NotifyObservers();

  MOCK_METHOD(net::ProxyConfig::DynamicRoutingConfig,
              GetDynamicRoutingConfig,
              (),
              (const, override));

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_MOCK_ENTERPRISE_PROXY_SERVICE_H_
