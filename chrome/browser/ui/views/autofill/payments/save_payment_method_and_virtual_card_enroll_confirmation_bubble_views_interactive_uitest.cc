// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl_test_api.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_payment_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_payment_method_and_virtual_card_enroll_confirmation_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_enroll_icon_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/widget_test.h"

namespace autofill {

class SaveCardConfirmationBubbleViewsInteractiveUiTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  SaveCardConfirmationBubbleViewsInteractiveUiTest() {
    const bool is_page_action_migration_enabled = GetParam();
    std::vector<base::test::FeatureRefAndParams> enabled_features = {};
    std::vector<base::test::FeatureRef> disabled_features = {};

    if (is_page_action_migration_enabled) {
      enabled_features.push_back(
          {::features::kPageActionsMigration,
           {
               {::features::kPageActionsMigrationSavePayments.name, "true"},
           }});
    } else {
      disabled_features.emplace_back(::features::kPageActionsMigration);
    }

    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);

    CHECK_EQ(IsPageActionMigrationEnabled(), is_page_action_migration_enabled);
  }
  ~SaveCardConfirmationBubbleViewsInteractiveUiTest() override = default;
  SaveCardConfirmationBubbleViewsInteractiveUiTest(
      const SaveCardConfirmationBubbleViewsInteractiveUiTest&) = delete;
  SaveCardConfirmationBubbleViewsInteractiveUiTest& operator=(
      const SaveCardConfirmationBubbleViewsInteractiveUiTest&) = delete;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    SaveCardBubbleControllerImpl* save_card_controller =
        static_cast<SaveCardBubbleControllerImpl*>(
            SaveCardBubbleControllerImpl::GetOrCreate(
                browser()->tab_strip_model()->GetActiveWebContents()));
    CHECK(save_card_controller);
  }

  SaveCardBubbleControllerImpl* GetController() {
    if (!browser() || !browser()->tab_strip_model() ||
        !browser()->tab_strip_model()->GetActiveWebContents()) {
      return nullptr;
    }

    return SaveCardBubbleControllerImpl::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews* BubbleView() {
    return static_cast<
        SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews*>(
        GetController()->GetPaymentBubbleView());
  }

  IconLabelBubbleView* IconView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    IconLabelBubbleView* icon;
    if (IsPageActionMigrationEnabled()) {
      icon = browser_view->toolbar_button_provider()->GetPageActionView(
          kActionShowPaymentsBubbleOrPage);
    } else {
      icon = browser_view->toolbar_button_provider()->GetPageActionIconView(
          PageActionIconType::kSaveCard);
    }
    CHECK(icon);
    return icon;
  }

  void ShowBubble(bool card_saved) {
    GetController()->ShowConfirmationBubbleView(
        card_saved,
        /*on_confirmation_closed_callback=*/std::nullopt);
  }

  void HideBubble(views::Widget::ClosedReason closed_reason) {
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        BubbleView()->GetWidget());
    BubbleView()->GetWidget()->CloseWithReason(closed_reason);
    destroyed_waiter.Wait();
  }

  bool IsPageActionMigrationEnabled() {
    return IsPageActionMigrated(PageActionIconType::kSaveCard);
  }

 private:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(SaveCardConfirmationBubbleViewsInteractiveUiTest,
                       ShowSuccessBubbleViewThenHideBubbleView) {
  views::test::AXEventCounter counter(views::AXUpdateNotifier::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));

  ShowBubble(/*card_saved=*/true);

  EXPECT_NE(BubbleView(), nullptr);
  // Checks the count of accessibility event registered by AXUpdateNotifier when
  // bubble is shown.
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));

  EXPECT_TRUE(BubbleView()->ShouldShowCloseButton());
  EXPECT_TRUE(BubbleView()
                  ->GetBubbleFrameView()
                  ->GetHeaderViewForTesting()
                  ->GetVisible());
  EXPECT_NE(BubbleView()->GetBubbleFrameView()->title(), nullptr);
  EXPECT_EQ(BubbleView()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_TITLE_TEXT));
  EXPECT_TRUE(
      BubbleView()->GetViewByID(DialogViewId::DESCRIPTION_LABEL)->GetVisible());
  EXPECT_EQ(static_cast<views::Label*>(
                BubbleView()->GetViewByID(DialogViewId::DESCRIPTION_LABEL))
                ->GetText(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT));
  EXPECT_EQ(static_cast<views::Label*>(
                BubbleView()->GetViewByID(DialogViewId::DESCRIPTION_LABEL))
                ->GetViewAccessibility()
                .GetCachedName(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT));
  EXPECT_EQ(BubbleView()->buttons(),
            static_cast<int>(ui::mojom::DialogButton::kNone));
  EXPECT_TRUE(IconView()->GetVisible());

  HideBubble(views::Widget::ClosedReason::kLostFocus);
  EXPECT_EQ(BubbleView(), nullptr);
  EXPECT_FALSE(IconView()->GetVisible());
}

