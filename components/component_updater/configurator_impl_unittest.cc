// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/configurator_impl.h"

#include <memory>

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/component_updater/component_updater_command_line_config_policy.h"
#include "components/component_updater/component_updater_switches.h"
#include "components/update_client/command_line_config_policy.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

class ComponentUpdaterConfiguratorImplTest : public testing::Test {
 public:
  ComponentUpdaterConfiguratorImplTest() = default;

  ComponentUpdaterConfiguratorImplTest(
      const ComponentUpdaterConfiguratorImplTest&) = delete;
  ComponentUpdaterConfiguratorImplTest& operator=(
      const ComponentUpdaterConfiguratorImplTest&) = delete;

  ~ComponentUpdaterConfiguratorImplTest() override = default;

 private:
  base::test::TaskEnvironment environment_;
};

TEST_F(ComponentUpdaterConfiguratorImplTest, FastUpdate) {
  // Test the default timing values when no command line argument is present.
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);
  std::unique_ptr<ConfiguratorImpl> config = std::make_unique<ConfiguratorImpl>(
      ComponentUpdaterCommandLineConfigPolicy(&cmdline), false);
  EXPECT_EQ(base::Minutes(1), config->InitialDelay());
  EXPECT_EQ(base::Hours(5), config->NextCheckDelay());
  EXPECT_EQ(base::Minutes(30), config->OnDemandDelay());
  EXPECT_EQ(base::Minutes(15), config->UpdateDelay());

  // Test the fast-update timings.
  cmdline.AppendSwitchASCII("--component-updater", "fast-update");
  config = std::make_unique<ConfiguratorImpl>(
      ComponentUpdaterCommandLineConfigPolicy(&cmdline), false);
  EXPECT_EQ(base::Seconds(10), config->InitialDelay());
  EXPECT_EQ(base::Hours(5), config->NextCheckDelay());
  EXPECT_EQ(base::Seconds(2), config->OnDemandDelay());
  EXPECT_EQ(base::Seconds(10), config->UpdateDelay());
}

TEST_F(ComponentUpdaterConfiguratorImplTest, FastUpdateWithCustomPolicy) {
  // Test the default timing values when no command line argument is present
  // (default).
  class DefaultCommandLineConfigPolicy
      : public update_client::CommandLineConfigPolicy {
   public:
    DefaultCommandLineConfigPolicy() = default;

    // update_client::CommandLineConfigPolicy overrides.
    bool BackgroundDownloadsEnabled() const override { return false; }
    bool DeltaUpdatesEnabled() const override { return false; }
    bool FastUpdate() const override { return false; }
    bool PingsEnabled() const override { return false; }
    bool TestRequest() const override { return false; }
    GURL UrlSourceOverride() const override { return GURL(); }
  };

  std::unique_ptr<ConfiguratorImpl> config = std::make_unique<ConfiguratorImpl>(
      DefaultCommandLineConfigPolicy(), false);
  EXPECT_EQ(base::Minutes(1), config->InitialDelay());
  EXPECT_EQ(base::Hours(5), config->NextCheckDelay());
  EXPECT_EQ(base::Minutes(30), config->OnDemandDelay());
  EXPECT_EQ(base::Minutes(15), config->UpdateDelay());

  // Test the fast-update timings.
  class FastUpdateCommandLineConfigurator
      : public DefaultCommandLineConfigPolicy {
   public:
    FastUpdateCommandLineConfigurator() = default;

    bool FastUpdate() const override { return true; }
  };
  config = std::make_unique<ConfiguratorImpl>(
      FastUpdateCommandLineConfigurator(), false);
  EXPECT_EQ(base::Seconds(10), config->InitialDelay());
  EXPECT_EQ(base::Hours(5), config->NextCheckDelay());
  EXPECT_EQ(base::Seconds(2), config->OnDemandDelay());
  EXPECT_EQ(base::Seconds(10), config->UpdateDelay());
}

