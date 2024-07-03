// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/test_variations_service.h"

#include <memory>

#include "components/metrics/clean_exit_beacon.h"
#include "components/variations/service/ui_string_overrider.h"
#include "components/variations/service/variations_service_client.h"
#include "components/web_resource/resource_request_allowed_notifier_test_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"

namespace variations {
namespace {

class TestVariationsServiceClient : public VariationsServiceClient {
 public:
  TestVariationsServiceClient() = default;
  TestVariationsServiceClient(const TestVariationsServiceClient&) = delete;
  TestVariationsServiceClient& operator=(const TestVariationsServiceClient&) =
      delete;
  ~TestVariationsServiceClient() override = default;

  // VariationsServiceClient:
  base::Version GetVersionForSimulation() override { return base::Version(); }
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return nullptr;
  }
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override {
    return nullptr;
  }
  bool OverridesRestrictParameter(std::string* parameter) override {
    return false;
  }
  bool IsEnterprise() override { return false; }
  void RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      PrefService* local_state) override {}

 private:
  version_info::Channel GetChannel() override {
    return version_info::Channel::UNKNOWN;
  }
};

}  // namespace

TestVariationsService::TestVariationsService(PrefService* prefs)
    : variations::VariationsService(
          std::make_unique<TestVariationsServiceClient>(),
          std::make_unique<web_resource::TestRequestAllowedNotifier>(
              prefs,
              network::TestNetworkConnectionTracker::GetInstance()),
          prefs,
          nullptr,
          variations::UIStringOverrider(),
          nullptr) {}

TestVariationsService::~TestVariationsService() = default;

// static
void TestVariationsService::RegisterPrefs(PrefRegistrySimple* registry) {
  // This call is required for full registration of variations service prefs.
  metrics::CleanExitBeacon::RegisterPrefs(registry);
  variations::VariationsService::RegisterPrefs(registry);
}

}  // namespace variations
