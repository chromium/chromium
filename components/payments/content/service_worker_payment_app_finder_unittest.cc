// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/service_worker_payment_app_finder.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace payments {

class ServiceWorkerPaymentAppFinderTest : public testing::Test {
 protected:
  void RemoveAppsWithoutMatchingMethodData(
      const std::vector<mojom::PaymentMethodDataPtr>& requested_method_data,
      content::InstalledPaymentAppsFinder::PaymentApps* apps) {
    ServiceWorkerPaymentAppFinder::RemoveAppsWithoutMatchingMethodData(
        requested_method_data, apps);
  }
};

TEST_F(ServiceWorkerPaymentAppFinderTest,
       RemoveAppsWithoutMatchingMethodData_NoApps) {
  std::vector<mojom::PaymentMethodDataPtr> requested_methods;
  requested_methods.emplace_back(mojom::PaymentMethodData::New());
  requested_methods.back()->supported_method = "method";
  content::InstalledPaymentAppsFinder::PaymentApps no_apps;

  RemoveAppsWithoutMatchingMethodData(requested_methods, &no_apps);

  EXPECT_TRUE(no_apps.empty());
}

TEST_F(ServiceWorkerPaymentAppFinderTest,
       RemoveAppsWithoutMatchingMethodData_NoMethods) {
  std::vector<mojom::PaymentMethodDataPtr> no_requested_methods;
  content::InstalledPaymentAppsFinder::PaymentApps apps;
  apps[0] = std::make_unique<content::StoredPaymentApp>();
  apps[0]->enabled_methods = {"method1", "method2"};

  RemoveAppsWithoutMatchingMethodData(no_requested_methods, &apps);

  EXPECT_TRUE(apps.empty());
}

TEST_F(ServiceWorkerPaymentAppFinderTest,
       RemoveAppsWithoutMatchingMethodData_IntersectionOfMethods) {
  std::vector<mojom::PaymentMethodDataPtr> requested_methods;
  requested_methods.emplace_back(mojom::PaymentMethodData::New());
  requested_methods.back()->supported_method = "method1";
  requested_methods.emplace_back(mojom::PaymentMethodData::New());
  requested_methods.back()->supported_method = "method2";
  requested_methods.emplace_back(mojom::PaymentMethodData::New());
  requested_methods.back()->supported_method = "method3";
  content::InstalledPaymentAppsFinder::PaymentApps apps;
  apps[0] = std::make_unique<content::StoredPaymentApp>();
  apps[0]->enabled_methods = {"method2"};
  apps[1] = std::make_unique<content::StoredPaymentApp>();
  apps[1]->enabled_methods = {"method3"};
  apps[2] = std::make_unique<content::StoredPaymentApp>();
  apps[2]->enabled_methods = {"method4"};

  RemoveAppsWithoutMatchingMethodData(requested_methods, &apps);

  EXPECT_EQ(2U, apps.size());
  ASSERT_NE(apps.end(), apps.find(0));
  EXPECT_EQ(std::vector<std::string>{"method2"},
            apps.find(0)->second->enabled_methods);
  ASSERT_NE(apps.end(), apps.find(1));
  EXPECT_EQ(std::vector<std::string>{"method3"},
            apps.find(1)->second->enabled_methods);
}

TEST_F(ServiceWorkerPaymentAppFinderTest,
       RemoveAppsWithoutMatchingMethodData_NoCapabilitiesNetworksOrTypes) {
  std::vector<mojom::PaymentMethodDataPtr> requested_methods;
  requested_methods.emplace_back(mojom::PaymentMethodData::New());
  requested_methods.back()->supported_method = "basic-card";
  content::InstalledPaymentAppsFinder::PaymentApps apps;
  apps[0] = std::make_unique<content::StoredPaymentApp>();
  apps[0]->enabled_methods = {"basic-card"};

  RemoveAppsWithoutMatchingMethodData(requested_methods, &apps);

  EXPECT_EQ(1U, apps.size());
  ASSERT_NE(apps.end(), apps.find(0));
  EXPECT_EQ(std::vector<std::string>{"basic-card"},
            apps.find(0)->second->enabled_methods);
}

