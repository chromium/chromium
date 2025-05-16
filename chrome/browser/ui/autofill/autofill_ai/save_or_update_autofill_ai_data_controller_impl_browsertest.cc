// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_ai/save_or_update_autofill_ai_data_controller_impl.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_ai {

namespace {
// Helper method used to simulate an update entity dialog. Returns two
// entities where the first is the new one and second is the old one. The new
// one contains one updated and one edited attribute.
std::pair<autofill::EntityInstance, autofill::EntityInstance>
GetUpdateEntities() {
  autofill::test::PassportEntityOptions new_entity_options;
  new_entity_options.name = u"Jon doe";
  autofill::EntityInstance new_entity =
      autofill::test::GetPassportEntityInstance(new_entity_options);

  autofill::test::PassportEntityOptions old_entity_options;
  old_entity_options.name = u"Jonas doe";
  old_entity_options.country = nullptr;
  autofill::EntityInstance old_entity =
      autofill::test::GetPassportEntityInstance(old_entity_options);
  return std::make_pair(new_entity, old_entity);
}
}  // namespace
class SaveOrUpdateAutofillAiDataControllerImplTest : public DialogBrowserTest {
 public:
  SaveOrUpdateAutofillAiDataControllerImplTest() = default;
  SaveOrUpdateAutofillAiDataControllerImplTest(
      const SaveOrUpdateAutofillAiDataControllerImplTest&) = delete;
  SaveOrUpdateAutofillAiDataControllerImplTest& operator=(
      const SaveOrUpdateAutofillAiDataControllerImplTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    SaveOrUpdateAutofillAiDataControllerImpl::CreateForWebContents(
        web_contents,
        /*app_locale=*/"en-US");
    controller_ =
        SaveOrUpdateAutofillAiDataControllerImpl::FromWebContents(web_contents);
    CHECK(controller_);
    if (name == "UpdateEntity") {
      std::pair<autofill::EntityInstance, autofill::EntityInstance> entities =
          GetUpdateEntities();
      controller_->ShowPrompt(std::move(entities.first),
                              std::move(entities.second), base::NullCallback());
      return;
    } else if (name == "SaveNewEntity") {
      controller_->ShowPrompt(autofill::test::GetPassportEntityInstance(),
                              std::nullopt, base::NullCallback());
      return;
    }
    NOTREACHED();
  }

  void TearDownOnMainThread() override {
    controller_ = nullptr;
    DialogBrowserTest::TearDownOnMainThread();
  }

  SaveOrUpdateAutofillAiDataControllerImpl* controller() { return controller_; }

 private:
  raw_ptr<SaveOrUpdateAutofillAiDataControllerImpl> controller_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(SaveOrUpdateAutofillAiDataControllerImplTest,
                       UpdatedAttributesDetails_UpdateEntity) {
  ShowUi("UpdateEntity");
  std::vector<
      SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails>
      update_details = controller()->GetUpdatedAttributesDetails();
  // The first two values should have been edited and updated.
  ASSERT_GT(update_details.size(), 3u);
  EXPECT_EQ(update_details[0].update_type,
            SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateType::
                kNewEntityAttributeUpdated);
  EXPECT_EQ(update_details[0].attribute_value, u"Jon doe");
  EXPECT_EQ(update_details[1].update_type,
            SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateType::
                kNewEntityAttributeAdded);
  EXPECT_EQ(update_details[1].attribute_value, u"Sweden");
  base::HistogramTester histogram_tester;
  controller()->OnBubbleClosed(SaveOrUpdateAutofillAiDataController::
                                   AutofillAiBubbleClosedReason::kAccepted);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.UpdatePrompt.Passport",
      SaveOrUpdateAutofillAiDataController::AutofillAiBubbleClosedReason::
          kAccepted,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.UpdatePrompt.AllEntities",
      SaveOrUpdateAutofillAiDataController::AutofillAiBubbleClosedReason::
          kAccepted,
      1);
}

IN_PROC_BROWSER_TEST_F(SaveOrUpdateAutofillAiDataControllerImplTest,
                       UpdatedAttributesDetails_SaveNewEntity) {
  ShowUi("SaveNewEntity");
  std::vector<
      SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails>
      update_details = controller()->GetUpdatedAttributesDetails();
  // In the save new entity case, all values are  from a new entity and are new.
  for (const SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails&
           detail : update_details) {
    EXPECT_EQ(detail.update_type,
              SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateType::
                  kNewEntityAttributeAdded);
  }
  base::HistogramTester histogram_tester;
  controller()->OnBubbleClosed(SaveOrUpdateAutofillAiDataController::
                                   AutofillAiBubbleClosedReason::kAccepted);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.SavePrompt.Passport",
      SaveOrUpdateAutofillAiDataController::AutofillAiBubbleClosedReason::
          kAccepted,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.SavePrompt.AllEntities",
      SaveOrUpdateAutofillAiDataController::AutofillAiBubbleClosedReason::
          kAccepted,
      1);
}
}  // namespace autofill_ai
