// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_data_controller_impl.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/visibility.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {
// Helper method used to simulate an update entity dialog. Returns two
// entities where the first is the new one and second is the old one. The new
// one contains one updated and one edited attribute.
std::pair<EntityInstance, EntityInstance> GetUpdateEntities() {
  test::PassportEntityOptions new_entity_options;
  new_entity_options.name = u"Jon doe";
  EntityInstance new_entity =
      test::GetPassportEntityInstance(new_entity_options);

  test::PassportEntityOptions old_entity_options;
  old_entity_options.name = u"Jonas doe";
  old_entity_options.country = nullptr;
  EntityInstance old_entity =
      test::GetPassportEntityInstance(old_entity_options);
  return std::make_pair(new_entity, old_entity);
}
}  // namespace
class AutofillAiImportDataControllerImplTest
    : public DialogBrowserTest,
      public base::test::WithFeatureOverride {
 public:
  AutofillAiImportDataControllerImplTest()
      : base::test::WithFeatureOverride(
            features::kAutofillShowBubblesBasedOnPriorities) {}

  AutofillAiImportDataControllerImplTest(
      const AutofillAiImportDataControllerImplTest&) = delete;
  AutofillAiImportDataControllerImplTest& operator=(
      const AutofillAiImportDataControllerImplTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    AutofillAiImportDataControllerImpl::CreateForWebContents(
        web_contents,
        /*app_locale=*/"en-US");
    controller_ =
        AutofillAiImportDataControllerImpl::FromWebContents(web_contents);
    CHECK(controller_);
    if (name == "UpdateEntity") {
      std::pair<EntityInstance, EntityInstance> entities = GetUpdateEntities();
      controller_->ShowPrompt(std::move(entities.first),
                              std::move(entities.second), base::NullCallback());
      return;
    } else if (name == "SaveNewEntity") {
      controller_->ShowPrompt(
          test::GetPassportEntityInstance(save_new_entity_options_),
          std::nullopt, base::NullCallback());
      return;
    }
    NOTREACHED();
  }

  void TearDownOnMainThread() override {
    controller_ = nullptr;
    DialogBrowserTest::TearDownOnMainThread();
  }

  bool IsBubbleManagerEnabled() const { return GetParam(); }

  AutofillAiImportDataControllerImpl* controller() { return controller_; }

  // Used in the save prompt case, this method can be called to set specific
  // attributes on the entity to be saved.
  void SetNewEntitiesOptions(
      test::PassportEntityOptions save_new_entity_options) {
    save_new_entity_options_ = save_new_entity_options;
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
  test::PassportEntityOptions save_new_entity_options_ = {};
  raw_ptr<AutofillAiImportDataControllerImpl> controller_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(AutofillAiImportDataControllerImplTest,
                       UpdatedAttributesDetails_UpdateEntity) {
  ShowUi("UpdateEntity");
  std::vector<AutofillAiImportDataController::EntityAttributeUpdateDetails>
      update_details = controller()->GetUpdatedAttributesDetails();
  // The first two values should have been edited and updated.
  ASSERT_GT(update_details.size(), 3u);
  EXPECT_EQ(update_details[0].update_type,
            AutofillAiImportDataController::EntityAttributeUpdateType::
                kNewEntityAttributeUpdated);
  EXPECT_EQ(update_details[0].attribute_value, u"Jon doe");
  EXPECT_EQ(update_details[1].update_type,
            AutofillAiImportDataController::EntityAttributeUpdateType::
                kNewEntityAttributeAdded);
  EXPECT_EQ(update_details[1].attribute_value, u"Sweden");
  controller()->OnBubbleClosed(
      AutofillClient::AutofillAiBubbleClosedReason::kAccepted);
}

IN_PROC_BROWSER_TEST_P(AutofillAiImportDataControllerImplTest,
                       UpdatedAttributesDetails_SaveNewEntity) {
  ShowUi("SaveNewEntity");
  std::vector<AutofillAiImportDataController::EntityAttributeUpdateDetails>
      update_details = controller()->GetUpdatedAttributesDetails();
  // In the save new entity case, all values are from a new entity and are new.
  for (const AutofillAiImportDataController::EntityAttributeUpdateDetails&
           detail : update_details) {
    EXPECT_EQ(detail.update_type,
              AutofillAiImportDataController::EntityAttributeUpdateType::
                  kNewEntityAttributeAdded);
  }
  controller()->OnBubbleClosed(
      AutofillClient::AutofillAiBubbleClosedReason::kAccepted);
}

// When clicking a link in the bubble the user is navigated to a new tab, which
// leads to the bubble to be closed. This test checks that when the user
// navigates back to the tab where the bubble was first shown, the bubble
// reapears.
IN_PROC_BROWSER_TEST_P(AutofillAiImportDataControllerImplTest,
                       LinkClicked_WebContentsBecomesVisible_ReshowBubble) {
  if (GetParam()) {
    GTEST_SKIP() << "BubbleManager doesn't get informed of the tab changes";
  }

  ShowUi("SaveNewEntity");

  ASSERT_TRUE(controller()->IsShowingBubble());
  controller()->OnGoToWalletLinkClicked();
  ASSERT_FALSE(controller()->IsShowingBubble());

  controller()->OnVisibilityChanged(content::Visibility::VISIBLE);
  EXPECT_TRUE(controller()->IsShowingBubble());
}

// Differently from when clicking on a link in the bubble, which leads to the
// bubble being closed. Other reasons for closing it should not lead to the
// bubble being re-shown when the webcontents becomes visible again.
IN_PROC_BROWSER_TEST_P(AutofillAiImportDataControllerImplTest,
                       BubbleDeclined_WebContentsBecomesVisible_DoNotReshowWh) {
  ShowUi("SaveNewEntity");

  ASSERT_TRUE(controller()->IsShowingBubble());
  controller()->OnSaveButtonClicked();
  ASSERT_FALSE(controller()->IsShowingBubble());

  controller()->OnVisibilityChanged(content::Visibility::VISIBLE);
  EXPECT_FALSE(controller()->IsShowingBubble());
}

IN_PROC_BROWSER_TEST_P(AutofillAiImportDataControllerImplTest,
                       WalletableEntity) {
  SetNewEntitiesOptions(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  ShowUi("SaveNewEntity");
  EXPECT_TRUE(controller()->IsWalletableEntity());
}

IN_PROC_BROWSER_TEST_P(AutofillAiImportDataControllerImplTest,
                       IsNotWalletableEntity) {
  SetNewEntitiesOptions({.record_type = EntityInstance::RecordType::kLocal});
  ShowUi("SaveNewEntity");
  EXPECT_FALSE(controller()->IsWalletableEntity());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(AutofillAiImportDataControllerImplTest);

}  // namespace autofill
