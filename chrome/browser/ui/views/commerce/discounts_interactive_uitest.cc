// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/time/default_clock.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/mock_commerce_ui_tab_helper.h"
#include "chrome/browser/ui/views/commerce/discounts_coupon_code_label_view.h"
#include "chrome/browser/ui/views/commerce/discounts_icon_view.h"
#include "chrome/browser/ui/views/controls/subpage_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kShoppingTab);
const char kShoppingURL[] = "/shopping.html";

std::unique_ptr<net::test_server::HttpResponse> BasicResponse(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content("page content");
  response->set_content_type("text/html");
  return response;
}

struct TestData {
  std::string name;
  commerce::DiscountClusterType type;
  std::optional<base::test::FeatureRefAndParams> enabled_feature;
};

std::string GetTestParamName(const ::testing::TestParamInfo<TestData>& info) {
  return info.param.name;
}
}  // namespace

class DiscountsInteractiveTest : public InteractiveBrowserTest,
                                 public testing::WithParamInterface<TestData> {
 public:
  DiscountsInteractiveTest() : test_discount_cluster_type_(GetParam().type) {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {commerce::kDiscountDialogAutoPopupBehaviorSetting,
         {{commerce::kMerchantWideBehaviorParam, "2"},
          {commerce::kNonMerchantWideBehaviorParam, "2"}}}};

    if (GetParam().enabled_feature.has_value()) {
      enabled_features.emplace_back(GetParam().enabled_feature.value());
    }

    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                /*disabled_features=*/{});
  }
  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterDefaultHandler(
        base::BindRepeating(&BasicResponse));
    embedded_test_server()->StartAcceptingConnections();

    InteractiveBrowserTest::SetUpOnMainThread();

    SetUpTabHelperAndShoppingService();
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &DiscountsInteractiveTest::OnWillCreateBrowserContextServices,
                weak_ptr_factory_.GetWeakPtr()));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    commerce::ShoppingServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context) {
          return commerce::MockShoppingService::Build();
        }));
  }

 protected:
  commerce::MockShoppingService* ShoppingService() {
    return static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForBrowserContext(
            browser()->profile()));
  }

  commerce::DiscountClusterType test_discount_cluster_type_;

 private:
  void SetUpTabHelperAndShoppingService() {
    ShoppingService()->SetIsDiscountEligibleToShowOnNavigation(true);

    std::string detail =
        "10% off on laptop stands, valid for purchase of $40 or more";
    std::string terms_and_conditions = "Seller's terms and conditions.";
    std::string value_in_text = "value_in_text";
    std::string discount_code = "WELCOME10";
    double expiry_time_sec =
        (base::DefaultClock::GetInstance()->Now() + base::Days(2))
            .InSecondsFSinceUnixEpoch();
    discount_info_ = commerce::CreateValidDiscountInfo(
        detail, terms_and_conditions, value_in_text, discount_code, /*id=*/1,
        /*is_merchant_wide=*/true, expiry_time_sec,
        test_discount_cluster_type_);

    ShoppingService()->SetResponseForGetDiscountInfoForUrl({discount_info_});
  }

  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription create_services_subscription_;
  commerce::DiscountInfo discount_info_;
  base::WeakPtrFactory<DiscountsInteractiveTest> weak_ptr_factory_{this};
};

class DiscountsIconViewInteractiveTest : public DiscountsInteractiveTest {};

INSTANTIATE_TEST_SUITE_P(
    All,
    DiscountsIconViewInteractiveTest,
    testing::Values(
        TestData{"OfferLevelDiscounts",
                 commerce::DiscountClusterType::kOfferLevel},
        TestData{"PageLevelDiscounts",
                 commerce::DiscountClusterType::kPageLevel,
                 std::make_optional<base::test::FeatureRefAndParams>(
                     {commerce::kEnableDiscountInfoApi,
                      {{commerce::kDiscountOnShoppyPageParam, "true"}}})}),
    GetTestParamName);

