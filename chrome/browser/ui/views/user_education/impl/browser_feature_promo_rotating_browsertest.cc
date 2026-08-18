// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_controller_browsertest_base.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_class_properties.h"

namespace user_education {

namespace {

using test::kRotatingPromoIPHFeature;

class BrowserFeaturePromoControllerRotatingPromoTest
    : public BrowserFeaturePromoControllerTestBase {
 public:
  BrowserFeaturePromoControllerRotatingPromoTest() = default;
  ~BrowserFeaturePromoControllerRotatingPromoTest() override = default;

  template <typename... Args>
  void RegisterRotatingPromo(Args&&... args) {
    registry()->clear_features_for_testing();
    registry()->RegisterFeature(
        FeaturePromoSpecification::CreateRotatingPromoForTesting(
            kRotatingPromoIPHFeature, FeaturePromoSpecification::RotatingPromos(
                                          std::forward<Args>(args)...)));
  }

  auto VerifyPromoData(
      int show_count,
      int snooze_count,
      std::optional<FeaturePromoClosedReason> last_closed_reason) {
    auto result =
        Steps(WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting),
              CheckResult([this]() { return GetData().show_count; }, show_count,
                          "Check show count."),
              CheckResult([this]() { return GetData().snooze_count; },
                          snooze_count, "Check snooze count."));
    if (last_closed_reason) {
      result.emplace_back(
          CheckResult([this]() { return GetData().last_dismissed_by; },
                      *last_closed_reason, "Check close reason."));
    }
    return result;
  }

  auto VerifyHasHelpBubble(ui::ElementIdentifier id) {
    return CheckView(id, [](views::View* view) {
      return view->GetProperty(user_education::kHasInProductHelpPromoKey);
    });
  }