IN_PROC_BROWSER_TEST_P(SaveCardConfirmationBubbleViewsInteractiveUiTest,
                       ShowFailureBubbleViewThenHideBubbleView) {
  views::test::AXEventCounter counter(views::AXUpdateNotifier::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));
  ShowBubble(/*card_saved=*/false);

  EXPECT_NE(BubbleView(), nullptr);
  // Checks the count of accessibility event registered by AXUpdateNotifier when
  // bubble is shown.
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));

  EXPECT_FALSE(BubbleView()->ShouldShowCloseButton());
  EXPECT_EQ(BubbleView()->GetBubbleFrameView()->GetHeaderViewForTesting(),
            nullptr);
  EXPECT_NE(BubbleView()->GetBubbleFrameView()->title(), nullptr);
  EXPECT_EQ(BubbleView()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_FAILURE_TITLE_TEXT));
  EXPECT_TRUE(
      BubbleView()->GetViewByID(DialogViewId::DESCRIPTION_LABEL)->GetVisible());
  EXPECT_EQ(static_cast<views::Label*>(
                BubbleView()->GetViewByID(DialogViewId::DESCRIPTION_LABEL))
                ->GetText(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_FAILURE_DESCRIPTION_TEXT));
  EXPECT_EQ(static_cast<views::Label*>(
                BubbleView()->GetViewByID(DialogViewId::DESCRIPTION_LABEL))
                ->GetViewAccessibility()
                .GetCachedName(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_FAILURE_DESCRIPTION_TEXT));
  EXPECT_EQ(BubbleView()->buttons(),
            static_cast<int>(ui::mojom::DialogButton::kOk));
  EXPECT_EQ(
      BubbleView()->GetDialogButtonLabel(ui::mojom::DialogButton::kOk),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_BUTTON_TEXT));
  EXPECT_EQ(
      BubbleView()->GetOkButton()->GetViewAccessibility().GetCachedName(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_FAILURE_OK_BUTTON_ACCESSIBLE_NAME));
  EXPECT_TRUE(IconView()->GetVisible());

  HideBubble(views::Widget::ClosedReason::kLostFocus);
  EXPECT_EQ(BubbleView(), nullptr);
  EXPECT_FALSE(IconView()->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SaveCardConfirmationBubbleViewsInteractiveUiTest,
    ::testing::Bool(),
    [](const ::testing::TestParamInfo<
        SaveCardConfirmationBubbleViewsInteractiveUiTest::ParamType>& info) {
      return base::StrCat({
          info.param ? "NewPageAction" : "OldPageAction",
      });
    });

class VirtualCardEnrollConfirmationBubbleViewsInteractiveUiTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  VirtualCardEnrollConfirmationBubbleViewsInteractiveUiTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {};
    std::vector<base::test::FeatureRef> disabled_features = {};

    if (GetParam()) {
      enabled_features.push_back(
          {::features::kPageActionsMigration,
           {
               {::features::kPageActionsMigrationVirtualCard.name, "true"},
           }});
    } else {
      disabled_features.emplace_back(::features::kPageActionsMigration);
    }

    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  ~VirtualCardEnrollConfirmationBubbleViewsInteractiveUiTest() override =
      default;
  VirtualCardEnrollConfirmationBubbleViewsInteractiveUiTest(
      const VirtualCardEnrollConfirmationBubbleViewsInteractiveUiTest&) =
      delete;
  VirtualCardEnrollConfirmationBubbleViewsInteractiveUiTest& operator=(
      const VirtualCardEnrollConfirmationBubbleViewsInteractiveUiTest&) =
      delete;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    VirtualCardEnrollBubbleControllerImpl* virtual_card_enroll_controller =
        static_cast<VirtualCardEnrollBubbleControllerImpl*>(
            VirtualCardEnrollBubbleControllerImpl::GetOrCreate(
                browser()->tab_strip_model()->GetActiveWebContents()));
    CHECK(virtual_card_enroll_controller);
  }

  VirtualCardEnrollBubbleControllerImpl* GetController() {
    if (!browser() || !browser()->tab_strip_model() ||
        !browser()->tab_strip_model()->GetActiveWebContents()) {
      return nullptr;
    }

    return VirtualCardEnrollBubbleControllerImpl::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews* BubbleView() {
    return static_cast<
        SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews*>(
        GetController()->GetVirtualCardBubbleView());
  }

  IconLabelBubbleView* IconView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    IconLabelBubbleView* icon =
        browser_view->toolbar_button_provider()->GetPageActionView(
            kActionVirtualCardEnroll);
    CHECK(icon);
    return icon;
  }

  void ShowBubble(bool is_vcn_enrolled) {
    GetController()->ShowConfirmationBubbleView(
        is_vcn_enrolled
            ? payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess
            : payments::PaymentsAutofillClient::PaymentsRpcResult::
                  kPermanentFailure);
  }

 private:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(
    VirtualCardEnrollConfirmationBubbleViewsInteractiveUiTest,
    ShowSuccessBubbleViewThenHideBubbleView) {
  views::test::AXEventCounter counter(views::AXUpdateNotifier::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));

  ShowBubble(/*is_vcn_enrolled=*/true);

  EXPECT_NE(BubbleView(), nullptr);
  // Checks the count of accessibility event registered by AXUpdateNotifier when
  // bubble is shown.
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));
  EXPECT_TRUE(BubbleView()->ShouldShowCloseButton());
  EXPECT_TRUE(BubbleView()
                  ->GetBubbleFrameView()
                  ->GetHeaderViewForTesting()
                  ->GetVisible());
  EXPECT_NE(BubbleView()->GetBubbleFrameView()->title(), nullptr);
  EXPECT_EQ(
      BubbleView()->GetWindowTitle(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_SUCCESS_TITLE_TEXT));
  EXPECT_TRUE(
      BubbleView()->GetViewByID(DialogViewId::DESCRIPTION_LABEL)->GetVisible());
  EXPECT_EQ(
      static_cast<views::Label*>(
          BubbleView()->GetViewByID(DialogViewId::DESCRIPTION_LABEL))
          ->GetText(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT));
  EXPECT_EQ(
      static_cast<views::Label*>(
          BubbleView()->GetViewByID(DialogViewId::DESCRIPTION_LABEL))
          ->GetViewAccessibility()
          .GetCachedName(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT));
  EXPECT_EQ(BubbleView()->buttons(),
            static_cast<int>(ui::mojom::DialogButton::kNone));
  EXPECT_TRUE(IconView()->GetVisible());

  GetController()->HideIconAndBubble();
  EXPECT_EQ(BubbleView(), nullptr);
  EXPECT_FALSE(IconView()->GetVisible());
}