IN_PROC_BROWSER_TEST_P(DiscountsIconViewInteractiveTest,
                       DiscountsBubbleDialogShownOnPress) {
  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kDiscountsChipElementId),
      EnsureNotPresent(kDiscountsBubbleDialogId),
      PressButton(kDiscountsChipElementId),
      WaitForShow(kDiscountsBubbleDialogId));
}

IN_PROC_BROWSER_TEST_P(DiscountsIconViewInteractiveTest,
                       DiscountsPageActionIconStateRecordedOnPress) {
  base::HistogramTester histogram_tester;

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kDiscountsChipElementId),
      EnsureNotPresent(kDiscountsBubbleDialogId),
      PressButton(kDiscountsChipElementId), Do([&]() {
        histogram_tester.ExpectBucketCount(
            "Commerce.Discounts.DiscountsPageActionIconIsExpandedWhenClicked",
            true, 1);
        if (commerce::kDiscountOnShoppyPage.Get()) {
          histogram_tester.ExpectBucketCount(
              "Commerce.Discounts.PageActionIcon.TypeOnClick",
              test_discount_cluster_type_, 1);
        }
      }));
}

IN_PROC_BROWSER_TEST_P(DiscountsIconViewInteractiveTest,
                       MetricsRecordedOnPress) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Shopping_ShoppingAction::Shopping_ShoppingAction::
          kEntryName);
  ASSERT_EQ(0u, entries.size());

  base::UserActionTester user_action_tester;
  user_action_tester.ResetCounts();

  base::HistogramTester histogram_tester;

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kDiscountsChipElementId),
      EnsureNotPresent(kDiscountsBubbleDialogId),
      PressButton(kDiscountsChipElementId), Do([&]() {
        entries = test_ukm_recorder.GetEntriesByName(
            ukm::builders::Shopping_ShoppingAction::Shopping_ShoppingAction::
                kEntryName);
        ASSERT_EQ(1u, entries.size());
        test_ukm_recorder.ExpectEntryMetric(
            entries[0],
            ukm::builders::Shopping_ShoppingAction::kDiscountOpenedName, 1);
        test_ukm_recorder.ExpectEntrySourceHasUrl(
            entries[0], embedded_test_server()->GetURL(kShoppingURL));

        histogram_tester.ExpectBucketCount(
            "Commerce.Discounts.DiscountsBubbleIsAutoShown", false, 1);

        EXPECT_TRUE(user_action_tester.GetActionCount(
                        "Commerce.Discounts.DiscountsBubble.AutoShown") == 0);

        if (commerce::kDiscountOnShoppyPage.Get()) {
          histogram_tester.ExpectBucketCount(
              "Commerce.Discounts.DiscountBubble.TypeOnShow",
              test_discount_cluster_type_, 1);
        }
      }));
}

IN_PROC_BROWSER_TEST_P(DiscountsIconViewInteractiveTest,
                       DiscountsPageActionIconClickedRecordedOnPress) {
  base::UserActionTester user_action_tester;
  user_action_tester.ResetCounts();

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kDiscountsChipElementId),
      EnsureNotPresent(kDiscountsBubbleDialogId),
      PressButton(kDiscountsChipElementId), Check([&]() {
        return user_action_tester.GetActionCount(
                   "Commerce.Discounts.DiscountsPageActionIcon.Clicked") == 1;
      }));
}

class DiscountsBubbleDialogInteractiveTest : public DiscountsInteractiveTest {
 public:
  auto HideDiscountBubbleDialog() {
    return Do(base::BindLambdaForTesting([&]() {
      base::RunLoop run_loop;
      views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
      observer.set_closing_callback(
          base::BindLambdaForTesting([&](views::Widget* w) {
            if (w->GetName() == "DiscountsBubbleDialogView") {
              run_loop.Quit();
            }
          }));
      auto* widget =
          static_cast<DiscountsBubbleDialogView*>(
              views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                  kDiscountsBubbleDialogId,
                  browser()->window()->GetElementContext()))
              ->GetWidget();
      widget->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
      run_loop.Run();
    }));
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    DiscountsBubbleDialogInteractiveTest,
    testing::Values(
        TestData{"OfferLevelDiscounts",
                 commerce::DiscountClusterType::kOfferLevel},
        TestData{"PageLevelDiscounts",
                 commerce::DiscountClusterType::kPageLevel,
                 std::make_optional<base::test::FeatureRefAndParams>(
                     {commerce::kEnableDiscountInfoApi,
                      {{commerce::kDiscountOnShoppyPageParam, "true"}}})}),
    GetTestParamName);

