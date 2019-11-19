// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test/components_test_suite.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "mojo/core/embedder/embedder.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "url/url_util.h"

#if defined(OS_IOS)
#include "components/test/ios_components_test_initializer.h"
#else
#include "content/public/common/content_client.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/unittest_test_suite.h"
#include "ui/gl/test/gl_surface_test_support.h"
#endif

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace {

// Not using kExtensionScheme and kChromeSearchScheme to avoid the dependency
// to extensions and chrome/common.
const char* const kNonWildcardDomainNonPortSchemes[] = {"chrome-extension",
                                                        "chrome-search"};

class ComponentsTestSuite : public base::TestSuite {
 public:
  ComponentsTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 private:
  void Initialize() override {
    base::TestSuite::Initialize();

    mojo::core::Init();

    // Before registering any schemes, clear GURL's internal state.
    url::ResetForTests();

#if !defined(OS_IOS)
    gl::GLSurfaceTestSupport::InitializeOneOff();

    content::ForceInProcessNetworkService(true);

    // Setup content scheme statics.
    {
      content::ContentClient content_client;
      content::ContentTestSuiteBase::RegisterContentSchemes(&content_client);
    }
#endif

    ui::RegisterPathProvider();

    base::FilePath pak_path;
#if defined(OS_ANDROID)
    base::PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &pak_path);
#else
    base::PathService::Get(base::DIR_MODULE, &pak_path);
#endif

    base::FilePath ui_test_pak_path;
    ASSERT_TRUE(base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_path.AppendASCII("components_tests_resources.pak"),
        ui::SCALE_FACTOR_NONE);

    // These schemes need to be added globally to pass tests of
    // autocomplete_input_unittest.cc and content_settings_pattern*
    url::AddStandardScheme("chrome", url::SCHEME_WITH_HOST);
    url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);
    url::AddStandardScheme("devtools", url::SCHEME_WITH_HOST);
    url::AddStandardScheme("chrome-search", url::SCHEME_WITH_HOST);
    url::AddStandardScheme("chrome-distiller", url::SCHEME_WITH_HOST);

    ContentSettingsPattern::SetNonWildcardDomainNonPortSchemes(
        kNonWildcardDomainNonPortSchemes,
        base::size(kNonWildcardDomainNonPortSchemes));
  }

  void Shutdown() override {
    ui::ResourceBundle::CleanupSharedInstance();
    base::TestSuite::Shutdown();
  }

#if defined(OS_WIN)
  base::win::ScopedCOMInitializer com_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ComponentsTestSuite);
};

class ComponentsUnitTestEventListener : public testing::EmptyTestEventListener {
 public:
  ComponentsUnitTestEventListener() {}
  ~ComponentsUnitTestEventListener() override {}

  void OnTestStart(const testing::TestInfo& test_info) override {
#if defined(OS_IOS)
    ios_initializer_.reset(new IosComponentsTestInitializer());
#else
    content_initializer_.reset(new content::TestContentClientInitializer());
#endif
  }

  void OnTestEnd(const testing::TestInfo& test_info) override {
#if defined(OS_IOS)
    ios_initializer_.reset();
#else
    content_initializer_.reset();
#endif
  }

 private:
#if defined(OS_IOS)
  std::unique_ptr<IosComponentsTestInitializer> ios_initializer_;
#else
  std::unique_ptr<content::TestContentClientInitializer> content_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ComponentsUnitTestEventListener);
};

}  // namespace

base::RunTestSuiteCallback GetLaunchCallback(int argc, char** argv) {
#if !defined(OS_IOS)
  auto test_suite = std::make_unique<content::UnitTestTestSuite>(
      new ComponentsTestSuite(argc, argv));
#else
  auto test_suite = std::make_unique<ComponentsTestSuite>(argc, argv);
#endif

  // The listener will set up common test environment for all components unit
  // tests.
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new ComponentsUnitTestEventListener());

#if !defined(OS_IOS)
  return base::BindOnce(&content::UnitTestTestSuite::Run,
                        std::move(test_suite));
#else
  return base::Bind(&base::TestSuite::Run, std::move(test_suite));
#endif
}
