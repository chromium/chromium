// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/quick_answers/test/chrome_quick_answers_test_base.h"

#include <memory>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/ash/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/ash/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/components/quick_answers/test/fake_quick_answers_state.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

namespace {

class AccountIdAnnotateProfileDelegate : public Profile::Delegate {
 public:
  explicit AccountIdAnnotateProfileDelegate(const AccountId& account_id)
      : account_id_(account_id) {}

  void OnProfileCreationStarted(Profile* profile,
                                Profile::CreateMode create_mode) override {
    ash::AnnotatedAccountId::Set(profile, account_id_);
  }

  void OnProfileCreationFinished(Profile* profile,
                                 Profile::CreateMode create_mode,
                                 bool success,
                                 bool is_new_profile) override {
    // Do nothing.
  }

 private:
  const AccountId account_id_;
};

}  // namespace

ChromeQuickAnswersTestBase::ChromeQuickAnswersTestBase() {
  set_start_session(false);
}

ChromeQuickAnswersTestBase::~ChromeQuickAnswersTestBase() = default;

user_manager::User* ChromeQuickAnswersTestBase::StartUserSession() {
  auto* user = user_manager_->AddGaiaUser(
      AccountId::FromUserEmailGaiaId(TestingProfile::kDefaultProfileUserName,
                                     GaiaId("fakegaia")),
      user_manager::UserType::kRegular);

  // TODO(crbug.com/278643115): Use SessionManager.
  user_manager_->UserLoggedIn(
      user->GetAccountId(),
      user_manager::TestHelper::GetFakeUsernameHash(user->GetAccountId()));
  return user;
}

void ChromeQuickAnswersTestBase::SetUp() {
  // UserManager should be instantiated before Ash system.
  // Note that local_state() provided by AshTestBase is initialized at
  // ctor, so the instance here should be the valid one.
  user_manager_.Reset(
      std::make_unique<user_manager::FakeUserManager>(local_state()));

  ChromeAshTestBase::SetUp();
  ash::ProfileHelper::Get();  // Instantiate BrowserContextHelper.

  auto* user = StartUserSession();

  profile_delegate_ =
      std::make_unique<AccountIdAnnotateProfileDelegate>(user->GetAccountId());
  TestingProfile::Builder profile_builder;
  profile_builder.SetDelegate(profile_delegate_.get());
  profile_builder.SetProfileName(user->GetAccountId().GetUserEmail());
  profile_ = profile_builder.Build();

  // TODO(crbug.com/383442863): the strategy of preference handling needs to be
  // redesigned.
  auto* test_session_controller_client = GetSessionControllerClient();
  test_session_controller_client->SetUnownedUserPrefService(
      user->GetAccountId(), profile_->GetPrefs());
  SimulateUserLogin({.display_email = user->GetDisplayEmail(),
                     .user_type = user->GetType(),
                     .given_name = base::UTF16ToUTF8(user->GetGivenName())},
                    user->GetAccountId());
  CHECK(
      profile_->GetPrefs() ==
      test_session_controller_client->GetUserPrefService(user->GetAccountId()));
  SetUpInitialPrefValues();
  quick_answers_controller_ =
      CreateQuickAnswersControllerImpl(read_write_cards_ui_controller_);
}

void ChromeQuickAnswersTestBase::TearDown() {
  quick_answers_controller_.reset();

  // Menu.
  menu_parent_.reset();
  menu_runner_.reset();
  menu_model_.reset();
  menu_delegate_.reset();

  // AshTestBase cannot handle TearDown of injected PrefService, even if it is
  // created after Ash initialization. So, we cannot destroy Profile in the
  // proper order here (i.e. in the same way with the production). At this
  // moment, we workaround the shutdown ordering issue by destroying Profile
  // after Ash shutdown, as it does not trigger issues.
  // TODO(crbug.com/383442863): the strategy of preference handling needs to be
  // redesigned.
  ChromeAshTestBase::TearDown();

  profile_.reset();
  profile_delegate_.reset();
  user_manager_.Reset();
}

std::unique_ptr<QuickAnswersControllerImpl>
ChromeQuickAnswersTestBase::CreateQuickAnswersControllerImpl(
    chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller) {
  return std::make_unique<QuickAnswersControllerImpl>(
      TestingBrowserProcess::GetGlobal()
          ->GetFeatures()
          ->application_locale_storage(),
      read_write_cards_ui_controller);
}

void ChromeQuickAnswersTestBase::CreateAndShowBasicMenu() {
  menu_delegate_ = std::make_unique<views::Label>();
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(menu_delegate_.get());
  menu_model_->AddItem(0, u"Menu item");
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
  menu_parent_ =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  menu_runner_->RunMenuAt(menu_parent_.get(), nullptr, gfx::Rect(),
                          views::MenuAnchorPosition::kTopLeft,
                          ui::mojom::MenuSourceType::kMouse);
}

void ChromeQuickAnswersTestBase::ResetMenuParent() {
  CHECK(menu_parent_.get() != nullptr);
  menu_parent_.reset();
}