IN_PROC_BROWSER_TEST_P(DiscountsBubbleDialogInteractiveTest,
                       CouponCodeCopiedOnCopyButtonPress) {
  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kDiscountsChipElementId),
      PressButton(kDiscountsChipElementId),
      WaitForShow(kDiscountsBubbleDialogId),
      InSameContext(Steps(
          PressButton(kDiscountsBubbleCopyButtonElementId), Check([&]() {
            ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
            std::u16string clipboard_text;
            clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste,
                                /* data_dst = */ nullptr, &clipboard_text);
            return clipboard_text == u"WELCOME10";
          }))));
}

IN_PROC_BROWSER_TEST_P(DiscountsBubbleDialogInteractiveTest,
                       TooltipChangedOnCopyButtonPress) {
  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kDiscountsChipElementId),
      PressButton(kDiscountsChipElementId),
      WaitForShow(kDiscountsBubbleDialogId),
      InSameContext(Steps(
          CheckViewProperty(kDiscountsBubbleCopyButtonElementId,
                            &views::Button::GetTooltipText,
                            l10n_util::GetStringUTF16(
                                IDS_DISCOUNTS_COUPON_CODE_BUTTON_TOOLTIP)),
          PressButton(kDiscountsBubbleCopyButtonElementId),
          CheckViewProperty(
              kDiscountsBubbleCopyButtonElementId,
              &views::Button::GetTooltipText,
              l10n_util::GetStringUTF16(
                  IDS_DISCOUNTS_COUPON_CODE_BUTTON_TOOLTIP_CLICKED)))));
}

IN_PROC_BROWSER_TEST_P(DiscountsBubbleDialogInteractiveTest,
                       AccessibleNameChangedOnCopyButtonPress) {
  static auto is_accessible_name_equal = [](views::MdTextButton* copy_button,
                                            int expected_message_id) {
    std::u16string expected_name =
        copy_button->GetText() + u" " +
        l10n_util::GetStringUTF16(expected_message_id);
    return copy_button->GetViewAccessibility().GetCachedName() == expected_name;
  };

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kDiscountsChipElementId),
      PressButton(kDiscountsChipElementId),
      WaitForShow(kDiscountsBubbleDialogId),
      CheckView(kDiscountsBubbleCopyButtonElementId,
                base::BindOnce([&](views::MdTextButton* copy_button) {
                  return is_accessible_name_equal(
                      copy_button, IDS_DISCOUNTS_COUPON_CODE_BUTTON_TOOLTIP);
                })),
      PressButton(kDiscountsBubbleCopyButtonElementId),
      CheckView(kDiscountsBubbleCopyButtonElementId,
                base::BindOnce([&](views::MdTextButton* copy_button) {
                  return is_accessible_name_equal(
                      copy_button,
                      IDS_DISCOUNTS_COUPON_CODE_BUTTON_TOOLTIP_CLICKED);
                })));
}

IN_PROC_BROWSER_TEST_P(DiscountsBubbleDialogInteractiveTest,
                       MetricsCollectedOnCopyButtonPress) {
  base::UserActionTester user_action_tester;
  user_action_tester.ResetCounts();

  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Shopping_ShoppingAction::Shopping_ShoppingAction::
          kEntryName);
  ASSERT_EQ(0u, entries.size());

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kDiscountsChipElementId),
      PressButton(kDiscountsChipElementId),
      WaitForShow(kDiscountsBubbleDialogId), Do([&]() {
        entries = test_ukm_recorder.GetEntriesByName(
            ukm::builders::Shopping_ShoppingAction::Shopping_ShoppingAction::
                kEntryName);
        ASSERT_EQ(1u, entries.size());
      }),
      InSameContext(Steps(
          PressButton(kDiscountsBubbleCopyButtonElementId), Check([&]() {
            return user_action_tester.GetActionCount(
                       "Commerce.Discounts.DiscountsBubbleCopyButtonClicked") ==
                   1;
          }),
          Do([&]() {
            entries = test_ukm_recorder.GetEntriesByName(
                ukm::builders::Shopping_ShoppingAction::
                    Shopping_ShoppingAction::kEntryName);
            ASSERT_EQ(2u, entries.size());
            test_ukm_recorder.ExpectEntryMetric(
                entries[1],
                ukm::builders::Shopping_ShoppingAction::kDiscountCopiedName, 1);
            test_ukm_recorder.ExpectEntrySourceHasUrl(
                entries[1], embedded_test_server()->GetURL(kShoppingURL));
          }))));
}

