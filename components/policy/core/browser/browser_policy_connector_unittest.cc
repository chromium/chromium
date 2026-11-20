// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/browser_policy_connector.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "components/policy/buildflags/buildflags.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/schema.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

class TestBrowserPolicyConnector : public BrowserPolicyConnector {
 public:
  TestBrowserPolicyConnector()
      : BrowserPolicyConnector(base::BindRepeating(
            [](const Schema&)
                -> std::unique_ptr<ConfigurationPolicyHandlerList> {
              return nullptr;
            })) {}
  ~TestBrowserPolicyConnector() override = default;

  void Init(PrefService* local_state,
            scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      override {}
  bool IsDeviceEnterpriseManaged() const override { return false; }
  bool HasMachineLevelPolicies() override { return false; }
  bool IsCommandLineSwitchSupported() const override { return true; }
};

}  // namespace

TEST(BrowserPolicyConnectorTest, DefaultUrls) {
  base::test::TaskEnvironment task_environment;
  TestBrowserPolicyConnector connector;
  EXPECT_EQ(connector.GetDeviceManagementUrl(),
            "https://m.google.com/devicemanagement/data/api");
  EXPECT_EQ(connector.GetRealtimeReportingUrl(),
            "https://chromereporting-pa.googleapis.com/v1/events");
}

#if BUILDFLAG(ALLOW_FAKE_DMSERVER)
TEST(BrowserPolicyConnectorTest, UseFakeDmserverSetsDefaultUrls) {
  base::test::TaskEnvironment task_environment;
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kUseFakeDmserver);
  TestBrowserPolicyConnector connector;
  EXPECT_EQ(connector.GetDeviceManagementUrl(),
            "http://127.0.0.1:6112/device_management");
  EXPECT_EQ(connector.GetRealtimeReportingUrl(),
            "http://127.0.0.1:6112/realtime_reporting");
}

TEST(BrowserPolicyConnectorTest, ExplicitUrlOverridesPrecedeFakeDmserver) {
  base::test::TaskEnvironment task_environment;
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kUseFakeDmserver);
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kDeviceManagementUrl, "http://127.0.0.1:9999/custom_dm");
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kRealtimeReportingUrl, "http://127.0.0.1:9999/custom_realtime");
  TestBrowserPolicyConnector connector;
  EXPECT_EQ(connector.GetDeviceManagementUrl(),
            "http://127.0.0.1:9999/custom_dm");
  EXPECT_EQ(connector.GetRealtimeReportingUrl(),
            "http://127.0.0.1:9999/custom_realtime");
}
#endif  // BUILDFLAG(ALLOW_FAKE_DMSERVER)

}  // namespace policy
