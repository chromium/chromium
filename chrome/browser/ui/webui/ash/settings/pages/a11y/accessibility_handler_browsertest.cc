// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/a11y/accessibility_handler.h"

#include <memory>
#include <optional>
#include <set>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/containers/adapters.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/input_method/mock_input_method_engine.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ime/ash/input_method_descriptor.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"

using ::testing::Contains;
using ::testing::Not;

namespace ash::settings {

namespace {

// Use a real domain to avoid policy loading problems.
constexpr char kTestUserName[] = "owner@gmail.com";
constexpr char kTestUserGaiaId[] = "9876543210";

}  // namespace

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
        {features::kOnDeviceSpeechRecognition}, {});
  }

  void SetUpOnMainThread() override {
    handler_ = std::make_unique<TestAccessibilityHandler>(browser()->profile());
    handler_->set_web_ui(&web_ui_);
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();
    base::RunLoop().RunUntilIdle();

    // Set the Dictation locale for tests.
    SetDictationLocale("en-US");
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
    for (const std::unique_ptr<content::TestWebUI::CallData>& data :
         base::Reversed(web_ui_.call_data())) {
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
                                         const base::Value::List*& argument) {
    for (const std::unique_ptr<content::TestWebUI::CallData>& data :
         base::Reversed(web_ui_.call_data())) {
      std::string listener;
      if (data->arg1()->is_string()) {
        listener = data->arg1()->GetString();
      }
      if (data->function_name() == "cr.webUIListenerCallback" &&
          listener == expected_listener) {
        if (!data->arg2()->is_list()) {
          return false;
        }
        argument = &data->arg2()->GetList();
        return true;
      }
    }

    return false;
  }

  void MaybeAddDictationLocales() { handler_->MaybeAddDictationLocales(); }

  void SetDictationLocale(const std::string& locale) {
    ProfileManager::GetActiveUserProfile()->GetPrefs()->SetString(
        prefs::kAccessibilityDictationLocale, locale);
  }

  void CreateSession(const AccountId& account_id) {
    auto* session_manager = session_manager::SessionManager::Get();
    session_manager->CreateSession(account_id, account_id.GetUserEmail(),
                                   false);
  }

  void StartUserSession(const AccountId& account_id) {
    profiles::testing::CreateProfileSync(
        g_browser_process->profile_manager(),
        BrowserContextHelper::Get()->GetBrowserContextPathByUserIdHash(
            user_manager::UserManager::Get()
                ->FindUser(account_id)
                ->username_hash()));

    auto* session_manager = session_manager::SessionManager::Get();
    session_manager->NotifyUserProfileLoaded(account_id);
    session_manager->SessionStarted();
  }

  speech::SodaInstaller* soda_installer() {
    return speech::SodaInstaller::GetInstance();
  }

  speech::LanguageCode en_us() { return speech::LanguageCode::kEnUs; }
  speech::LanguageCode fr_fr() { return speech::LanguageCode::kFrFr; }
  content::TestWebUI* web_ui() { return &web_ui_; }

  std::unique_ptr<input_method::MockInputMethodEngine> mock_ime_engine_handler_;

  const AccountId test_account_id_ =
      AccountId::FromUserEmailGaiaId(kTestUserName, kTestUserGaiaId);

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestAccessibilityHandler> handler_;
  content::TestWebUI web_ui_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensures that AccessibilityHandler listens to SODA download state changes, and
// fires the correct listener when SODA AND the language pack matching the
// Dictation locale are installed.
IN_PROC_BROWSER_TEST_F(AccessibilityHandlerTest, OnSodaInstalledNotification) {
  SetDictationLocale("fr-FR");
  size_t num_calls = GetNumWebUICalls();
  // Pretend that the SODA binary was installed. We still need to wait for the
  // correct language pack before doing anything.
  soda_installer()->NotifySodaInstalledForTesting();
  AssertWebUICalls(num_calls);
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  AssertWebUICalls(num_calls);
  soda_installer()->NotifySodaInstalledForTesting(fr_fr());
  AssertWebUICalls(num_calls + 1);
  ASSERT_TRUE(WasWebUIListenerCalledWithStringArgument(
      "dictation-locale-menu-subtitle-changed",
      "French (France) is processed locally and works offline"));
}

// Verifies that the correct string is sent to the JavaScript end of the web UI.
// Ensures we only notify the user of progress for the language pack matching
// the Dictation locale.
IN_PROC_BROWSER_TEST_F(AccessibilityHandlerTest, OnSodaProgressNotification) {
  size_t num_calls = GetNumWebUICalls();
  soda_installer()->NotifySodaProgressForTesting(50, fr_fr());
  AssertWebUICalls(num_calls);
  soda_installer()->NotifySodaProgressForTesting(50, en_us());
  AssertWebUICalls(num_calls + 1);
  soda_installer()->NotifySodaProgressForTesting(50);
  AssertWebUICalls(num_calls + 2);
  ASSERT_TRUE(WasWebUIListenerCalledWithStringArgument(
      "dictation-locale-menu-subtitle-changed",
      "Downloading speech recognition files… 50%"));
}

// Verifies that the correct string is sent to the JavaScript end of the web UI
// when the SODA binary fails to download.
IN_PROC_BROWSER_TEST_F(AccessibilityHandlerTest, OnSodaErrorNotification) {
  size_t num_calls = GetNumWebUICalls();
  soda_installer()->NotifySodaErrorForTesting();
  AssertWebUICalls(num_calls + 1);
  ASSERT_TRUE(WasWebUIListenerCalledWithStringArgument(
      "dictation-locale-menu-subtitle-changed",
      "Couldn’t download English (United States) speech files. Download will "
      "be attempted later. Speech is sent to Google for processing until "
      "download is completed."));
}

// Verifies that the correct listener is fired when the language pack matching
// the Dictation locale fails to download.
IN_PROC_BROWSER_TEST_F(AccessibilityHandlerTest,
                       OnSodaLanguageErrorNotification) {
  size_t num_calls = GetNumWebUICalls();
  // Do nothing if the failed language pack is different than the Dictation
  // locale.
  soda_installer()->NotifySodaErrorForTesting(fr_fr());
  AssertWebUICalls(num_calls);
  // Fire the correct listener when the language pack matching the Dictation
  // locale fails.
  soda_installer()->NotifySodaErrorForTesting(en_us());
  AssertWebUICalls(num_calls + 1);
  ASSERT_TRUE(WasWebUIListenerCalledWithStringArgument(
      "dictation-locale-menu-subtitle-changed",
      "Couldn’t download English (United States) speech files. Download will "
      "be attempted later. Speech is sent to Google for processing until "
      "download is completed."));
}

IN_PROC_BROWSER_TEST_F(AccessibilityHandlerTest, DictationLocalesCalculation) {
  input_method::InputMethodManager* ime_manager =
      input_method::InputMethodManager::Get();

  struct {
    std::string application_locale;
    std::vector<std::string> ime_locales;
    std::string preferred_languages;
    std::set<std::string_view> expected_recommended_prefixes;
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
      input_method::InputMethodDescriptor descriptor(
          id, locale, std::string(), std::string(), {locale}, false, GURL(),
          GURL(), /*handwriting_language=*/std::nullopt);
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

    const base::Value::List* argument = nullptr;
    ASSERT_TRUE(
        GetWebUIListenerArgumentListValue("dictation-locales-set", argument));
    for (const base::Value& it : *argument) {
      const base::Value::Dict& dict = it.GetDict();
      std::string_view language_code =
          language::SplitIntoMainAndTail(*(dict.FindString("value"))).first;
      // Only expect some locales to be recommended based on application and
      // IME languages.
      if (*(dict.FindBool("recommended"))) {
        EXPECT_THAT(testcase.expected_recommended_prefixes,
                    Contains(language_code));
      } else {
        EXPECT_THAT(testcase.expected_recommended_prefixes,
                    Not(Contains(language_code)));
      }
    }
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityHandlerTest,
                       DictationLocalesOfflineAndInstalled) {
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(en_us());
  MaybeAddDictationLocales();
  const base::Value::List* argument = nullptr;
  ASSERT_TRUE(
      GetWebUIListenerArgumentListValue("dictation-locales-set", argument));

  for (auto& it : *argument) {
    const base::Value::Dict& dict = it.GetDict();
    const std::string locale = *dict.FindString("value");
    bool works_offline = dict.FindBool("worksOffline").value();
    bool installed = dict.FindBool("installed").value();
    if (locale == speech::kUsEnglishLocale) {
      EXPECT_TRUE(works_offline);
      EXPECT_TRUE(installed);
    } else {
      // Some locales other than en-us can be installed offline, but should not
      // be.
      EXPECT_FALSE(installed) << " for locale " << locale;
    }
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityHandlerTest, GetStartupSoundEnabled) {
  CreateSession(test_account_id_);
  StartUserSession(test_account_id_);
  AccessibilityManager::Get()->SetStartupSoundEnabled(true);

  size_t call_data_count_before_call = web_ui()->call_data().size();

  base::Value::List empty_args;
  web_ui()->HandleReceivedMessage("getStartupSoundEnabled", empty_args);

  ASSERT_EQ(call_data_count_before_call + 1u, web_ui()->call_data().size());

  const content::TestWebUI::CallData& call_data =
      *(web_ui()->call_data()[call_data_count_before_call]);
  EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
  EXPECT_EQ("startup-sound-setting-retrieved", call_data.arg1()->GetString());
  EXPECT_TRUE(call_data.arg2()->GetBool());
}

}  // namespace ash::settings
