// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test/components_test_suite.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "components/breadcrumbs/core/crash_reporter_breadcrumb_observer.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "mojo/core/embedder/embedder.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_IOS)
#include "components/test/ios_components_test_initializer.h"
#else
#include "content/public/common/content_client.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/unittest_test_suite.h"
#include "ui/gl/test/gl_surface_test_support.h"
#endif

namespace {

// Not using kExtensionScheme and kChromeSearchScheme to avoid the dependency
// to extensions and chrome/common.
const char* const kNonWildcardDomainNonPortSchemes[] = {
    "chrome-extension", "chrome-search", "chrome", "chrome-untrusted",
    "devtools"};

class ComponentsTestSuite : public base::TestSuite {
 public:
  ComponentsTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}
  ComponentsTestSuite(const ComponentsTestSuite&) = delete;
  ComponentsTestSuite& operator=(const ComponentsTestSuite&) = delete;

 private:
  void Initialize() override {
    base::TestSuite::Initialize();

    mojo::core::Init();

    // These schemes need to be added globally to pass tests of
    // autocomplete_input_unittest.cc and content_settings_pattern*
    // TODO(https://crbug.com/1047702): Move this scheme initialization into the
    //    individual tests that need these schemes.
    url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);
    url::AddStandardScheme("chrome-search", url::SCHEME_WITH_HOST);
    url::AddStandardScheme("chrome-distiller", url::SCHEME_WITH_HOST);

#if !BUILDFLAG(IS_IOS)
    gl::GLSurfaceTestSupport::InitializeOneOff();

    content::ForceInProcessNetworkService(true);

    // Setup content scheme statics.
    {
      content::ContentClient content_client;
      content::ContentTestSuiteBase::RegisterContentSchemes(&content_client);
    }
#else
    url::AddStandardScheme("chrome", url::SCHEME_WITH_HOST);
    url::AddStandardScheme("chrome-untrusted", url::SCHEME_WITH_HOST);
    url::AddStandardScheme("devtools", url::SCHEME_WITH_HOST);

#endif

    ui::RegisterPathProvider();

    base::FilePath pak_path;
#if BUILDFLAG(IS_ANDROID)
    base::PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &pak_path);
#else
    base::PathService::Get(base::DIR_ASSETS, &pak_path);
#endif

    base::FilePath ui_test_pak_path;
    ASSERT_TRUE(base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_path.AppendASCII("components_tests_resources.pak"),
        ui::kScaleFactorNone);

    ContentSettingsPattern::SetNonWildcardDomainNonPortSchemes(
        kNonWildcardDomainNonPortSchemes,
        std::size(kNonWildcardDomainNonPortSchemes));
  }

  void Shutdown() override {
    ui::ResourceBundle::CleanupSharedInstance();
    base::TestSuite::Shutdown();
  }
};

class ComponentsUnitTestEventListener : public testing::EmptyTestEventListener {
 public:
  ComponentsUnitTestEventListener() = default;
  ComponentsUnitTestEventListener(const ComponentsUnitTestEventListener&) =
      delete;
  ComponentsUnitTestEventListener& operator=(
      const ComponentsUnitTestEventListener&) = delete;
  ~ComponentsUnitTestEventListener() override = default;

#if BUILDFLAG(IS_IOS)
  void OnTestStart(const testing::TestInfo& test_info) override {
    ios_initializer_.reset(new IosComponentsTestInitializer());
  }
#endif

  void OnTestEnd(const testing::TestInfo& test_info) override {
    breadcrumbs::BreadcrumbManager::GetInstance().ResetForTesting();
#if BUILDFLAG(IS_IOS)
    ios_initializer_.reset();
#endif
  }

#if BUILDFLAG(IS_IOS)
 private:
  std::unique_ptr<IosComponentsTestInitializer> ios_initializer_;
#endif
};

}  // namespace

base::RunTestSuiteCallback GetLaunchCallback(int argc, char** argv) {
#if !BUILDFLAG(IS_IOS)
  auto test_suite = std::make_unique<content::UnitTestTestSuite>(
      new ComponentsTestSuite(argc, argv),
      base::BindRepeating(
          content::UnitTestTestSuite::CreateTestContentClients));
#else
  auto test_suite = std::make_unique<ComponentsTestSuite>(argc, argv);
#endif

  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new ComponentsUnitTestEventListener());

#if !BUILDFLAG(IS_IOS)
  return base::BindOnce(&content::UnitTestTestSuite::Run,
                        std::move(test_suite));
#else
  return base::BindOnce(&base::TestSuite::Run, std::move(test_suite));
#endif
}