 private:
  FeaturePromoData GetData() {
    auto result = storage_service()->ReadPromoData(kRotatingPromoIPHFeature);
    CHECK(result.has_value());
    return *result;
  }
};

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerRotatingPromoTest,
                       OnePromo) {
  RegisterRotatingPromo(FeaturePromoSpecification::CreateForSnoozePromo(
      kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId,
      IDS_CHROME_TIP));

  // Show the rotating promo twice, closing it different ways. The same text
  // should re-show each time.
  RunTestSequence(MaybeShowPromo({kRotatingPromoIPHFeature}),
                  CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                                    &views::Label::GetText,
                                    l10n_util::GetStringUTF16(IDS_CHROME_TIP)),
                  ClosePromo(),
                  VerifyPromoData(1, 0, FeaturePromoClosedReason::kCancel),
                  MaybeShowPromo({kRotatingPromoIPHFeature}),
                  CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                                    &views::Label::GetText,
                                    l10n_util::GetStringUTF16(IDS_CHROME_TIP)),
                  PressButton(HelpBubbleView::kDefaultButtonIdForTesting),
                  VerifyPromoData(2, 0, FeaturePromoClosedReason::kDismiss));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerRotatingPromoTest,
                       ToastHasDismissButton) {
  RegisterRotatingPromo(
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId,
          IDS_CHROME_TIP, IDS_CANCEL,
          FeaturePromoSpecification::AcceleratorInfo()),
      FeaturePromoSpecification::CreateForSnoozePromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId, IDS_OK));

  // Show the rotating promo three times, verifying that it wraps around to the,
  // first promo after the second.
  RunTestSequence(
      // Show the promo and press the default button to close it.
      MaybeShowPromo({kRotatingPromoIPHFeature}),
      PressButton(HelpBubbleView::kDefaultButtonIdForTesting),
      WaitForHide(HelpBubbleView::kHelpBubbleElementIdForTesting),
      // Ensure the next promo shows.
      MaybeShowPromo({kRotatingPromoIPHFeature}),
      CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringUTF16(IDS_OK)));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerRotatingPromoTest,
                       TwoPromosRotating) {
  int call_count = 0;
  RegisterRotatingPromo(
      FeaturePromoSpecification::CreateForSnoozePromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId,
          IDS_CHROME_TIP),
      FeaturePromoSpecification::CreateForCustomAction(
          kRotatingPromoIPHFeature, kTopContainerElementId, IDS_OK,
          IDS_CHROME_TIP,
          base::BindLambdaForTesting(
              [&call_count](const user_education::UserEducationContextPtr&,
                            FeaturePromoHandle) { ++call_count; })));

  // Show the rotating promo three times, verifying that it wraps around to the,
  // first promo after the second.
  RunTestSequence(
      MaybeShowPromo({kRotatingPromoIPHFeature}),
      CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringUTF16(IDS_CHROME_TIP)),
      VerifyHasHelpBubble(kToolbarAppMenuButtonElementId), ClosePromo(),
      MaybeShowPromo({kRotatingPromoIPHFeature}),
      CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringUTF16(IDS_OK)),
      VerifyHasHelpBubble(kTopContainerElementId),
      PressButton(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      CheckResult([&call_count]() { return call_count; }, 1),
      MaybeShowPromo({kRotatingPromoIPHFeature}),
      CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringUTF16(IDS_CHROME_TIP)),
      VerifyHasHelpBubble(kToolbarAppMenuButtonElementId),
      PressButton(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      CheckResult([&call_count]() { return call_count; }, 1));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerRotatingPromoTest,
                       SnoozeButtonRepeats) {
  int call_count = 0;
  RegisterRotatingPromo(
      FeaturePromoSpecification::CreateForSnoozePromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId,
          IDS_CHROME_TIP),
      FeaturePromoSpecification::CreateForCustomAction(
          kRotatingPromoIPHFeature, kTopContainerElementId, IDS_OK,
          IDS_CHROME_TIP,
          base::BindLambdaForTesting(
              [&call_count](const user_education::UserEducationContextPtr&,
                            FeaturePromoHandle) { ++call_count; })));

  // Show the rotating promo three times, snoozing the first time. Verify that
  // snoozing re-shows the same promo.
  RunTestSequence(
      MaybeShowPromo({kRotatingPromoIPHFeature}),
      CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringUTF16(IDS_CHROME_TIP)),
      VerifyHasHelpBubble(kToolbarAppMenuButtonElementId),
      PressButton(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      VerifyPromoData(1, 1, std::nullopt),
      MaybeShowPromo({kRotatingPromoIPHFeature}),
      CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringUTF16(IDS_CHROME_TIP)),
      VerifyHasHelpBubble(kToolbarAppMenuButtonElementId), ClosePromo(),
      VerifyPromoData(2, 1, FeaturePromoClosedReason::kCancel),
      MaybeShowPromo({kRotatingPromoIPHFeature}),
      CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringUTF16(IDS_OK)),
      VerifyHasHelpBubble(kTopContainerElementId),
      PressButton(HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      CheckResult([&call_count]() { return call_count; }, 1));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerRotatingPromoTest,
                       RotatesPastGaps) {
  RegisterRotatingPromo(
      std::nullopt,
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId,
          IDS_CHROME_TIP, IDS_OK, FeaturePromoSpecification::AcceleratorInfo()),
      std::nullopt,
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId, IDS_OK,
          IDS_CHROME_TIP, FeaturePromoSpecification::AcceleratorInfo()),
      std::nullopt, std::nullopt);

  // Show the rotating promo three times, verifying that it skips gaps.
  RunTestSequence(MaybeShowPromo({kRotatingPromoIPHFeature}),
                  CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                                    &views::Label::GetText,
                                    l10n_util::GetStringUTF16(IDS_CHROME_TIP)),
                  ClosePromo(), MaybeShowPromo({kRotatingPromoIPHFeature}),
                  CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                                    &views::Label::GetText,
                                    l10n_util::GetStringUTF16(IDS_OK)),
                  ClosePromo(), MaybeShowPromo({kRotatingPromoIPHFeature}),
                  CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                                    &views::Label::GetText,
                                    l10n_util::GetStringUTF16(IDS_CHROME_TIP)),
                  ClosePromo());
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerRotatingPromoTest,
                       ContinuesWithNewRotatingPromo) {
  RegisterRotatingPromo(
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId,
          IDS_CHROME_TIP, IDS_OK, FeaturePromoSpecification::AcceleratorInfo()),
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId, IDS_OK,
          IDS_CHROME_TIP, FeaturePromoSpecification::AcceleratorInfo()));

  // Show the two existing promos, putting the promo at the end of the list.
  RunTestSequence(MaybeShowPromo({kRotatingPromoIPHFeature}),
                  CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                                    &views::Label::GetText,
                                    l10n_util::GetStringUTF16(IDS_CHROME_TIP)),
                  ClosePromo(), MaybeShowPromo({kRotatingPromoIPHFeature}),
                  CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                                    &views::Label::GetText,
                                    l10n_util::GetStringUTF16(IDS_OK)),
                  ClosePromo());

  // Re-register with an additional promo.
  RegisterRotatingPromo(
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId,
          IDS_CHROME_TIP, IDS_OK, FeaturePromoSpecification::AcceleratorInfo()),
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId, IDS_OK,
          IDS_CHROME_TIP, FeaturePromoSpecification::AcceleratorInfo()),
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId, IDS_CANCEL,
          IDS_CHROME_TIP, FeaturePromoSpecification::AcceleratorInfo()));

  // Show one more promo; it should be the new one.
  RunTestSequence(MaybeShowPromo({kRotatingPromoIPHFeature}),
                  CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                                    &views::Label::GetText,
                                    l10n_util::GetStringUTF16(IDS_CANCEL)),
                  ClosePromo());
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerRotatingPromoTest,
                       ContinuesAfterPromoRemoved) {
  RegisterRotatingPromo(
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId,
          IDS_CHROME_TIP, IDS_OK, FeaturePromoSpecification::AcceleratorInfo()),
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId, IDS_OK,
          IDS_CHROME_TIP, FeaturePromoSpecification::AcceleratorInfo()),
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId, IDS_CANCEL,
          IDS_CHROME_TIP, FeaturePromoSpecification::AcceleratorInfo()));

  // Show the first promo, putting the promo at the second index.
  RunTestSequence(MaybeShowPromo({kRotatingPromoIPHFeature}),
                  CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                                    &views::Label::GetText,
                                    l10n_util::GetStringUTF16(IDS_CHROME_TIP)),
                  ClosePromo());

  // Re-register with the second promo removed.
  RegisterRotatingPromo(
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId,
          IDS_CHROME_TIP, IDS_OK, FeaturePromoSpecification::AcceleratorInfo()),
      std::nullopt,
      FeaturePromoSpecification::CreateForToastPromo(
          kRotatingPromoIPHFeature, kToolbarAppMenuButtonElementId, IDS_CANCEL,
          IDS_CHROME_TIP, FeaturePromoSpecification::AcceleratorInfo()));

  // Show one more promo; it should be the third.
  RunTestSequence(MaybeShowPromo({kRotatingPromoIPHFeature}),
                  CheckViewProperty(HelpBubbleView::kBodyTextIdForTesting,
                                    &views::Label::GetText,
                                    l10n_util::GetStringUTF16(IDS_CANCEL)),
                  ClosePromo());
}

}  // namespace
}  // namespace user_education