IN_PROC_BROWSER_TEST_P(
    VirtualCardEnrollConfirmationBubbleViewsInteractiveUiTest,
    ShowFailureBubbleViewThenHideBubbleView) {
  CreditCard card = test::GetCreditCard();
  VirtualCardEnrollmentFields enrollment_fields;
  enrollment_fields.credit_card = card;
  test_api(*GetController())
      .SetUiModel(
          std::make_unique<VirtualCardEnrollUiModel>(enrollment_fields));

  views::test::AXEventCounter counter(views::AXUpdateNotifier::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));

  ShowBubble(/*is_vcn_enrolled=*/false);

  EXPECT_NE(BubbleView(), nullptr);
  // Checks the count of accessibility event registered by AXUpdateNotifier when
  // bubble is shown.
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));
  EXPECT_FALSE(BubbleView()->ShouldShowCloseButton());
  EXPECT_EQ(BubbleView()->GetBubbleFrameView()->GetHeaderViewForTesting(),
            nullptr);
  EXPECT_NE(BubbleView()->GetBubbleFrameView()->title(), nullptr);
  EXPECT_EQ(
      BubbleView()->GetWindowTitle(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_FAILURE_TITLE_TEXT));
  EXPECT_TRUE(
      BubbleView()->GetViewByID(DialogViewId::DESCRIPTION_LABEL)->GetVisible());
  EXPECT_EQ(
      static_cast<views::Label*>(
          BubbleView()->GetViewByID(DialogViewId::DESCRIPTION_LABEL))
          ->GetText(),
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_FAILURE_DESCRIPTION_TEXT,
          card.NetworkAndLastFourDigits()));
  EXPECT_EQ(
      static_cast<views::Label*>(
          BubbleView()->GetViewByID(DialogViewId::DESCRIPTION_LABEL))
          ->GetViewAccessibility()
          .GetCachedName(),
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_FAILURE_DESCRIPTION_TEXT,
          card.NetworkAndLastFourDigits()));
  EXPECT_EQ(BubbleView()->buttons(),
            static_cast<int>(ui::mojom::DialogButton::kOk));
  EXPECT_EQ(
      BubbleView()->GetDialogButtonLabel(ui::mojom::DialogButton::kOk),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_BUTTON_TEXT));
  EXPECT_EQ(
      BubbleView()->GetOkButton()->GetViewAccessibility().GetCachedName(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_FAILURE_OK_BUTTON_ACCESSIBLE_NAME));
  EXPECT_TRUE(IconView()->GetVisible());

  GetController()->HideIconAndBubble();
  EXPECT_EQ(BubbleView(), nullptr);
  EXPECT_FALSE(IconView()->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    VirtualCardEnrollConfirmationBubbleViewsInteractiveUiTest,
    ::testing::Bool(),
    [](const ::testing::TestParamInfo<
        VirtualCardEnrollConfirmationBubbleViewsInteractiveUiTest::ParamType>&
           info) {
      return base::StrCat({info.param ? "NewPageAction" : "OldPageAction"});
    });

}  // namespace autofill
