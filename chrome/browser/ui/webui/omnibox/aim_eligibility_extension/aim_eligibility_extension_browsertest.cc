// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_bridge.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_frame_page_handler_factory.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility_extension/aim_eligibility_extension_service_worker_page_handler_factory.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/crash/content/browser/error_reporting/javascript_error_report.h"
#include "components/crash/content/browser/error_reporting/js_error_report_processor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/switches.h"
#include "extensions/test/test_extension_dir.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class MockJsErrorReportProcessor : public JsErrorReportProcessor {
 public:
  void SendErrorReport(JavaScriptErrorReport error_report,
                       base::OnceClosure completion_callback,
                       content::BrowserContext* browser_context) override {
    last_report_ = std::move(error_report);
    std::move(completion_callback).Run();
  }

  void SetAsDefault() { JsErrorReportProcessor::SetDefault(this); }
  static void ResetDefault() { JsErrorReportProcessor::SetDefault(nullptr); }

  const JavaScriptErrorReport& last_report() const { return last_report_; }

 protected:
  ~MockJsErrorReportProcessor() override = default;

 private:
  JavaScriptErrorReport last_report_;
};

}  // namespace

class AimEligibilityExtensionBrowserTest : public ExtensionApiTest {
 public:
  AimEligibilityExtensionBrowserTest() {
    ComponentLoader::EnableBackgroundExtensionsForTesting();
    scoped_feature_list_.InitAndEnableFeature(
        omnibox::kAimEligibilityComponentExtension);
  }
  ~AimEligibilityExtensionBrowserTest() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kDisableCrashOnComponentExtensionJsError);
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&AimEligibilityExtensionBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

  virtual void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) {
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(
                     &AimEligibilityExtensionBrowserTest::CreateMockService,
                     base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> CreateMockService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
        *profile->GetPrefs(),
        /*template_url_service=*/nullptr,
        /*url_loader_factory=*/nullptr,
        /*identity_manager=*/nullptr);
  }

  base::CallbackListSubscription create_services_subscription_;
  base::test::ScopedFeatureList scoped_feature_list_;
  bool aim_eligible_ = true;
};

// Tests that multiple page handler instances can exist and that disconnecting
// a pipe cleans up the handler in the bridge.
IN_PROC_BROWSER_TEST_F(AimEligibilityExtensionBrowserTest,
                       PageHandlersAndDisconnect) {
  auto* mock_service = static_cast<MockAimEligibilityService*>(
      AimEligibilityServiceFactory::GetForProfile(profile()));
  EXPECT_CALL(*mock_service, IsServerEligibilityEnabled())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_service, IsAimEligible())
      .WillRepeatedly(testing::Return(true));

  auto* bridge = AimEligibilityExtensionBridge::Get(profile());
  ASSERT_TRUE(bridge);
  auto& factory = bridge->service_worker_page_handler_factory();
  EXPECT_EQ(0u, factory.page_handlers_size_for_testing());

  // Create a first page handler.
  mojo::Remote<aim_eligibility::mojom::PageHandler> page_handler_remote_1;
  mojo::PendingReceiver<aim_eligibility::mojom::Page> page_receiver_1;
  factory.CreatePageHandler(page_receiver_1.InitWithNewPipeAndPassRemote(),
                            page_handler_remote_1.BindNewPipeAndPassReceiver());
  EXPECT_EQ(1u, factory.page_handlers_size_for_testing());

  // Verify it works.
  base::test::TestFuture<aim_eligibility::mojom::EligibilityStatePtr> future_1;
  page_handler_remote_1->GetEligibilityState(future_1.GetCallback());
  EXPECT_TRUE(future_1.Take()->is_eligible);

  // Create a second page handler.
  mojo::Remote<aim_eligibility::mojom::PageHandler> page_handler_remote_2;
  mojo::PendingReceiver<aim_eligibility::mojom::Page> page_receiver_2;
  factory.CreatePageHandler(page_receiver_2.InitWithNewPipeAndPassRemote(),
                            page_handler_remote_2.BindNewPipeAndPassReceiver());
  EXPECT_EQ(2u, factory.page_handlers_size_for_testing());

  // Verify it also works.
  base::test::TestFuture<aim_eligibility::mojom::EligibilityStatePtr> future_2;
  page_handler_remote_2->GetEligibilityState(future_2.GetCallback());
  EXPECT_TRUE(future_2.Take()->is_eligible);

  // Disconnect the first page handler.
  page_handler_remote_1.reset();

  // Wait for the disconnect handler callback to execute.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return factory.page_handlers_size_for_testing() == 1u; }));

  // Disconnect the second page handler.
  page_handler_remote_2.reset();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return factory.page_handlers_size_for_testing() == 0u; }));
}

