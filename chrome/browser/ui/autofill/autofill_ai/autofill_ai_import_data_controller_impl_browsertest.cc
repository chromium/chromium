// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_data_controller_impl.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/autofill/autofill_ai/entity_attribute_update_details.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/visibility.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {
// Helper method used to simulate an update entity dialog for a private pass
// (passport). Returns two entities where the first is the new one and second is
// the old one. The new one contains one updated and one edited attribute.
std::pair<EntityInstance, EntityInstance> GetUpdatePassportEntities() {
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

// Helper method used to simulate an update entity dialog for a public pass
// (vehicle). Returns two entities where the first is the new one and second is
// the old one.
std::pair<EntityInstance, EntityInstance> GetUpdateVehicleEntities(
    test::VehicleOptions options = {
        .record_type = EntityInstance::RecordType::kServerWallet}) {
  test::VehicleOptions new_entity_options = options;
  new_entity_options.name = u"Jon doe";
  EntityInstance new_entity =
      test::GetVehicleEntityInstance(new_entity_options);

  test::VehicleOptions old_entity_options = options;
  old_entity_options.name = u"Jonas doe";
  EntityInstance old_entity =
      test::GetVehicleEntityInstance(old_entity_options);
  return std::make_pair(new_entity, old_entity);
}
}  // namespace
class AutofillAiImportDataControllerImplTest : public DialogBrowserTest {
 public:
  AutofillAiImportDataControllerImplTest() {
    scoped_features_.InitAndEnableFeature(
        features::kAutofillEnableWalletDisclosureNoticePublicPass);
  }

