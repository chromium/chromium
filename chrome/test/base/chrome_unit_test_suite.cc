// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_unit_test_suite.h"

#include <memory>

#include "base/environment.h"
#include "base/path_service.h"
#include "base/power_monitor/power_monitor.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_measurement.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/browser/update_client/chrome_update_query_params_delegate.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/utility/chrome_content_utility_client.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/update_client/update_query_params.h"
#include "content/public/common/content_paths.h"
#include "extensions/buildflags/buildflags.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_handle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_paths.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/initialize_extensions_client.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/extensions_client.h"
#endif

namespace {

constexpr char kDefaultLocale[] = "en-US";

class ChromeContentBrowserClientWithoutNetworkServiceInitialization
    : public ChromeContentBrowserClient {
 public:
  // content::ContentBrowserClient:
  // Skip some production Network Service code that doesn't work in unit tests.
  void OnNetworkServiceCreated(
      network::mojom::NetworkService* network_service) override {}
};

// Creates a TestingBrowserProcess for each test.
class ChromeUnitTestSuiteInitializer : public testing::EmptyTestEventListener {
 public:
  ChromeUnitTestSuiteInitializer() = default;
  ChromeUnitTestSuiteInitializer(const ChromeUnitTestSuiteInitializer&) =
      delete;
  ChromeUnitTestSuiteInitializer& operator=(
      const ChromeUnitTestSuiteInitializer&) = delete;
  ~ChromeUnitTestSuiteInitializer() override = default;

  void OnTestStart(const testing::TestInfo& test_info) override {
    content_client_ = std::make_unique<ChromeContentClient>();
    content::SetContentClient(content_client_.get());

    browser_content_client_ = std::make_unique<
        ChromeContentBrowserClientWithoutNetworkServiceInitialization>();
    content::SetBrowserClientForTesting(browser_content_client_.get());
    utility_content_client_ = std::make_unique<ChromeContentUtilityClient>();
    content::SetUtilityClientForTesting(utility_content_client_.get());

    TestingBrowserProcess::CreateInstance();
    // Make sure the loaded locale is "en-US".
    if (ui::ResourceBundle::GetSharedInstance().GetLoadedLocaleForTesting() !=
        kDefaultLocale) {
      // Linux uses environment to determine locale.
      std::unique_ptr<base::Environment> env(base::Environment::Create());
      env->SetVar("LANG", kDefaultLocale);
      ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources(
          kDefaultLocale);
    }
  }

  void OnTestEnd(const testing::TestInfo& test_info) override {
    // To ensure that NetworkConnectionTracker doesn't complain in unit_tests
    // about outstanding listeners.
    data_use_measurement::ChromeDataUseMeasurement::DeleteInstance();

    browser_content_client_.reset();
    utility_content_client_.reset();
    content_client_.reset();
    content::SetContentClient(nullptr);

    TestingBrowserProcess::DeleteInstance();
    // Some tests cause ChildThreadImpl to initialize a PowerMonitor.
    base::PowerMonitor::ShutdownForTesting();
  }

 private:
  // Client implementations for the content module.
  std::unique_ptr<ChromeContentClient> content_client_;
  std::unique_ptr<ChromeContentBrowserClient> browser_content_client_;
  std::unique_ptr<ChromeContentUtilityClient> utility_content_client_;
};

}  // namespace

ChromeUnitTestSuite::ChromeUnitTestSuite(int argc, char** argv)
    : ChromeTestSuite(argc, argv) {}

void ChromeUnitTestSuite::Initialize() {
  // Add an additional listener to do the extra initialization for unit tests.
  // It will be started before the base class listeners and ended after the
  // base class listeners.
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new ChromeUnitTestSuiteInitializer);

  {
    ChromeContentClient content_client;
    RegisterContentSchemes(&content_client);
  }
  InitializeProviders();
  RegisterInProcessThreads();

  ChromeTestSuite::Initialize();

  // This needs to run after ChromeTestSuite::Initialize which calls content's
  // intialization which calls base's which initializes ICU.
  InitializeResourceBundle();

  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);
  ProfileShortcutManager::DisableForUnitTests();

  // BrowserView assumes that application start time is set when it is painted.
  // Since RecordApplicationStartTime() would DCHECK if it was invoked from
  // multiple tests in the same process, invoke it once in test suite
  // initialization.
  startup_metric_utils::RecordApplicationStartTime(base::TimeTicks::Now());
}

void ChromeUnitTestSuite::Shutdown() {
  ui::ResourceBundle::CleanupSharedInstance();
  ChromeTestSuite::Shutdown();
}

void ChromeUnitTestSuite::InitializeProviders() {
  chrome::RegisterPathProvider();
  content::RegisterPathProvider();
  ui::RegisterPathProvider();
  component_updater::RegisterPathProvider(chrome::DIR_COMPONENTS,
#if BUILDFLAG(IS_CHROMEOS_ASH)
                                          chromeos::DIR_PREINSTALLED_COMPONENTS,
#else
                                          chrome::DIR_INTERNAL_PLUGINS,
#endif
                                          chrome::DIR_USER_DATA);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::RegisterPathProvider();
  chromeos::dbus_paths::RegisterPathProvider();
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::RegisterPathProvider();

  EnsureExtensionsClientInitialized();
#endif

  content::WebUIControllerFactory::RegisterFactory(
      ChromeWebUIControllerFactory::GetInstance());

  gl::GLSurfaceTestSupport::InitializeOneOff();

  update_client::UpdateQueryParams::SetDelegate(
      ChromeUpdateQueryParamsDelegate::GetInstance());
}

void ChromeUnitTestSuite::InitializeResourceBundle() {
  // Force unittests to run using en-US so if we test against string
  // output, it'll pass regardless of the system language.
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      kDefaultLocale, nullptr, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  base::FilePath resources_pack_path;
  base::PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      resources_pack_path, ui::SCALE_FACTOR_NONE);

  base::FilePath unit_tests_pack_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_MODULE, &unit_tests_pack_path));
  unit_tests_pack_path = unit_tests_pack_path.AppendASCII("unit_tests.pak");
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      unit_tests_pack_path, ui::SCALE_FACTOR_NONE);
}
