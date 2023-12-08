// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extensions_hats_handler.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;

class Profile;

namespace extensions {

class ExtensionsHatsHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  ExtensionsHatsHandlerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    base::test::FeatureRefAndParams extensions_hub{
        features::kHappinessTrackingSurveysExtensionsSafetyHub,
        {{"settings-time", "15s"}}};
    scoped_feature_list_.InitWithFeaturesAndParameters({extensions_hub}, {});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents());
    web_ui_->ClearTrackedCalls();

    browser_window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(profile(), true);
    params.type = Browser::TYPE_NORMAL;
    params.window = browser_window_.get();
    browser_.reset(Browser::Create(params));

    // Stagger install of extensions so the average age is 2 days
    // and the last time an extension was installed was 1 day ago.
    AddExtension("extension1");
    task_environment()->FastForwardBy(base::Days(3));
    AddExtension("extension2");
    task_environment()->FastForwardBy(base::Days(1));

    handler_ = std::make_unique<ExtensionsHatsHandler>(profile());
    handler_->SetWebUI(web_ui_.get());
    handler_->EnableNavigationForTest();

    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockHatsService)));
    EXPECT_CALL(*mock_hats_service_, CanShowAnySurvey(_))
        .WillRepeatedly(testing::Return(true));
  }

  void TearDown() override {
    handler_->SetWebUI(nullptr);
    handler_.reset();
    web_ui_.reset();
    browser_.reset();
    browser_window_.reset();
    mock_hats_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void PrimaryPageChanged(content::Page& page) {
    handler()->PrimaryPageChanged(web_contents()->GetPrimaryPage());
  }

  void ExtensionsSafetyHubTriggerSurvey() {
    handler()->HandleExtensionsSafetyHubTriggerSurvey(args_);
  }

  void ExtensionsSafetyHubExtensionKept() {
    handler()->HandleExtensionsSafetyHubExtensionKept(args_);
  }

  void ExtensionsSafetyHubExtensionRemoved() {
    handler()->HandleExtensionsSafetyHubExtensionRemoved(args_);
  }

  void ExtensionsSafetyHubNonTriggerExtensionRemoved() {
    handler()->HandleExtensionsSafetyHubNonTriggerExtensionRemoved(args_);
  }

  void ExtensionsSafetyHubRemoveAll(int removed_extensions) {
    args_.Append(removed_extensions);
    handler()->HandleExtensionsSafetyHubRemoveAll(args_);
  }

  void AddExtension(const std::string& name) {
    const std::string kId = crx_file::id_util::GenerateId(name);
    scoped_refptr<const Extension> extension = CreateExtension(name);
    ExtensionRegistry::Get(profile())->AddEnabled(extension);
  }

  scoped_refptr<const Extension> CreateExtension(const std::string& name) {
    const std::string kId = crx_file::id_util::GenerateId(name);
    scoped_refptr<const Extension> extension =
        ExtensionBuilder(name).SetID(kId).Build();
    ExtensionPrefs::Get(profile())->OnExtensionInstalled(
        extension.get(), Extension::State::ENABLED, syncer::StringOrdinal(),
        "");
    return extension;
  }

  ExtensionsHatsHandler* handler() { return handler_.get(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<ExtensionsHatsHandler> handler_;
  raw_ptr<MockHatsService> mock_hats_service_;
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;
  base::Value::List args_;
};

TEST_F(ExtensionsHatsHandlerTest, ExtensionsPageLoad) {
  SurveyStringData expected_product_specific_data = {
      {"Average extension age in days", "2"},
      {"Number of extensions installed", "2"},
      {"Time on extension page in minutes", "10"},
      {"Time since last extension was installed in days", "1"},
      {"Client Channel", "unknown"}};
  task_environment()->FastForwardBy(base::Minutes(10));

  // Check that the hats service will show when the review panel is displayed.
  EXPECT_CALL(*mock_hats_service_,
              LaunchDelayedSurveyForWebContents(
                  "HappinessTrackingSurveysExtensionsSafetyHub", web_contents(),
                  15000, _, expected_product_specific_data, true, _, _, _))
      .Times(1);
  ExtensionsSafetyHubTriggerSurvey();
  task_environment()->RunUntilIdle();
}

TEST_F(ExtensionsHatsHandlerTest, OnSurveyInteraction) {
  SurveyStringData expected_product_specific_data = {
      {"Average extension age in days", "2"},
      {"Time since last extension was installed in days", "1"},
      {"Number of extensions installed", "2"},
      {"Time on extension page in minutes", "10"},
      {"Number of extensions removed", "3"},
      {"Time on extension page in minutes", "10"},
      {"Number of extensions kept", "2"},
      {"Number of non-trigger extensions removed", "1"},
      {"Client Channel", "unknown"}};

  task_environment()->FastForwardBy(base::Minutes(10));

  EXPECT_CALL(*mock_hats_service_,
              LaunchDelayedSurveyForWebContents(
                  "HappinessTrackingSurveysExtensionsSafetyHub", web_contents(),
                  15000, _, expected_product_specific_data, true, _, _, _))
      .Times(1);

  ExtensionsSafetyHubExtensionKept();
  ExtensionsSafetyHubExtensionKept();
  ExtensionsSafetyHubRemoveAll(2);
  ExtensionsSafetyHubNonTriggerExtensionRemoved();
  ExtensionsSafetyHubExtensionRemoved();

  // After navigating away from the extensions page, a hats
  // survey should show
  PrimaryPageChanged(web_contents()->GetPrimaryPage());
  task_environment()->RunUntilIdle();
}
}  // namespace extensions
