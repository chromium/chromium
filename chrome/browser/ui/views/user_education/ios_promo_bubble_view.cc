// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/ios_promo_bubble_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/promos/ios_promos_utils.h"
#include "chrome/browser/ui/views/promos/ios_promo_bubble.h"
#include "chrome/browser/ui/views/promos/ios_promo_constants.h"
#include "chrome/browser/ui/views/user_education/impl/browser_user_education_context.h"
#include "chrome/grit/theme_resources.h"
#include "components/user_education/views/help_bubble_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

using ios_promos_utils::IsUserActiveOnIOS;
using user_education::CustomHelpBubbleUi;

namespace {

// The optional header view for the IOSPromoBubbleView that displays a green
// checkmark with a title and subtitle underneath.
class IOSPromoBubbleHeaderView : public views::View {
  METADATA_HEADER(IOSPromoBubbleHeaderView, views::View)

 public:
  IOSPromoBubbleHeaderView(const std::u16string& title,
                           const std::u16string& subtitle) {
    const auto* layout_provider = views::LayoutProvider::Get();
    const int bottom_margin = layout_provider->GetDistanceMetric(
        views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT);
    const int vertical_spacing = layout_provider->GetDistanceMetric(
        views::DISTANCE_RELATED_CONTROL_VERTICAL);
    const gfx::Insets dialog_insets =
        layout_provider->GetInsetsMetric(views::INSETS_DIALOG);
    const gfx::Insets insets =
        gfx::Insets::TLBR(dialog_insets.top(), dialog_insets.left(),
                          bottom_margin, dialog_insets.right());

    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, insets, vertical_spacing));
    SetBackground(views::CreateSolidBackground(ui::kColorSysSurface));

    // Add the green checkmark, centered horizontally.
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    AddChildView(
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
            .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
            .SetInsideBorderInsets(gfx::Insets::VH(vertical_spacing, 0))
            .AddChild(views::Builder<views::ImageView>().SetImage(
                ui::ImageModel::FromImageSkia(
                    *bundle.GetImageSkiaNamed(IDR_SUCCESS_GREEN_CHECKMARK))))
            .Build());

    // Add the header title and subtitle.
    AddChildView(
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kVertical)
            .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
            .AddChild(views::Builder<views::Label>()
                          .SetText(title)
                          .SetMultiLine(true)
                          .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
                          .SetTextStyle(views::style::STYLE_HEADLINE_4))
            .AddChild(views::Builder<views::Label>()
                          .SetText(subtitle)
                          .SetMultiLine(true)
                          .SetTextContext(views::style::CONTEXT_LABEL)
                          .SetTextStyle(views::style::STYLE_SECONDARY))
            .Build());
  }
  ~IOSPromoBubbleHeaderView() override = default;

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    SetBorder(views::CreateSolidSidedBorder(
        gfx::Insets::TLBR(0, 0, 1, 0),
        GetColorProvider()->GetColor(ui::kColorSeparator)));
  }
};

BEGIN_METADATA(IOSPromoBubbleHeaderView)
END_METADATA

}  // namespace

// static
std::unique_ptr<IOSPromoBubbleView> IOSPromoBubbleView::Create(
    IOSPromoType promo_type,
    const scoped_refptr<user_education::UserEducationContext>& context,
    user_education::FeaturePromoSpecification::BuildHelpBubbleParams params) {
  Profile* profile = context->AsA<BrowserUserEducationContext>()
                         ->GetBrowserView()
                         .GetProfile();
  IOSPromoBubbleType promo_bubble_type = IsUserActiveOnIOS(profile)
                                             ? IOSPromoBubbleType::kReminder
                                             : IOSPromoBubbleType::kQRCode;
  auto* const anchor_element = params.anchor_element.get();
  return std::make_unique<IOSPromoBubbleView>(
      profile, promo_type, promo_bubble_type,
      anchor_element->AsA<views::TrackedElementViews>()->view(),
      user_education::HelpBubbleViews::TranslateArrow(params.arrow));
}

IOSPromoBubbleView::IOSPromoBubbleView(Profile* profile,
                                       IOSPromoType promo_type,
                                       IOSPromoBubbleType promo_bubble_type,
                                       views::View* anchor_view,
                                       views::BubbleBorder::Arrow arrow)
    : views::BubbleDialogDelegateView(anchor_view, arrow),
      profile_(profile),
      promo_type_(promo_type),
      promo_bubble_type_(promo_bubble_type),
      config_(IOSPromoBubble::SetUpBubble(promo_type_, promo_bubble_type_)) {
  // Set up the Dialog.
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBackgroundColor(ui::kColorSysSurface2);
  SetShowCloseButton(true);
  SetShowTitle(true);
  SetTitle(config_.promo_title_id);
  SetWidth(config_.with_header ? views::DISTANCE_BUBBLE_PREFERRED_WIDTH
                               : views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  AddChildView(
      IOSPromoBubble::CreateImageAndBodyTextView(config_, promo_bubble_type_));

  // Set up buttons.
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kCancel) |
             static_cast<int>(ui::mojom::DialogButton::kOk));
  SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(config_.accept_button_text_id));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(config_.decline_button_text_id));
  SetButtonStyle(ui::mojom::DialogButton::kCancel, ui::ButtonStyle::kDefault);
  SetCloseCallback(
      base::BindOnce(&IOSPromoBubbleView::OnDismissal, base::Unretained(this)));
}

IOSPromoBubbleView::~IOSPromoBubbleView() = default;

void IOSPromoBubbleView::AddedToWidget() {
  BubbleDialogDelegateView::AddedToWidget();
  if (config_.with_header) {
    GetBubbleFrameView()->SetHeaderView(
        std::make_unique<IOSPromoBubbleHeaderView>(
            l10n_util::GetStringUTF16(config_.bubble_title_id),
            l10n_util::GetStringUTF16(config_.bubble_subtitle_id)));
  }
}

void IOSPromoBubbleView::VisibilityChanged(View* starting_from,
                                           bool is_visible) {
  BubbleDialogDelegateView::VisibilityChanged(starting_from, is_visible);
  if (starting_from == nullptr && is_visible) {
    GetBubbleFrameView()->SetDisplayVisibleArrow(false);
  }
}

bool IOSPromoBubbleView::Cancel() {
  NotifyUserAction(CustomHelpBubbleUi::UserAction::kCancel);
  return true;
}

bool IOSPromoBubbleView::Accept() {
  // TODO(crbug.com/457394511): Wire up the buttons.
  return false;
}

void IOSPromoBubbleView::SetWidth(views::DistanceMetric metric) {
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(metric));
}
void IOSPromoBubbleView::OnDismissal() {
  NotifyUserAction(CustomHelpBubbleUi::UserAction::kDismiss);
}

BEGIN_METADATA(IOSPromoBubbleView)
END_METADATA
