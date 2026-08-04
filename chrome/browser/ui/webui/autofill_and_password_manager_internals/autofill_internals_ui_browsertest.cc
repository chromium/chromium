// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/autofill_and_password_manager_internals/internals_ui_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

class AutofillInternalsWebUIBrowserTest : public InProcessBrowserTest {
 public:
  AutofillInternalsWebUIBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{autofill::features::kAutofillAiServerModel,
                              autofill::features::kAutofillAiWithDataSchema},
        /*disabled_features=*/{});
  }

  content::EvalJsResult EvalJs(const std::string& code) {
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return content::EvalJs(contents, code,
                           content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                           1 /* world_id */);
  }

  ::testing::AssertionResult ExecJs(const std::string& code) {
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return content::ExecJs(contents, code,
                           content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                           1 /* world_id */);
  }

  void SpinRunLoop() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(20));
    run_loop.Run();
  }

 private:
  autofill::test::AutofillBrowserTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AutofillInternalsWebUIBrowserTest, ResetCache) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://autofill-internals")));

  // Wait for reset-fake-button to become visible
  constexpr char kGetResetButtonDisplayStyle[] =
      "document.getElementById('reset-cache-fake-button').style.display";
  while ("inline" != EvalJs(kGetResetButtonDisplayStyle)) {
    SpinRunLoop();
  }

  // Trigger reset button.
  constexpr char kClickResetButton[] =
      "document.getElementById('reset-cache-fake-button').click();";
  EXPECT_TRUE(ExecJs(kClickResetButton));

  // Wait for dialog to appear.
  constexpr char kDialogTextVisible[] =
      "document.getElementsByClassName('modal-dialog-text').length > 0";
  while (!EvalJs(kDialogTextVisible).ExtractBool()) {
    SpinRunLoop();
  }

  // Check result text.
  constexpr char kDialogText[] =
      "document.getElementsByClassName('modal-dialog-text')[0].innerText";
  EXPECT_EQ(autofill::kCacheResetDone, EvalJs(kDialogText));

  // Close dialog.
  constexpr char kClickCloseButton[] =
      "document.getElementsByClassName('modal-dialog-close-button')[0]"
      ".click();";
  EXPECT_TRUE(ExecJs(kClickCloseButton));

  // Wait for dialog to disappear.
  while (EvalJs(kDialogTextVisible).ExtractBool()) {
    SpinRunLoop();
  }
}

// Tests the "Check AtMemory permissions" button works as expected.
IN_PROC_BROWSER_TEST_F(AutofillInternalsWebUIBrowserTest,
                       CheckAtMemoryPermissions) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://autofill-internals")));

  // Wait for check-at-memory-permissions button to become visible.
  constexpr char kGetButtonDisplayStyle[] =
      "document.getElementById('check-at-memory-permissions').style.display";
  while ("inline" != EvalJs(kGetButtonDisplayStyle)) {
    SpinRunLoop();
  }

  // Trigger check button to open dialog.
  constexpr char kClickButton[] =
      "document.getElementById('check-at-memory-permissions').click();";
  EXPECT_TRUE(ExecJs(kClickButton));

  // Wait for dialog to appear.
  constexpr char kDialogVisible[] =
      "document.getElementsByClassName('modal-dialog').length > 0";
  while (!EvalJs(kDialogVisible).ExtractBool()) {
    SpinRunLoop();
  }

  // Click check button inside modal dialog.
  constexpr char kClickCheck[] =
      "document.querySelector('.modal-dialog "
      ".fake-button:not(.modal-dialog-close-button)').click();";
  EXPECT_TRUE(ExecJs(kClickCheck));

  // Wait for result text.
  constexpr char kGetResultText[] =
      "document.getElementById('at-memory-permission-result').innerText";
  while ("" == EvalJs(kGetResultText)) {
    SpinRunLoop();
  }

  EXPECT_NE("", EvalJs(kGetResultText));
}

