// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scalable_iph/scalable_iph_factory.h"

#include "base/check_is_test.h"
#include "base/logging.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace {

constexpr char kScalableIphServiceName[] = "ScalableIphKeyedService";

ScalableIphFactory* g_scalable_iph_factory = nullptr;

}  // namespace

ScalableIphFactory::ScalableIphFactory()
    : BrowserContextKeyedServiceFactory(
          kScalableIphServiceName,
          BrowserContextDependencyManager::GetInstance()) {
  CHECK(!g_scalable_iph_factory);
  g_scalable_iph_factory = this;
}

ScalableIphFactory::~ScalableIphFactory() {
  CHECK(g_scalable_iph_factory);
  g_scalable_iph_factory = nullptr;
}

ScalableIphFactory* ScalableIphFactory::GetInstance() {
  CHECK(g_scalable_iph_factory)
      << "ScalableIphFactory instance must be instantiated by "
         "ScalableIphFactoryImpl::BuildInstance()";
  return g_scalable_iph_factory;
}

scalable_iph::ScalableIph* ScalableIphFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<scalable_iph::ScalableIph*>(
      // Service must be created via `InitializeServiceForBrowserContext`.
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /*create=*/false));
}

void ScalableIphFactory::InitializeServiceForBrowserContext(
    content::BrowserContext* browser_context) {
  // TODO(b/286604737): Disables ScalableIph services if multi-user sign-in is
  // used.

  if (!on_building_service_instance_for_testing_callback_.is_null()) {
    CHECK_IS_TEST();
    on_building_service_instance_for_testing_callback_.Run(browser_context);
  }

  // Create a `ScalableIph` service to start a timer for time tick event. Ignore
  // a return value. It can be nullptr if the browser context (i.e.
  // browser_context) is not eligible for `ScalableIph`.
  GetServiceForBrowserContext(browser_context, /*create=*/true);
}

void ScalableIphFactory::SetOnBuildingServiceInstanceForTestingCallback(
    OnBuildingServiceInstanceForTestingCallback callback) {
  CHECK_IS_TEST();
  CHECK(on_building_service_instance_for_testing_callback_.is_null());
  CHECK(!callback.is_null());
  on_building_service_instance_for_testing_callback_ = std::move(callback);
}
