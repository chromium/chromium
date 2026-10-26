// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/autofill/entity_suppression_manager_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/views/autofill/autofill_ai/entity_suppression_dialog_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_manager_impl.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_sync_bridge.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_sync_bridge_test_util.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#include "components/autofill/core/browser/ui/autofill_external_delegate.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace autofill {
namespace {

// Tests the UI flows that suppress Autofill AI entities from the personal
// context. The flows start at the `AutofillExternalDelegate`, which is what the
// Autofill popup calls.
class EntitySuppressionBrowserTest : public InProcessBrowserTest {
 public:
  EntitySuppressionBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kAutofillAmbientAutofillSuppression);
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    InProcessBrowserTest::SetUpBrowserContextKeyedServices(context);
    // The bridge finishes loading during browser startup, before the test body
    // runs.
    EntitySuppressionManagerFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<EntitySuppressionManagerImpl>(
                  CreateTestEntitySuppressionSyncBridge());
            }));
  }

 protected:
  // Adds a passport to Autofill as if it came from the personal context.
  EntityInstance AddPersonalContextPassport() {
    EntityInstance passport = test::GetPassportEntityInstance(
        {.record_type = EntityInstance::RecordType::kPersonalContext});
    AutofillEntityDataManagerFactory::GetForProfile(browser()->GetProfile())
        ->SetPersonalContextEntitiesForTesting({passport});
    return passport;
  }

  EntitySuppressionManager& suppression_manager() {
    return *EntitySuppressionManagerFactory::GetForProfile(
        browser()->GetProfile());
  }

  // Simulates pressing Shift+Delete on the suggestion for `entity` and returns
  // the confirmation dialog.
  views::Widget* ShiftDeleteSuggestion(const EntityInstance& entity) {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         kEntitySuppressionDialogName);
    external_delegate().RemoveSuggestion(
        CreateSuggestion(SuggestionType::kFillAutofillAi, entity));
    return waiter.WaitIfNeededAndGet();
  }

  // Simulates clicking "Remove info" in the submenu of the suggestion for
  // `entity` and returns the confirmation dialog.
  views::Widget* ClickRemoveInfo(const EntityInstance& entity) {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         kEntitySuppressionDialogName);
    external_delegate().DidAcceptSuggestion(
        CreateSuggestion(SuggestionType::kRemoveAutofillAi, entity),
        {.multi_index = {0, 0}}, FormGlobalId(), FieldGlobalId());
    return waiter.WaitIfNeededAndGet();
  }

  // Clicks Undo on the toast that is shown after an entity was suppressed.
  void ClickUndo() {
    ToastController::From(browser())
        ->GetToastViewForTesting()
        ->action_button_for_testing()
        ->button_controller()
        ->NotifyClick();
  }

 private:
  AutofillExternalDelegate& external_delegate() {
    content::RenderFrameHost* rfh = browser()
                                        ->GetTabStripModel()
                                        ->GetActiveWebContents()
                                        ->GetPrimaryMainFrame();
    auto& manager = static_cast<BrowserAutofillManager&>(
        ContentAutofillDriver::GetForRenderFrameHost(rfh)
            ->GetAutofillManager());
    return *test_api(manager).external_delegate();
  }

  static Suggestion CreateSuggestion(SuggestionType type,
                                     const EntityInstance& entity) {
    Suggestion suggestion(type);
    suggestion.payload = Suggestion::AutofillAiPayload(entity.guid());
    return suggestion;
  }

  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(EntitySuppressionBrowserTest,
                       ShiftDeleteAndConfirmSuppressesEntity) {
  const EntityInstance passport = AddPersonalContextPassport();

  views::test::AcceptDialog(ShiftDeleteSuggestion(passport));

  EXPECT_TRUE(suppression_manager().IsSuppressed(passport));
}

IN_PROC_BROWSER_TEST_F(EntitySuppressionBrowserTest,
                       RemoveInfoAndConfirmSuppressesEntity) {
  const EntityInstance passport = AddPersonalContextPassport();

  views::test::AcceptDialog(ClickRemoveInfo(passport));

  EXPECT_TRUE(suppression_manager().IsSuppressed(passport));
}

IN_PROC_BROWSER_TEST_F(EntitySuppressionBrowserTest,
                       CancelledConfirmationDoesNotSuppressEntity) {
  const EntityInstance passport = AddPersonalContextPassport();

  views::test::CancelDialog(ClickRemoveInfo(passport));

  EXPECT_FALSE(suppression_manager().IsSuppressed(passport));
}

IN_PROC_BROWSER_TEST_F(EntitySuppressionBrowserTest, UndoUnsuppressesEntity) {
  const EntityInstance passport = AddPersonalContextPassport();
  views::test::AcceptDialog(ClickRemoveInfo(passport));
  ASSERT_TRUE(suppression_manager().IsSuppressed(passport));
  ASSERT_EQ(ToastController::From(browser())->GetCurrentToastId(),
            ToastId::kAutofillAiSuggestionRemoved);

  ClickUndo();

  EXPECT_FALSE(suppression_manager().IsSuppressed(passport));
}

}  // namespace
}  // namespace autofill