TEST_F(ComponentUpdaterConfiguratorImplTest, InitialDelay) {
  std::unique_ptr<ConfiguratorImpl> config = std::make_unique<ConfiguratorImpl>(
      update_client::CommandLineConfigPolicy(), false);
  EXPECT_EQ(base::Minutes(1), config->InitialDelay());

  class CommandLineConfigPolicy
      : public update_client::CommandLineConfigPolicy {
   public:
    CommandLineConfigPolicy() = default;

    // update_client::CommandLineConfigPolicy overrides.
    bool BackgroundDownloadsEnabled() const override { return false; }
    bool DeltaUpdatesEnabled() const override { return false; }
    bool FastUpdate() const override { return fast_update_; }
    bool PingsEnabled() const override { return false; }
    bool TestRequest() const override { return false; }
    GURL UrlSourceOverride() const override { return GURL(); }
    base::TimeDelta InitialDelay() const override { return initial_delay_; }

    void set_fast_update(bool fast_update) { fast_update_ = fast_update; }
    void set_initial_delay(base::TimeDelta initial_delay) {
      initial_delay_ = initial_delay;
    }

   private:
    base::TimeDelta initial_delay_ = base::Seconds(0);
    bool fast_update_ = false;
  };

  {
    CommandLineConfigPolicy clcp;
    clcp.set_fast_update(true);
    config = std::make_unique<ConfiguratorImpl>(clcp, false);
    EXPECT_EQ(base::Seconds(10), config->InitialDelay());
  }

  {
    CommandLineConfigPolicy clcp;
    clcp.set_fast_update(false);
    config = std::make_unique<ConfiguratorImpl>(clcp, false);
    EXPECT_EQ(base::Minutes(1), config->InitialDelay());
  }

  {
    CommandLineConfigPolicy clcp;
    clcp.set_initial_delay(base::Minutes(2));
    config = std::make_unique<ConfiguratorImpl>(clcp, false);
    EXPECT_EQ(base::Minutes(2), config->InitialDelay());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine* command_line =
        scoped_command_line.GetProcessCommandLine();
    command_line->AppendSwitchASCII(switches::kComponentUpdater,
                                    "initial-delay=3.14");
    config = std::make_unique<ConfiguratorImpl>(
        ComponentUpdaterCommandLineConfigPolicy(command_line), false);
    EXPECT_EQ(base::Seconds(3.14), config->InitialDelay());
  }
}

TEST_F(ComponentUpdaterConfiguratorImplTest, TestRequest) {
  class CommandLineConfigPolicy
      : public update_client::CommandLineConfigPolicy {
   public:
    CommandLineConfigPolicy() = default;

    // update_client::CommandLineConfigPolicy overrides.
    bool BackgroundDownloadsEnabled() const override { return false; }
    bool DeltaUpdatesEnabled() const override { return false; }
    bool FastUpdate() const override { return false; }
    bool PingsEnabled() const override { return false; }
    bool TestRequest() const override { return test_request_; }
    GURL UrlSourceOverride() const override { return GURL(); }
    base::TimeDelta InitialDelay() const override { return base::Seconds(0); }

    void set_test_request(bool test_request) { test_request_ = test_request; }

   private:
    bool test_request_ = false;
  };

  auto config = std::make_unique<ConfiguratorImpl>(
      update_client::CommandLineConfigPolicy(), false);
  EXPECT_TRUE(config->ExtraRequestParams().empty());

  CommandLineConfigPolicy clcp;
  config = std::make_unique<ConfiguratorImpl>(clcp, false);
  auto extra_request_params = config->ExtraRequestParams();
  EXPECT_EQ(0u, extra_request_params.count("testrequest"));
  EXPECT_EQ(0u, extra_request_params.count("testsource"));
  clcp.set_test_request(true);
  config = std::make_unique<ConfiguratorImpl>(clcp, false);
  extra_request_params = config->ExtraRequestParams();
  EXPECT_EQ("1", extra_request_params["testrequest"]);
  EXPECT_EQ("dev", extra_request_params["testsource"]);
}

TEST_F(ComponentUpdaterConfiguratorImplTest, MeteredConnection) {
  std::unique_ptr<net::test::MockNetworkChangeNotifier>
      network_change_notifier = net::test::MockNetworkChangeNotifier::Create();
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);
  std::unique_ptr<ConfiguratorImpl> config = std::make_unique<ConfiguratorImpl>(
      ComponentUpdaterCommandLineConfigPolicy(&cmdline), false);

  network_change_notifier->SetConnectionCost(
      net::NetworkChangeNotifier::CONNECTION_COST_UNKNOWN);
  EXPECT_EQ(false, config->IsConnectionMetered());

  network_change_notifier->SetConnectionCost(
      net::NetworkChangeNotifier::CONNECTION_COST_METERED);
  EXPECT_EQ(true, config->IsConnectionMetered());

  network_change_notifier->SetConnectionCost(
      net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED);
  EXPECT_EQ(false, config->IsConnectionMetered());
}

}  // namespace component_updater