TEST_F(ServiceWorkerPaymentAppFinderTest,
       RemoveAppsWithoutMatchingMethodData_NoRequestedNetwork) {
  std::vector<mojom::PaymentMethodDataPtr> requested_methods;
  requested_methods.emplace_back(mojom::PaymentMethodData::New());
  requested_methods.back()->supported_method = "basic-card";
  content::InstalledPaymentAppsFinder::PaymentApps apps;
  apps[0] = std::make_unique<content::StoredPaymentApp>();
  apps[0]->enabled_methods = {"basic-card"};
  apps[0]->capabilities.emplace_back();
  apps[0]->capabilities.back().supported_card_networks = {
      static_cast<int32_t>(mojom::BasicCardNetwork::VISA)};

  RemoveAppsWithoutMatchingMethodData(requested_methods, &apps);

  EXPECT_EQ(1U, apps.size());
  ASSERT_NE(apps.end(), apps.find(0));
  const auto& actual = apps.find(0)->second;
  EXPECT_EQ(std::vector<std::string>{"basic-card"}, actual->enabled_methods);
  ASSERT_EQ(1U, actual->capabilities.size());
  const auto& capability = actual->capabilities.back();
  ASSERT_EQ(1U, actual->capabilities.back().supported_card_networks.size());
  EXPECT_EQ(static_cast<int32_t>(mojom::BasicCardNetwork::VISA),
            capability.supported_card_networks[0]);
}

TEST_F(ServiceWorkerPaymentAppFinderTest,
       RemoveAppsWithoutMatchingMethodData_IntersectionOfNetworks) {
  std::vector<mojom::PaymentMethodDataPtr> requested_methods;
  requested_methods.emplace_back(mojom::PaymentMethodData::New());
  requested_methods.back()->supported_method = "basic-card";
  requested_methods.back()->supported_networks = {
      mojom::BasicCardNetwork::AMEX, mojom::BasicCardNetwork::DINERS};
  content::InstalledPaymentAppsFinder::PaymentApps apps;
  apps[0] = std::make_unique<content::StoredPaymentApp>();
  apps[0]->enabled_methods = {"basic-card"};
  apps[0]->capabilities.emplace_back();
  apps[0]->capabilities.back().supported_card_networks = {
      static_cast<int32_t>(mojom::BasicCardNetwork::DINERS),
      static_cast<int32_t>(mojom::BasicCardNetwork::VISA)};

  RemoveAppsWithoutMatchingMethodData(requested_methods, &apps);

  EXPECT_EQ(1U, apps.size());
  ASSERT_NE(apps.end(), apps.find(0));
  const auto& actual = apps.find(0)->second;
  EXPECT_EQ(std::vector<std::string>{"basic-card"}, actual->enabled_methods);
  ASSERT_EQ(1U, actual->capabilities.size());
  const auto& capability = actual->capabilities.back();
  EXPECT_EQ((std::vector<int32_t>{
                static_cast<int32_t>(mojom::BasicCardNetwork::DINERS),
                static_cast<int32_t>(mojom::BasicCardNetwork::VISA)}),
            capability.supported_card_networks);
}

TEST_F(ServiceWorkerPaymentAppFinderTest,
       RemoveAppsWithoutMatchingMethodData_NonBasicCardIgnoresCapabilities) {
  std::vector<mojom::PaymentMethodDataPtr> requested_methods;
  requested_methods.emplace_back(mojom::PaymentMethodData::New());
  requested_methods.back()->supported_method = "unknown-method";
  content::InstalledPaymentAppsFinder::PaymentApps apps;
  apps[0] = std::make_unique<content::StoredPaymentApp>();
  apps[0]->enabled_methods = {"unknown-method"};
  apps[0]->capabilities.emplace_back();

  RemoveAppsWithoutMatchingMethodData(requested_methods, &apps);

  EXPECT_EQ(1U, apps.size());
  ASSERT_NE(apps.end(), apps.find(0));
  EXPECT_EQ(std::vector<std::string>{"unknown-method"},
            apps.find(0)->second->enabled_methods);
}

}  // namespace payments
