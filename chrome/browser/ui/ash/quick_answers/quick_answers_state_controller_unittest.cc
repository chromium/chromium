// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/quick_answers/quick_answers_state_controller.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "chrome/browser/ui/ash/quick_answers/test/chrome_quick_answers_test_base.h"
#include "components/language/core/browser/pref_names.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace ash {

class TestQuickAnswersStateObserver : public QuickAnswersStateObserver {
 public:
  TestQuickAnswersStateObserver() = default;

  TestQuickAnswersStateObserver(const TestQuickAnswersStateObserver&) = delete;
  TestQuickAnswersStateObserver& operator=(
      const TestQuickAnswersStateObserver&) = delete;

  ~TestQuickAnswersStateObserver() override = default;

  // QuickAnswersStateObserver:
  void OnSettingsEnabled(bool settings_enabled) override {
    settings_enabled_ = settings_enabled;
  }

  bool settings_enabled() const { return settings_enabled_; }

 private:
  bool settings_enabled_ = false;
};

class QuickAnswersStateControllerTest : public ChromeQuickAnswersTestBase {
 protected:
  QuickAnswersStateControllerTest() = default;
  QuickAnswersStateControllerTest(const QuickAnswersStateControllerTest&) =
      delete;
  QuickAnswersStateControllerTest& operator=(
      const QuickAnswersStateControllerTest&) = delete;
  ~QuickAnswersStateControllerTest() override = default;

  // ChromeQuickAnswersTestBase:
  void SetUp() override {
    ChromeQuickAnswersTestBase::SetUp();

    prefs_ = Shell::Get()->session_controller()->GetPrimaryUserPrefService();
    DCHECK(prefs_);

    observer_ = std::make_unique<TestQuickAnswersStateObserver>();
  }

  PrefService* prefs() { return prefs_; }

  TestQuickAnswersStateObserver* observer() { return observer_.get(); }

  void SimulateSessionStart() {
    QuickAnswersState::Get()->RegisterPrefChanges(prefs_);
  }

 private:
  PrefService* prefs_ = nullptr;
  std::unique_ptr<TestQuickAnswersStateObserver> observer_;
};

TEST_F(QuickAnswersStateControllerTest, InitObserver) {
  EXPECT_FALSE(ash::QuickAnswersState::Get()->settings_enabled());
  prefs()->SetBoolean(quick_answers::prefs::kQuickAnswersEnabled, true);

  // The observer class should get an instant notification about the current
  // pref value.
  QuickAnswersState::Get()->AddObserver(observer());
  EXPECT_TRUE(ash::QuickAnswersState::Get()->settings_enabled());
  EXPECT_TRUE(observer()->settings_enabled());

  QuickAnswersState::Get()->RemoveObserver(observer());
}

TEST_F(QuickAnswersStateControllerTest, NotifySettingsEnabled) {
  QuickAnswersState::Get()->AddObserver(observer());

  EXPECT_FALSE(ash::QuickAnswersState::Get()->settings_enabled());
  EXPECT_FALSE(observer()->settings_enabled());

  // The observer class should get an notification when the pref value changes.
  prefs()->SetBoolean(quick_answers::prefs::kQuickAnswersEnabled, true);
  EXPECT_TRUE(ash::QuickAnswersState::Get()->settings_enabled());
  EXPECT_TRUE(observer()->settings_enabled());

  QuickAnswersState::Get()->RemoveObserver(observer());
}

TEST_F(QuickAnswersStateControllerTest, LocaleEligible) {
  UErrorCode error_code = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale(ULOC_US), error_code);
  prefs()->SetString(language::prefs::kApplicationLocale, "en");

  SimulateSessionStart();
  EXPECT_TRUE(ash::QuickAnswersState::Get()->is_eligible());
}

TEST_F(QuickAnswersStateControllerTest, LocaleIneligible) {
  UErrorCode error_code = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale(ULOC_CHINESE), error_code);
  prefs()->SetString(language::prefs::kApplicationLocale, "zh");

  SimulateSessionStart();
  EXPECT_FALSE(ash::QuickAnswersState::Get()->is_eligible());
}

}  // namespace ash