IN_PROC_BROWSER_TEST_P(DiscountsBubbleDialogInteractiveTest,
                       IsNotCopiedRecordedOnDialogClosed) {
  base::HistogramTester histogram_tester;

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kDiscountsChipElementId),
      PressButton(kDiscountsChipElementId),
      WaitForShow(kDiscountsBubbleDialogId),
      InSameContext(
          Steps(HideDiscountBubbleDialog(),
                WaitForHide(kDiscountsBubbleDialogId), Do([&]() {
                  histogram_tester.ExpectBucketCount(
                      "Commerce.Discounts.DiscountsBubbleCouponCodeIsCopied",
                      false, 1);
                  if (commerce::kDiscountOnShoppyPage.Get()) {
                    histogram_tester.ExpectBucketCount(
                        "Commerce.Discounts.DiscountsBubble.TypeOnCopy",
                        test_discount_cluster_type_, 0);
                  }
                }))));
}

IN_PROC_BROWSER_TEST_P(DiscountsBubbleDialogInteractiveTest,
                       IsCopiedRecordedOnDialogClosed) {
  base::HistogramTester histogram_tester;

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kDiscountsChipElementId),
      PressButton(kDiscountsChipElementId),
      WaitForShow(kDiscountsBubbleDialogId),
      InSameContext(
          Steps(PressButton(kDiscountsBubbleCopyButtonElementId),
                HideDiscountBubbleDialog(),
                WaitForHide(kDiscountsBubbleDialogId), Do([&]() {
                  histogram_tester.ExpectBucketCount(
                      "Commerce.Discounts.DiscountsBubbleCouponCodeIsCopied",
                      true, 1);
                  if (commerce::kDiscountOnShoppyPage.Get()) {
                    histogram_tester.ExpectBucketCount(
                        "Commerce.Discounts.DiscountsBubble.TypeOnCopy",
                        test_discount_cluster_type_, 1);
                  }
                }))));
}

IN_PROC_BROWSER_TEST_P(DiscountsBubbleDialogInteractiveTest,
                       ShowTermsAndConditionOnClick) {
  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      WaitForShow(kDiscountsChipElementId),
      PressButton(kDiscountsChipElementId),
      WaitForShow(kDiscountsBubbleDialogId),
      EnsurePresent(kDiscountsBubbleTermsAndConditionLabelId),
      EnsureNotPresent(kDiscountsBubbleTermsAndConditionPageId),
      WithElement(kDiscountsBubbleTermsAndConditionLabelId,
                  [](ui::TrackedElement* el) {
                    AsView<views::StyledLabel>(el)->ClickFirstLinkForTesting();
                  })
          .SetMustRemainVisible(false),
      EnsurePresent(kDiscountsBubbleTermsAndConditionPageId));
}

IN_PROC_BROWSER_TEST_P(DiscountsBubbleDialogInteractiveTest,
                       ShowMainPageOnBackPress) {
  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      PressButton(kDiscountsChipElementId),
      WaitForShow(kDiscountsBubbleDialogId),
      WithElement(kDiscountsBubbleTermsAndConditionLabelId,
                  [](ui::TrackedElement* el) {
                    AsView<views::StyledLabel>(el)->ClickFirstLinkForTesting();
                  })
          .SetMustRemainVisible(false),
      EnsureNotPresent(kDiscountsBubbleMainPageId),
      PressButton(kSubpageBackButtonElementId),
      EnsurePresent(kDiscountsBubbleMainPageId));
}
