// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/accessibility_handler.h"

#include <memory>
#include <set>

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/input_method/mock_input_method_engine.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_util.h"

using ::testing::Contains;
using ::testing::Not;

namespace chromeos {
namespace settings {

class TestAccessibilityHandler : public AccessibilityHandler {
 public:
  explicit TestAccessibilityHandler(Profile* profile)
      : AccessibilityHandler(profile) {}
  ~TestAccessibilityHandler() override = default;
};

class AccessibilityHandlerTest : public InProcessBrowserTest {
 public:
  AccessibilityHandlerTest()
      : mock_ime_engine_handler_(
            std::make_unique<input_method::MockInputMethodEngine>()) {}
  AccessibilityHandlerTest(const AccessibilityHandlerTest&) = delete;
  AccessibilityHandlerTest& operator=(const AccessibilityHandlerTest&) = delete;
  ~AccessibilityHandlerTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitWithFeatures(
        {::features::kExperimentalAccessibilityDictationOffline,
         features::kOnDeviceSpeechRecognition},
        {});
  }

  void SetUpOnMainThread() override {
    handler_ = std::make_unique<TestAccessibilityHandler>(browser()->profile());
    handler_->set_web_ui(&web_ui_);
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();
    base::RunLoop().RunUntilIdle();
  }

  void TearDownOnMainThread() override {
    handler_->DisallowJavascript();
    handler_.reset();
  }

  size_t GetNumWebUICalls() { return web_ui_.call_data().size(); }

  void AssertWebUICalls(unsigned int num) {
    ASSERT_EQ(num, web_ui_.call_data().size());
  }

  bool WasWebUIListenerCalledWithStringArgument(
      const std::string& expected_listener,
      const std::string& expected_argument) {
    for (auto it = web_ui_.call_data().rbegin();
         it != web_ui_.call_data().rend(); ++it) {
      const content::TestWebUI::CallData* data = it->get();
      std::string listener = data->arg1()->GetString();
      if (!data->arg2()->is_string()) {
        // Only look for listeners with a single string argument. Continue
        // silently if we find anything else.
        continue;
      }
      std::string listener_argument = data->arg2()->GetString();

      if (data->function_name() == "cr.webUIListenerCallback" &&
          listener == expected_listener &&
          expected_argument == listener_argument) {
        return true;
      }
    }

    return false;
  }

  bool GetWebUIListenerArgumentListValue(const std::string& expected_listener,
                                         const base::ListValue** argument) {
    for (auto it = web_ui_.call_data().rbegin();
         it != web_ui_.call_data().rend(); ++it) {
      const content::TestWebUI::CallData* data = it->get();
      std::string listener;
      data->arg1()->GetAsString(&listener);
      if (data->function_name() == "cr.webUIListenerCallback" &&
          listener == expected_listener) {
        if (!data->arg2()->GetAsList(argument))
          return false;
        return true;
      }
    }

    return false;
  }

  void OnSodaInstalled() { handler_->OnSodaInstalled(); }

  void OnSodaProgress(int progress) { handler_->OnSodaProgress(progress); }

  void OnSodaError() { handler_->OnSodaError(); }

  void MaybeAddDictationLocales() { handler_->MaybeAddDictationLocales(); }

