// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "components/component_updater/component_updater_command_line_config_policy.h"
#include "components/component_updater/configurator_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {

const int kDelayOneMinute = 60;
const int kDelayOneHour = kDelayOneMinute * 60;

}  // namespace

class ComponentUpdaterConfiguratorImplTest : public testing::Test {
 public:
  ComponentUpdaterConfiguratorImplTest() {}
  ~ComponentUpdaterConfiguratorImplTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ComponentUpdaterConfiguratorImplTest);
};

TEST_F(ComponentUpdaterConfiguratorImplTest, FastUpdate) {
  // Test the default timing values when no command line argument is present.
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);
  std::unique_ptr<ConfiguratorImpl> config = std::make_unique<ConfiguratorImpl>(
      ComponentUpdaterCommandLineConfigPolicy(&cmdline), false);
  CHECK_EQ(kDelayOneMinute, config->InitialDelay());
  CHECK_EQ(5 * kDelayOneHour, config->NextCheckDelay());
  CHECK_EQ(30 * kDelayOneMinute, config->OnDemandDelay());
  CHECK_EQ(15 * kDelayOneMinute, config->UpdateDelay());

  // Test the fast-update timings.
  cmdline.AppendSwitchASCII("--component-updater", "fast-update");
  config = std::make_unique<ConfiguratorImpl>(
      ComponentUpdaterCommandLineConfigPolicy(&cmdline), false);
  CHECK_EQ(10, config->InitialDelay());
  CHECK_EQ(5 * kDelayOneHour, config->NextCheckDelay());
  CHECK_EQ(2, config->OnDemandDelay());
  CHECK_EQ(10, config->UpdateDelay());
}

TEST_F(ComponentUpdaterConfiguratorImplTest, FastUpdateWithCustomPolicy) {
  // Test the default timing values when no command line argument is present
  // (default).
  class DefaultCommandLineConfigPolicy
      : public update_client::CommandLineConfigPolicy {
   public:
    DefaultCommandLineConfigPolicy() {}

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
  CHECK_EQ(kDelayOneMinute, config->InitialDelay());
  CHECK_EQ(5 * kDelayOneHour, config->NextCheckDelay());
  CHECK_EQ(30 * kDelayOneMinute, config->OnDemandDelay());
  CHECK_EQ(15 * kDelayOneMinute, config->UpdateDelay());

  // Test the fast-update timings.
  class FastUpdateCommandLineConfigurator
      : public DefaultCommandLineConfigPolicy {
   public:
    FastUpdateCommandLineConfigurator() {}

    bool FastUpdate() const override { return true; }
  };
  config = std::make_unique<ConfiguratorImpl>(
      FastUpdateCommandLineConfigurator(), false);
  CHECK_EQ(10, config->InitialDelay());
  CHECK_EQ(5 * kDelayOneHour, config->NextCheckDelay());
  CHECK_EQ(2, config->OnDemandDelay());
  CHECK_EQ(10, config->UpdateDelay());
}

TEST_F(ComponentUpdaterConfiguratorImplTest, InitialDelay) {
  std::unique_ptr<ConfiguratorImpl> config = std::make_unique<ConfiguratorImpl>(
      update_client::CommandLineConfigPolicy(), false);
  CHECK_EQ(kDelayOneMinute, config->InitialDelay());

  class CommandLineConfigPolicy
      : public update_client::CommandLineConfigPolicy {
   public:
    CommandLineConfigPolicy() {}

    // update_client::CommandLineConfigPolicy overrides.
    bool BackgroundDownloadsEnabled() const override { return false; }
    bool DeltaUpdatesEnabled() const override { return false; }
    bool FastUpdate() const override { return fast_update_; }
    bool PingsEnabled() const override { return false; }
    bool TestRequest() const override { return false; }
    GURL UrlSourceOverride() const override { return GURL(); }
    int InitialDelay() const override { return initial_delay_; }

    void set_fast_update(bool fast_update) { fast_update_ = fast_update; }
    void set_initial_delay(int initial_delay) {
      initial_delay_ = initial_delay;
    }

   private:
    int initial_delay_ = 0;
    bool fast_update_ = false;
  };

  {
    CommandLineConfigPolicy clcp;
    clcp.set_fast_update(true);
    config = std::make_unique<ConfiguratorImpl>(clcp, false);
    CHECK_EQ(10, config->InitialDelay());
  }

  {
    CommandLineConfigPolicy clcp;
    clcp.set_fast_update(false);
    config = std::make_unique<ConfiguratorImpl>(clcp, false);
    CHECK_EQ(kDelayOneMinute, config->InitialDelay());
  }

  {
    CommandLineConfigPolicy clcp;
    clcp.set_initial_delay(2 * kDelayOneMinute);
    config = std::make_unique<ConfiguratorImpl>(clcp, false);
    CHECK_EQ(2 * kDelayOneMinute, config->InitialDelay());
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
    int InitialDelay() const override { return 0; }

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

}  // namespace component_updater