// Tests that the component extension's UI loads, resolves Mojo JS, and updates
// dynamically when the backend eligibility state changes.
IN_PROC_BROWSER_TEST_F(AimEligibilityExtensionBrowserTest, UiParity) {
  auto* mock_service = static_cast<MockAimEligibilityService*>(
      AimEligibilityServiceFactory::GetForProfile(profile()));
  EXPECT_CALL(*mock_service, IsServerEligibilityEnabled())
      .WillRepeatedly(testing::Return(false));
  aim_eligible_ = true;
  EXPECT_CALL(*mock_service, IsAimEligible())
      .WillRepeatedly(testing::ReturnPointee(&aim_eligible_));

  // Intercept the registration of eligibility changed callbacks to invoke them
  // manually.
  base::RepeatingClosureList eligibility_changed_callbacks;
  EXPECT_CALL(*mock_service, RegisterEligibilityChangedCallback(testing::_))
      .WillRepeatedly(
          [&eligibility_changed_callbacks](base::RepeatingClosure callback) {
            return eligibility_changed_callbacks.Add(std::move(callback));
          });

  // Verify the component extension is loaded.
  auto* registry = ExtensionRegistry::Get(profile());
  const Extension* extension = registry->enabled_extensions().GetByID(
      extension_misc::kAimEligibilityExtensionId);
  ASSERT_TRUE(extension);

  // Navigate the active tab to the extension's popup HTML page.
  GURL popup_url = extension->GetResourceURL("aim_eligibility.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), popup_url));

  // Local helper to extract the current eligibility checklist DOM strings.
  auto get_ui_checklist = [this]() {
    std::string js = R"(
      (() => {
        const app = document.querySelector('aim-eligibility-app');
        if (!app) return 'App not found';
        const shadow = app.shadowRoot;
        if (!shadow) return 'Shadow root not found';
        return Array.from(shadow.querySelectorAll('.check-value'))
            .map(el => el.textContent.trim())
            .join('|');
      })()
    )";
    return content::EvalJs(web_contents(), js).ExtractString();
  };

  // Wait for the UI to load and render the expected checklist.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return base::StartsWith(get_ui_checklist(),
                            "✓ Eligible|✓ Allowed|✓ Allowed|✓ Google");
  })) << "Actual: "
      << get_ui_checklist();

  // Change state to Ineligible.
  aim_eligible_ = false;
  eligibility_changed_callbacks.Notify();

  // Wait for the UI checklist to update.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return base::StartsWith(get_ui_checklist(),
                            "✗ Not Eligible|✓ Allowed|✓ Allowed|✓ Google");
  })) << "Actual: "
      << get_ui_checklist();
}