  std::unique_ptr<input_method::MockInputMethodEngine> mock_ime_engine_handler_;

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestAccessibilityHandler> handler_;
  content::TestWebUI web_ui_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// A sanity check that ensures that |handler_| can be used to call into
// AccessibilityHandler and produce the expected results.
// This also verifies that the correct string is sent to the JavaScript end
// of the web UI.
IN_PROC_BROWSER_TEST_F(AccessibilityHandlerTest, OnSodaInstalledTestApi) {
  size_t num_calls = GetNumWebUICalls();
  OnSodaInstalled();
  AssertWebUICalls(num_calls + 1);
  ASSERT_TRUE(WasWebUIListenerCalledWithStringArgument(
      "dictation-setting-subtitle-changed", "Speech files downloaded"));
}

// Verifies that the correct string is sent to the JavaScript end of the web UI.
IN_PROC_BROWSER_TEST_F(AccessibilityHandlerTest, OnSodaProgressTestApi) {
  size_t num_calls = GetNumWebUICalls();
  OnSodaProgress(50);
  AssertWebUICalls(num_calls + 1);
  ASSERT_TRUE(WasWebUIListenerCalledWithStringArgument(
      "dictation-setting-subtitle-changed",
      "Downloading speech recognition files… 50%"));
}

// Verifies that the correct string is sent to the JavaScript end of the web UI.
IN_PROC_BROWSER_TEST_F(AccessibilityHandlerTest, OnSodaErrorTestApi) {
  size_t num_calls = GetNumWebUICalls();
  OnSodaError();
  AssertWebUICalls(num_calls + 1);
  ASSERT_TRUE(WasWebUIListenerCalledWithStringArgument(
      "dictation-setting-subtitle-changed",
      "Can't download speech files. Dictation will continue to work by sending "
      "your voice to Google."));
}

// Ensures that AccessibilityHandler listens to SODA download state and fires
// the correct listener when SODA is installed.
IN_PROC_BROWSER_TEST_F(AccessibilityHandlerTest, OnSodaInstalledNotification) {
  size_t num_calls = GetNumWebUICalls();
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  AssertWebUICalls(num_calls + 1);
  ASSERT_TRUE(WasWebUIListenerCalledWithStringArgument(
      "dictation-setting-subtitle-changed", "Speech files downloaded"));
}

IN_PROC_BROWSER_TEST_F(AccessibilityHandlerTest, DictationLocalesCalculation) {
  input_method::InputMethodManager* ime_manager =
      input_method::InputMethodManager::Get();

  struct {
    std::string application_locale;
    std::vector<std::string> ime_locales;
    std::string preferred_languages;
    std::set<base::StringPiece> expected_recommended_prefixes;
  } kTestCases[] = {
      {"en-US", {}, "", {"en"}},
      {"en", {}, "", {"en"}},
      {"fr", {}, "", {"fr"}},
      {"en", {"en", "en-US"}, "", {"en"}},
      {"en", {"en", "en-US"}, "en", {"en"}},
      {"en", {"en", "es"}, "", {"en", "es"}},
      {"en", {"fr", "es", "fr-FR"}, "", {"en", "es", "fr"}},
      {"it-IT", {"ar", "is-IS", "uk"}, "", {"it", "ar", "is", "uk"}},
      {"en", {"fr", "es", "fr-FR"}, "en-US,it-IT", {"en", "es", "fr", "it"}},
      {"en", {}, "en-US,it-IT,uk", {"en", "it", "uk"}},
  };
  for (const auto& testcase : kTestCases) {
    // Set application locale.
    g_browser_process->SetApplicationLocale(testcase.application_locale);

    // Set up fake IMEs.
    auto state =
        ime_manager->CreateNewState(ProfileManager::GetActiveUserProfile());
    ime_manager->SetState(state);
    input_method::InputMethodDescriptors imes;
    for (auto& locale : testcase.ime_locales) {
      std::string id = "fake-ime-extension-" + locale;
      input_method::InputMethodDescriptor descriptor(id, locale, std::string(),
                                                     std::string(), {locale},
                                                     false, GURL(), GURL());
      imes.push_back(descriptor);
    }
    ime_manager->GetInputMethodUtil()->ResetInputMethods(imes);

    for (auto& descriptor : imes) {
      state->AddInputMethodExtension(descriptor.id(), {descriptor},
                                     mock_ime_engine_handler_.get());
      ASSERT_TRUE(state->EnableInputMethod(descriptor.id()));
    }

    // Set up fake preferred languages.
    browser()->profile()->GetPrefs()->SetString(
        language::prefs::kPreferredLanguages, testcase.preferred_languages);

    MaybeAddDictationLocales();

    const base::ListValue* argument;
    ASSERT_TRUE(
        GetWebUIListenerArgumentListValue("dictation-locales-set", &argument));
    for (auto& it : argument->GetList()) {
      const base::DictionaryValue* dict = &base::Value::AsDictionaryValue(it);
      base::StringPiece language_code =
          language::SplitIntoMainAndTail(*(dict->FindStringPath("value")))
              .first;
      // Only expect some locales to be recommended based on application and
      // IME languages.
      if (*(dict->FindBoolPath("recommended"))) {
        EXPECT_THAT(testcase.expected_recommended_prefixes,
                    Contains(language_code));
      } else {
        EXPECT_THAT(testcase.expected_recommended_prefixes,
                    Not(Contains(language_code)));
      }
    }
  }
}

}  // namespace settings
}  // namespace chromeos