IN_PROC_BROWSER_TEST_F(AutofillInternalsWebUIBrowserTest,
                       GetAutofillAiEntities) {
  autofill::EntityDataManager* entity_data_manager =
      autofill::AutofillEntityDataManagerFactory::GetForProfile(GetProfile());
  ASSERT_TRUE(entity_data_manager);

  autofill::EntityInstance entity_instance =
      autofill::test::GetPassportEntityInstance();
  entity_data_manager->AddOrUpdateEntityInstance(entity_instance);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://autofill-internals")));

  // Switch to the Autofill AI entities tab.
  constexpr char kClickTab[] =
      "Array.from(document.querySelectorAll('#tab-links a'))"
      ".find(a => a.innerText === 'AutofillAI entities').click();";
  EXPECT_TRUE(ExecJs(kClickTab));

  // Wait for the entities table to render.
  constexpr char kTableVisible[] =
      "document.querySelectorAll('#tab-autofill-ai-entities table').length > "
      "0;";
  while (!EvalJs(kTableVisible).ExtractBool()) {
    SpinRunLoop();
  }

  // Verify that the rendered table contains the entity GUID and the redacted
  // number attribute.
  constexpr char kGetTableText[] =
      "document.querySelector('#tab-autofill-ai-entities').innerText;";
  std::string table_text = EvalJs(kGetTableText).ExtractString();
  EXPECT_NE(table_text.find(entity_instance.guid().value()), std::string::npos);
  EXPECT_NE(table_text.find("<redacted>"), std::string::npos);
}

IN_PROC_BROWSER_TEST_F(AutofillInternalsWebUIBrowserTest,
                       ReauthButtonTriggersAuth) {
  autofill::EntityDataManager* entity_data_manager =
      autofill::AutofillEntityDataManagerFactory::GetForProfile(GetProfile());
  ASSERT_TRUE(entity_data_manager);

  autofill::EntityInstance entity_instance =
      autofill::test::GetPassportEntityInstance();
  entity_data_manager->AddOrUpdateEntityInstance(entity_instance);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://autofill-internals")));

  // Switch to the Autofill AI entities tab.
  constexpr char kClickTab[] =
      "Array.from(document.querySelectorAll('#tab-links a'))"
      ".find(a => a.innerText === 'AutofillAI entities').click();";
  EXPECT_TRUE(ExecJs(kClickTab));

  // Wait for the reauth button to render.
  constexpr char kButtonVisible[] =
      "document.querySelectorAll('#tab-autofill-ai-entities .fake-button')"
      ".length > 0;";
  while (!EvalJs(kButtonVisible).ExtractBool()) {
    SpinRunLoop();
  }

  auto mock_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*mock_authenticator, CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_authenticator, AuthenticateWithMessage)
      .WillOnce(base::test::RunOnceCallback<1>(true));
  content::WebUI* web_ui =
      browser()->tab_strip_model()->GetActiveWebContents()->GetWebUI();
  autofill::InternalsUIHandler* handler = nullptr;
  for (const std::unique_ptr<content::WebUIMessageHandler>& handler_ptr :
       *web_ui->GetHandlersForTesting()) {
    if ((handler =
             static_cast<autofill::InternalsUIHandler*>(handler_ptr.get()))) {
      break;
    }
  }
  ASSERT_TRUE(handler);
  handler->set_authenticator_for_testing(std::move(mock_authenticator));

  // Click the reauth button.
  constexpr char kClickReauth[] =
      "document.querySelector('#tab-autofill-ai-entities .fake-button')"
      ".click();";
  EXPECT_TRUE(ExecJs(kClickReauth));

  // Verify that sensitive attributes remain redacted.
  constexpr char kGetTableText[] =
      "document.querySelector('#tab-autofill-ai-entities').innerText;";
  std::string table_text = EvalJs(kGetTableText).ExtractString();
  EXPECT_NE(table_text.find("<redacted>"), std::string::npos);
}

}  // namespace
