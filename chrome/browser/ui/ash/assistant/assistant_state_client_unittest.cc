// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_state_client.h"

#include <memory>

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

class AssistantStateClientTest : public ChromeAshTestBase {
 public:
  AssistantStateClientTest()
      : fake_user_manager_(std::make_unique<ash::FakeChromeUserManager>()) {}
  ~AssistantStateClientTest() override = default;

  void SetUp() override {
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    ChromeAshTestBase::SetUp();

    // Setup test profile.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("user@gmail.com");
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestArcProfile"));
    profile_ = profile_builder.Build();

    // Setup dependencies
    arc_session_manager_ = arc::CreateTestArcSessionManager(
        std::make_unique<arc::ArcSessionRunner>(
            base::BindRepeating(arc::FakeArcSession::Create)));
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "1234567890"));
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);

    assistant_state_client_ = std::make_unique<AssistantStateClient>();
    assistant_state_client_->SetProfile(profile_.get());
  }

  void TearDown() override {
    assistant_state_client_.reset();
    profile_.reset();
    arc_session_manager_->Shutdown();
    arc_session_manager_.reset();
    ChromeAshTestBase::TearDown();
    ash::ConciergeClient::Shutdown();
  }

  AssistantStateClient* assistant_state_client() {
    return assistant_state_client_.get();
  }

  Profile* profile() { return profile_.get(); }

  arc::ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }

 private:
  ash::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
  user_manager::ScopedUserManager fake_user_manager_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;
  std::unique_ptr<AssistantStateClient> assistant_state_client_;
};

TEST_F(AssistantStateClientTest, LocaleChanged) {
  PrefService* prefs = profile()->GetPrefs();

  ASSERT_EQ("", prefs->GetString(language::prefs::kApplicationLocale));
  prefs->SetString(language::prefs::kApplicationLocale, "en-CA");
  ASSERT_EQ("en-CA", prefs->GetString(language::prefs::kApplicationLocale));
  EXPECT_EQ("en-CA", ash::AssistantState::Get()->locale());
}

TEST_F(AssistantStateClientTest, ArcPlayStoreEnabled) {
  ASSERT_TRUE(ash::AssistantState::Get()->arc_play_store_enabled().has_value());
  ASSERT_FALSE(ash::AssistantState::Get()->arc_play_store_enabled().value());

  arc_session_manager()->NotifyArcPlayStoreEnabledChanged(true);

  ASSERT_TRUE(ash::AssistantState::Get()->arc_play_store_enabled().has_value());
  ASSERT_TRUE(ash::AssistantState::Get()->arc_play_store_enabled().value());
}