IN_PROC_BROWSER_TEST_F(AimEligibilityExtensionBrowserTest,
                       LoadTimeDataInterception) {
  auto* mock_service = static_cast<MockAimEligibilityService*>(
      AimEligibilityServiceFactory::GetForProfile(profile()));
  EXPECT_CALL(*mock_service, IsServerEligibilityEnabled())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_service, IsAimEligible())
      .WillRepeatedly(testing::Return(true));

  // Verify the component extension is loaded.
  auto* registry = ExtensionRegistry::Get(profile());
  const Extension* extension = registry->enabled_extensions().GetByID(
      extension_misc::kAimEligibilityExtensionId);
  ASSERT_TRUE(extension);

  // Navigate the active tab to the extension's popup HTML page.
  GURL popup_url = extension->GetResourceURL("aim_eligibility.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), popup_url));

  // Wait for the UI to load and showFooter_ to be populated.
  bool show_footer = false;
  ASSERT_TRUE(base::test::RunUntil([&]() {
    auto result = content::EvalJs(
        web_contents(),
        "(() => {"
        "  const app = document.querySelector('aim-eligibility-app');"
        "  if (!app) return null;"
        "  if (typeof app.showFooter_ === 'undefined') return null;"
        "  return app.showFooter_;"
        "})()");
    if (!result.is_bool()) {
      return false;
    }
    show_footer = result.ExtractBool();
    return true;
  }));

  // Assert showAimEligibilityFooter is true.
  EXPECT_TRUE(show_footer);

  // Assert the title placeholder is correctly replaced.
  EXPECT_EQ("AIM Eligibility Diagnostic",
            content::EvalJs(web_contents(), "document.title").ExtractString());
}

IN_PROC_BROWSER_TEST_F(AimEligibilityExtensionBrowserTest,
                       CrossOriginLoadTimeDataIsBlocked) {
  // Load a separate, custom extension (Extension A).
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension A",
    "version": "1.0",
    "manifest_version": 3
  })");
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"),
                     "<html><body></body></html>");
  const Extension* extension_a = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension_a);

  // Navigate to Extension A's page.
  GURL page_url = extension_a->GetResourceURL("page.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), page_url));

  // Try to fetch the strings.m.js of the AIM eligibility component extension
  // (Extension B).
  static constexpr char kScriptTemplate[] = R"(
    (async () => {
      try {
        let response = await fetch('chrome-extension://%s/strings.m.js');
        return response.status.toString();
      } catch (e) {
        return 'failed: ' + e.toString();
      }
    })()
  )";
  std::string script = base::StringPrintf(
      kScriptTemplate, extension_misc::kAimEligibilityExtensionId);
  std::string result = content::EvalJs(web_contents(), script).ExtractString();
  // Fetch should either fail completely due to CORS/network errors, or return
  // 404.
  EXPECT_TRUE(base::StartsWith(result, "failed:") || result == "404")
      << "Actual: " << result;
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_JavaScriptErrorReportingCapturesConsoleError \
  DISABLED_JavaScriptErrorReportingCapturesConsoleError
#else
#define MAYBE_JavaScriptErrorReportingCapturesConsoleError \
  JavaScriptErrorReportingCapturesConsoleError
#endif
IN_PROC_BROWSER_TEST_F(AimEligibilityExtensionBrowserTest,
                       MAYBE_JavaScriptErrorReportingCapturesConsoleError) {
  auto mock_processor = base::MakeRefCounted<MockJsErrorReportProcessor>();
  mock_processor->SetAsDefault();

  auto* mock_service = static_cast<MockAimEligibilityService*>(
      AimEligibilityServiceFactory::GetForProfile(profile()));
  EXPECT_CALL(*mock_service, IsServerEligibilityEnabled())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*mock_service, IsAimEligible())
      .WillRepeatedly(testing::Return(true));

  auto* registry = ExtensionRegistry::Get(profile());
  const Extension* extension = registry->enabled_extensions().GetByID(
      extension_misc::kAimEligibilityExtensionId);
  ASSERT_TRUE(extension);

  auto* bridge = AimEligibilityExtensionBridge::Get(profile());
  ASSERT_TRUE(bridge);

  GURL popup_url = extension->GetResourceURL("aim_eligibility.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), popup_url));

  // Trigger an artificial JS error in the component extension frame.
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "console.error('Artificial JS console error in "
                              "AIM eligibility extension');"));

  const JavaScriptErrorReport& report = mock_processor->last_report();
  EXPECT_EQ(report.message,
            "Artificial JS console error in AIM eligibility extension");
  EXPECT_EQ(report.url, popup_url.spec());
  EXPECT_EQ(report.source_system,
            JavaScriptErrorReport::SourceSystem::kExtensionObserver);

  MockJsErrorReportProcessor::ResetDefault();
}

}  // namespace extensions
