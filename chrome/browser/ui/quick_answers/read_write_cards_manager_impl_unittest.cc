// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/read_write_cards_manager_impl.h"

#include <cstddef>
#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_controller.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/components/editor_menu/public/cpp/read_write_cards_manager.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/context_menu_params.h"

namespace chromeos {

namespace {

// Compare the result of the fetched controllers with the expectation.
void ExpectControllersEqual(
    const std::vector<ReadWriteCardController*>& expected_controllers,
    std::vector<base::WeakPtr<ReadWriteCardController>> actual_controllers) {
  ASSERT_EQ(expected_controllers.size(), actual_controllers.size());

  for (size_t i = 0; i < expected_controllers.size(); ++i) {
    EXPECT_EQ(expected_controllers[i], actual_controllers[i].get());
  }
}

}  // namespace

class ReadWriteCardsManagerImplTest : public ChromeAshTestBase,
                                      public testing::WithParamInterface<bool> {
 public:
  ReadWriteCardsManagerImplTest() = default;

  ReadWriteCardsManagerImplTest(const ReadWriteCardsManagerImplTest&) = delete;
  ReadWriteCardsManagerImplTest& operator=(
      const ReadWriteCardsManagerImplTest&) = delete;

  ~ReadWriteCardsManagerImplTest() override = default;

  // ChromeAshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(chromeos::features::kMahi,
                                              IsMahiEnabled());

    ChromeAshTestBase::SetUp();

    manager_ = std::make_unique<ReadWriteCardsManagerImpl>();
  }

  bool IsMahiEnabled() { return GetParam(); }

  void TearDown() override {
    manager_.reset();
    ChromeAshTestBase::TearDown();
  }

  QuickAnswersControllerImpl* quick_answers_controller() {
    return manager_->quick_answers_controller_.get();
  }
  chromeos::mahi::MahiMenuController* mahi_menu_controller() {
    return manager_->mahi_menu_controller_.has_value()
               ? &manager_->mahi_menu_controller_.value()
               : nullptr;
  }

 protected:
  std::unique_ptr<ReadWriteCardsManagerImpl> manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(, ReadWriteCardsManagerImplTest, testing::Bool());

TEST_P(ReadWriteCardsManagerImplTest, InputPassword) {
  content::ContextMenuParams params;
  params.form_control_type = blink::mojom::FormControlType::kInputPassword;
  TestingProfile profile;

  // If this is password input, no controller should be fetched.
  manager_->FetchController(
      params, &profile,
      base::BindOnce(&ExpectControllersEqual,
                     std::vector<ReadWriteCardController*>{}));
}

TEST_P(ReadWriteCardsManagerImplTest, QuickAnswersAndMahiControllers) {
  QuickAnswersState::Get()->set_eligibility_for_testing(true);

  content::ContextMenuParams params;
  TestingProfile profile;

  // When Mahi is disabled and no text is selected, no controller should be
  // fetched. When Mahi is enabled and no text is selected, Mahi controller
  // should be fetched.
  manager_->FetchController(
      params, &profile,
      base::BindOnce(
          &ExpectControllersEqual,
          IsMahiEnabled()
              ? std::vector<ReadWriteCardController*>{mahi_menu_controller()}
              : std::vector<ReadWriteCardController*>{}));

  // When Mahi is disabled and text is selected, quick answers controller should
  // be fetched. When Mahi is enabled and text is selected, both Mahi and quick
  // answers controller should be fetched.
  params.selection_text = u"text";
  manager_->FetchController(
      params, &profile,
      base::BindOnce(
          &ExpectControllersEqual,
          IsMahiEnabled()
              ? std::vector<
                    ReadWriteCardController*>{mahi_menu_controller(),
                                              quick_answers_controller()}
              : std::vector<ReadWriteCardController*>{
                    quick_answers_controller()}));
}

}  // namespace chromeos
