// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/at_memory_promo_bubble_view.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/user_education/impl/browser_user_education_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/accessibility_annotator/core/url_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace autofill {

// static
std::unique_ptr<AtMemoryPromoBubbleView> AtMemoryPromoBubbleView::Create(
    const scoped_refptr<user_education::UserEducationContext>& context,
    user_education::FeaturePromoSpecification::BuildHelpBubbleParams params) {
  BrowserView& browser_view =
      context->AsA<BrowserUserEducationContext>()->GetBrowserView();
  content::WebContents* web_contents = browser_view.GetActiveWebContents();

  views::BubbleAnchor anchor =
      browser_view.toolbar_button_provider()->GetBubbleAnchor(
          kActionShowAddressesBubbleOrPage);

  return std::make_unique<AtMemoryPromoBubbleView>(anchor, web_contents);
}

AtMemoryPromoBubbleView::AtMemoryPromoBubbleView(
    views::BubbleAnchor anchor_view,
    content::WebContents* web_contents)
    : AutofillLocationBarBubble(anchor_view, web_contents) {
  SetShowCloseButton(true);

  const views::LayoutProvider* provider = views::LayoutProvider::Get();

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  int max_width =
      provider->GetDistanceMetric(views::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
      margins().width();

  AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(IDS_AT_MEMORY_PROMO_DESCRIPTION))
          .SetTextStyle(views::style::STYLE_SECONDARY)
          .SetTextContext(views::style::CONTEXT_LABEL)
          .SetMultiLine(true)
          .SetMaximumWidth(max_width)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .Build());

  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(IDS_AT_MEMORY_PROMO_LEARN_MORE_BUTTON));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(IDS_AT_MEMORY_PROMO_GOT_IT_BUTTON));
  SetButtonStyle(ui::mojom::DialogButton::kOk, ui::ButtonStyle::kProminent);
  SetButtonStyle(ui::mojom::DialogButton::kCancel, ui::ButtonStyle::kDefault);
  SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kNone));

  SetAcceptCallback(base::BindOnce(&AtMemoryPromoBubbleView::OnLearnMoreClicked,
                                   base::Unretained(this)));
  SetCancelCallback(base::BindOnce(&AtMemoryPromoBubbleView::OnGotItClicked,
                                   base::Unretained(this)));
}

AtMemoryPromoBubbleView::~AtMemoryPromoBubbleView() = default;

void AtMemoryPromoBubbleView::OnLearnMoreClicked() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      anchor_widget()->GetNativeWindow());

  if (!browser_view) {
    return;
  }
  Browser* browser = browser_view->browser();
  if (browser) {
    // `PostTask` is used to avoid a use-after-free. Both `Navigate()`
    // and `NotifyUserAction()` trigger the destruction of this bubble. If
    // `Navigate()` is called first, it can trigger destruction via focus loss
    // before the action is recorded. If `NotifyUserAction()` is called first,
    // it destroys the view before navigation can be initiated.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<Browser> browser) {
                         if (!browser) {
                           return;
                         }
                         NavigateParams params(
                             browser.get(),
                             GURL(accessibility_annotator::
                                      kAccessibilityAnnotatorLearnMoreURL),
                             ui::PAGE_TRANSITION_LINK);
                         params.disposition =
                             WindowOpenDisposition::NEW_FOREGROUND_TAB;
                         Navigate(&params);
                       },
                       browser->AsWeakPtr()));
  }

  NotifyUserAction(CustomHelpBubbleUi::UserAction::kDismiss);
}

void AtMemoryPromoBubbleView::OnGotItClicked() {
  NotifyUserAction(CustomHelpBubbleUi::UserAction::kDismiss);
}

void AtMemoryPromoBubbleView::Hide() {
  CloseBubble();
}

std::u16string AtMemoryPromoBubbleView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_AT_MEMORY_PROMO_TITLE);
}

void AtMemoryPromoBubbleView::AddedToWidget() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  auto image_view = std::make_unique<views::ImageView>(
      bundle.GetThemedLottieImageNamed(IDR_AUTOFILL_AT_MEMORY_PROMO_HEADER));
  image_view->GetViewAccessibility().SetIsInvisible(true);
  GetBubbleFrameView()->SetHeaderView(std::move(image_view));
}

BEGIN_METADATA(AtMemoryPromoBubbleView)
END_METADATA

}  // namespace autofill