  AutofillAiImportDataControllerImplTest(
      const AutofillAiImportDataControllerImplTest&) = delete;
  AutofillAiImportDataControllerImplTest& operator=(
      const AutofillAiImportDataControllerImplTest&) = delete;

  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    AutofillAiImportDataControllerImpl::CreateForWebContents(
        web_contents, /*app_locale=*/"en-US");
    controller_ =
        AutofillAiImportDataControllerImpl::FromWebContents(web_contents);
    CHECK(controller_);
  }

  void TearDownOnMainThread() override {
    controller_ = nullptr;
    DialogBrowserTest::TearDownOnMainThread();
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    if (name == "UpdatePassportEntity") {
      std::pair<EntityInstance, EntityInstance> entities =
          GetUpdatePassportEntities();
      controller_->ShowPrompt(std::move(entities.first),
                              std::move(entities.second),
                              /*close_on_accept=*/true,
                              /*legal_message_lines=*/{}, base::NullCallback());
      return;
    } else if (name == "UpdateVehicleEntity") {
      std::pair<EntityInstance, EntityInstance> entities =
          GetUpdateVehicleEntities();
      controller_->ShowPrompt(std::move(entities.first),
                              std::move(entities.second),
                              /*close_on_accept=*/true,
                              /*legal_message_lines=*/{}, base::NullCallback());
      return;
    } else if (name == "SaveNewPassportEntity") {
      controller_->ShowPrompt(
          test::GetPassportEntityInstance(save_new_passport_options_),
          std::nullopt, /*close_on_accept=*/true,
          /*legal_message_lines=*/{}, base::NullCallback());
      return;
    } else if (name == "SaveNewPassportEntity_NoCloseOnAccept") {
      controller_->ShowPrompt(
          test::GetPassportEntityInstance(save_new_passport_options_),
          std::nullopt, /*close_on_accept=*/false,
          /*legal_message_lines=*/{}, base::NullCallback());
      return;
    } else if (name == "SaveNewVehicleEntity") {
      controller_->ShowPrompt(
          test::GetVehicleEntityInstance(save_new_vehicle_options_),
          std::nullopt, /*close_on_accept=*/true,
          legal_message_lines_, base::NullCallback());
      return;
    }
    NOTREACHED();
  }

  AutofillAiImportDataControllerImpl* controller() { return controller_; }

  // Used in the save prompt case, this method can be called to set specific
  // attributes on the entity to be saved.
  void SetNewPassportOptions(
      test::PassportEntityOptions save_new_passport_options) {
    save_new_passport_options_ = save_new_passport_options;
  }

  // Used in the save prompt case, this method can be called to set specific
  // attributes on the vehicle entity to be saved.
  void SetNewVehicleOptions(test::VehicleOptions save_new_vehicle_options) {
    save_new_vehicle_options_ = save_new_vehicle_options;
  }

  void SetLegalMessageLines(LegalMessageLines legal_message_lines) {
    legal_message_lines_ = std::move(legal_message_lines);
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
  test::PassportEntityOptions save_new_passport_options_ = {};
  test::VehicleOptions save_new_vehicle_options_ = {};
  LegalMessageLines legal_message_lines_;
  raw_ptr<AutofillAiImportDataControllerImpl> controller_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(AutofillAiImportDataControllerImplTest,
                       UpdatedAttributesDetails_UpdateEntity) {
  ShowUi("UpdatePassportEntity");
  std::vector<EntityAttributeUpdateDetails> update_details =
      controller()->GetUpdatedAttributesDetails();
  // The first two values should have been edited and updated.
  ASSERT_GT(update_details.size(), 3u);
  EXPECT_EQ(update_details[0].update_type(),
            EntityAttributeUpdateType::kNewEntityAttributeUpdated);
  EXPECT_EQ(update_details[0].attribute_value(), u"Jon doe");
  EXPECT_EQ(update_details[1].update_type(),
            EntityAttributeUpdateType::kNewEntityAttributeAdded);
  EXPECT_EQ(update_details[1].attribute_value(), u"Sweden");
  controller()->OnBubbleClosed(
      AutofillClient::AutofillAiBubbleResult::kAccepted);
}

IN_PROC_BROWSER_TEST_F(AutofillAiImportDataControllerImplTest,
                       UpdatedAttributesDetails_SaveNewEntity) {
  ShowUi("SaveNewPassportEntity");
  std::vector<EntityAttributeUpdateDetails> update_details =
      controller()->GetUpdatedAttributesDetails();
  // In the save new entity case, all values are from a new entity and are new.
  for (const EntityAttributeUpdateDetails& detail : update_details) {
    EXPECT_EQ(detail.update_type(),
              EntityAttributeUpdateType::kNewEntityAttributeAdded);
  }
  controller()->OnBubbleClosed(
      AutofillClient::AutofillAiBubbleResult::kAccepted);
}

// Differently from when clicking on a link in the bubble, which leads to the
// bubble being closed. Other reasons for closing it should not lead to the
// bubble being re-shown when the webcontents becomes visible again.
IN_PROC_BROWSER_TEST_F(AutofillAiImportDataControllerImplTest,
                       BubbleDeclined_WebContentsBecomesVisible_DoNotReshowWh) {
  ShowUi("SaveNewPassportEntity");

  ASSERT_TRUE(controller()->IsShowingBubble());
  EXPECT_TRUE(controller()->CloseOnAccept());
  controller()->OnSaveButtonClicked();
  ASSERT_FALSE(controller()->IsShowingBubble());

  controller()->OnVisibilityChanged(content::Visibility::VISIBLE);
  EXPECT_FALSE(controller()->IsShowingBubble());
}

IN_PROC_BROWSER_TEST_F(AutofillAiImportDataControllerImplTest,
                       WalletableEntity) {
  SetNewPassportOptions(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  ShowUi("SaveNewPassportEntity");
  EXPECT_TRUE(controller()->IsWalletableEntity());
}

IN_PROC_BROWSER_TEST_F(AutofillAiImportDataControllerImplTest,
                       IsNotWalletableEntity) {
  SetNewPassportOptions({.record_type = EntityInstance::RecordType::kLocal});
  ShowUi("SaveNewPassportEntity");
  EXPECT_FALSE(controller()->IsWalletableEntity());
}

// Tests that calling `ShowPrompt()` when a bubble is already visible result in
// the prompt closed callback being called with the `kUnknown` reason.
IN_PROC_BROWSER_TEST_F(AutofillAiImportDataControllerImplTest,
                       ShowPrompt_BubbleAlreadyVisible) {
  ShowUi("SaveNewPassportEntity");
  ASSERT_TRUE(controller()->IsShowingBubble());

  base::test::TestFuture<AutofillClient::AutofillAiBubbleResult,
                         std::optional<EntityInstance>,
                         const AutofillClient::EntityImportUIContext&>
      prompt_result_future;
  controller()->ShowPrompt(test::GetPassportEntityInstance(), std::nullopt,
                           /*close_on_accept=*/true,
                           /*legal_message_lines=*/{},
                           prompt_result_future.GetCallback());
  EXPECT_EQ(std::get<0>(prompt_result_future.Get()),
            AutofillClient::AutofillAiBubbleResult::kUnknown);
}

// Tests that if the prompt is configured to not close on accept, clicking the
// save button does not close the bubble.
IN_PROC_BROWSER_TEST_F(AutofillAiImportDataControllerImplTest,
                       AcceptPrompt_DoNotCloseBubble) {
  ShowUi("SaveNewPassportEntity_NoCloseOnAccept");

  ASSERT_TRUE(controller()->IsShowingBubble());
  EXPECT_FALSE(controller()->CloseOnAccept());
  controller()->OnSaveButtonClicked();
  // Expect it to stay open
  EXPECT_TRUE(controller()->IsShowingBubble());

  // Manually close the bubble to clean up.
  controller()->OnBubbleClosed(
      AutofillClient::AutofillAiBubbleResult::kAccepted);
}

IN_PROC_BROWSER_TEST_F(AutofillAiImportDataControllerImplTest,
                       IsEligibleForWalletPassDisclosure_Eligible) {
  SetNewVehicleOptions(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  ShowUi("SaveNewVehicleEntity");
  EXPECT_TRUE(controller()->IsEligibleForWalletPassDisclosure());
}

IN_PROC_BROWSER_TEST_F(AutofillAiImportDataControllerImplTest,
                       IsEligibleForWalletPassDisclosure_PrivatePass) {
  SetNewPassportOptions(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  ShowUi("SaveNewPassportEntity");
  EXPECT_FALSE(controller()->IsEligibleForWalletPassDisclosure());
}

IN_PROC_BROWSER_TEST_F(AutofillAiImportDataControllerImplTest,
                       IsEligibleForWalletPassDisclosure_NotWalletable) {
  SetNewVehicleOptions({.record_type = EntityInstance::RecordType::kLocal});
  ShowUi("SaveNewVehicleEntity");
  EXPECT_FALSE(controller()->IsEligibleForWalletPassDisclosure());
}

IN_PROC_BROWSER_TEST_F(AutofillAiImportDataControllerImplTest,
                       IsEligibleForWalletPassDisclosure_UpdatePrompt) {
  ShowUi("UpdateVehicleEntity");
  EXPECT_FALSE(controller()->IsEligibleForWalletPassDisclosure());
}

IN_PROC_BROWSER_TEST_F(AutofillAiImportDataControllerImplTest,
                       IsEligibleForWalletPassDisclosure_ReadOnly) {
  SetNewVehicleOptions(
      {.record_type = EntityInstance::RecordType::kServerWallet,
       .are_attributes_read_only =
           EntityInstance::AreAttributesReadOnly(true)});
  ShowUi("SaveNewVehicleEntity");
  EXPECT_FALSE(controller()->IsEligibleForWalletPassDisclosure());
}

IN_PROC_BROWSER_TEST_F(AutofillAiImportDataControllerImplTest,
                       LegalMessageLines) {
  SetLegalMessageLines({TestLegalMessageLine("Test legal message")});
  SetNewVehicleOptions(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  ShowUi("SaveNewVehicleEntity");
  EXPECT_EQ(controller()->GetLegalMessageLines().size(), 1u);
  EXPECT_EQ(controller()->GetLegalMessageLines()[0].text(),
            u"Test legal message");
  controller()->OnBubbleClosed(
      AutofillClient::AutofillAiBubbleResult::kAccepted);
}

IN_PROC_BROWSER_TEST_F(AutofillAiImportDataControllerImplTest,
                       OnLegalMessageLinkClicked) {
  SetLegalMessageLines({TestLegalMessageLine("Test legal message")});
  SetNewVehicleOptions(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  ShowUi("SaveNewVehicleEntity");
  ASSERT_TRUE(controller()->IsShowingBubble());
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);

  const GURL legal_url("https://example.com/legal");
  controller()->OnLegalMessageLinkClicked(legal_url);

  EXPECT_TRUE(controller()->ShouldReshowOnTabVisible());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      legal_url);
}

IN_PROC_BROWSER_TEST_F(AutofillAiImportDataControllerImplTest,
                       OnGoToWalletLinkClicked) {
  SetNewVehicleOptions(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  ShowUi("SaveNewVehicleEntity");
  ASSERT_TRUE(controller()->IsShowingBubble());
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);

  controller()->OnGoToWalletLinkClicked();

  EXPECT_TRUE(controller()->ShouldReshowOnTabVisible());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      GURL(chrome::kWalletPassesPageURL));
}

class AutofillAiImportDataControllerImplFeatureDisabledTest
    : public AutofillAiImportDataControllerImplTest {
 public:
  AutofillAiImportDataControllerImplFeatureDisabledTest() {
    scoped_features_disabled_.InitAndDisableFeature(
        features::kAutofillEnableWalletDisclosureNoticePublicPass);
  }

 private:
  base::test::ScopedFeatureList scoped_features_disabled_;
};

IN_PROC_BROWSER_TEST_F(AutofillAiImportDataControllerImplFeatureDisabledTest,
                       IsEligibleForWalletPassDisclosure_FeatureDisabled) {
  SetNewVehicleOptions(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  ShowUi("SaveNewVehicleEntity");
  EXPECT_FALSE(controller()->IsEligibleForWalletPassDisclosure());
}

}  // namespace autofill
