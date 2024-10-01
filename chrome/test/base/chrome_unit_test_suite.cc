// Copyright 2013 The Chromium Authors
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
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_configs.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/browser/update_client/chrome_update_query_params_delegate.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/startup_metric_utils/common/startup_metric_utils.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "extensions/buildflags/buildflags.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_handle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/constants/dbus_paths.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_paths.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "crypto/nss_util_internal.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/common/initialize_extensions_client.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension_paths.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

constexpr char kDefaultLocale[] = "en-US";

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
    TestingBrowserProcess::TearDownAndDeleteInstance();
    // Some tests cause ChildThreadImpl to initialize a PowerMonitor.
    base::PowerMonitor::GetInstance()->ShutdownForTesting();
#if BUILDFLAG(IS_WIN)
    // Running tests locally on Windows machines with some degree of
    // accessibility enabled can cause this flag to become implicitly set.
    constexpr ui::AXMode kAllowedFlags(ui::AXMode::kNativeAPIs);
#else
    constexpr ui::AXMode kAllowedFlags(ui::AXMode::kNone);
#endif
    if (ui::AXMode disallowed =
            content::BrowserAccessibilityState::GetInstance()
                ->GetAccessibilityMode() &
            ~kAllowedFlags;
        !disallowed.is_mode_off()) {
      CHECK_EQ(disallowed, ui::AXMode())
          << "Use content::ScopedAccessibilityModeOverride or otherwise ensure "
             "that accessibility is disabled at the end of your test.";
    }
#if BUILDFLAG(IS_CHROMEOS_ASH)
    arc::ClearArcAllowedCheckForTesting();
    crypto::ResetTokenManagerForTesting();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if !BUILDFLAG(IS_ANDROID)
    web_app::SetTrustedWebBundleIdsForTesting({});
#endif  // !BUILDFLAG(IS_ANDROID)
    browser_shutdown::ResetShutdownGlobalsForTesting();
  }
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
  listeners.Append(new content::CheckForLeakedWebUIRegistrations);

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
  startup_metric_utils::GetCommon().RecordApplicationStartTime(
      base::TimeTicks::Now());
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
                                          chrome::DIR_INTERNAL_PLUGINS,
                                          chrome::DIR_USER_DATA);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::RegisterPathProvider();
#endif

#if BUILDFLAG(IS_CHROMEOS)
  chromeos::dbus_paths::RegisterPathProvider();
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::RegisterPathProvider();
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  EnsureExtensionsClientInitialized();
#endif

  content::WebUIControllerFactory::RegisterFactory(
      ChromeWebUIControllerFactory::GetInstance());
  RegisterChromeWebUIConfigs();

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
      resources_pack_path, ui::kScaleFactorNone);
}
