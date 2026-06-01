// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/email_verification_popup_view.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/email_verification_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace autofill {

namespace {
constexpr int kMinIconSize = 16;
constexpr int kDesiredIconSize = 20;
constexpr int kEmailVerificationMaxWidth = 480;
}  // namespace

EmailVerificationPopupView::EmailVerificationPopupView(
    base::WeakPtr<EmailVerificationPopupController> controller,
    views::Widget* parent_widget,
    const net::SchemefulSite& issuer_site,
    const std::u16string& email,
    base::OnceCallback<void(bool)> callback)
    : PopupBaseView(controller,
                    parent_widget,
                    views::Widget::InitParams::Activatable::kYes),
      callback_(std::move(callback)) {
  SetBackground(views::CreateSolidBackground(ui::kColorDropdownBackground));

  views::BoxLayout* box_layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int kVerticalPadding = provider->GetDistanceMetric(
      views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT);
  const int kHorizontalMargin =
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_HORIZONTAL);

  std::u16string issuer_site_string =
      url_formatter::FormatUrlForSecurityDisplay(
          issuer_site.GetURL(),
          url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);

  // Headline: Email Icon + Title (horizontal)
  auto* headline = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
          .SetBetweenChildSpacing(provider->GetDistanceMetric(
              views::DISTANCE_RELATED_LABEL_HORIZONTAL))
          .SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
              kVerticalPadding, kHorizontalMargin, 0, kHorizontalMargin)))
          .Build());

  icon_view_ = headline->AddChildView(std::make_unique<views::ImageView>(
      ui::ImageModel::FromVectorIcon(vector_icons::kEmailOutlineOldIcon,
                                     ui::kColorIcon, kDesiredIconSize)));
  icon_view_->SetImageSize(gfx::Size(kDesiredIconSize, kDesiredIconSize));

  if (controller && controller->GetWebContents()) {
    Profile* profile = Profile::FromBrowserContext(
        controller->GetWebContents()->GetBrowserContext());
    if (auto* favicon_service =
            LargeIconServiceFactory::GetForBrowserContext(profile)) {
      favicon_service->GetLargeIconRawBitmapForPageUrl(
          issuer_site.GetURL(),
          /*min_source_size_in_pixel=*/kMinIconSize,
          /*size_in_pixel_to_resize_to=*/kDesiredIconSize,
          favicon::LargeIconService::NoBigEnoughIconBehavior::kReturnBitmap,
          base::BindOnce(&EmailVerificationPopupView::OnFaviconLoaded,
                         weak_ptr_factory_.GetWeakPtr()),
          &favicon_task_tracker_);
    }
  }

  headline->AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_EMAIL_VERIFIER_PROMPT_TITLE))
          .SetTextStyle(views::style::TextStyle::STYLE_BODY_3_MEDIUM)
          .SetAccessibleRole(ax::mojom::Role::kHeading)
          .Build());

  // Body (explanatory text)
  auto* body = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
              kVerticalPadding / 2, kHorizontalMargin, 0, kHorizontalMargin)))
          .Build());

  body->AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringFUTF16(
              IDS_AUTOFILL_EMAIL_VERIFIER_PROMPT_BODY, issuer_site_string,
              email))
          .SetMultiLine(true)
          .SetTextStyle(views::style::TextStyle::STYLE_BODY_4)
          .SetEnabledColor(ui::kColorLabelForegroundSecondary)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .Build());

  // Controls (buttons row)
  auto* controls = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd)
          .SetBetweenChildSpacing(provider->GetDistanceMetric(
              views::DISTANCE_RELATED_BUTTON_HORIZONTAL))
          .SetBorder(views::CreateEmptyBorder(
              gfx::Insets::TLBR(kVerticalPadding, kHorizontalMargin,
                                kVerticalPadding, kHorizontalMargin)))
          .Build());

  controls->AddChildView(
      views::Builder<views::MdTextButton>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_EMAIL_VERIFIER_PROMPT_NOT_NOW))
          .SetStyle(ui::ButtonStyle::kDefault)
          .SetCallback(base::BindRepeating(
              &EmailVerificationPopupView::OnCancel, base::Unretained(this)))
          .SetID(static_cast<int>(PopupViewId::kCancelButton))
          .Build());

  auto* allow_button = controls->AddChildView(
      views::Builder<views::MdTextButton>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_EMAIL_VERIFIER_PROMPT_VERIFY))
          .SetStyle(ui::ButtonStyle::kProminent)
          .SetCallback(base::BindRepeating(
              &EmailVerificationPopupView::OnConfirm, base::Unretained(this)))
          .SetID(static_cast<int>(PopupViewId::kConfirmButton))
          .Build());

  SetInitiallyFocusedView(allow_button);
}

EmailVerificationPopupView::~EmailVerificationPopupView() = default;

void EmailVerificationPopupView::Hide() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  DoHide();
}

void EmailVerificationPopupView::Show() {
  DoShow();
  input_protector_.VisibilityChanged(true);
}

bool EmailVerificationPopupView::OverlapsWithPictureInPictureWindow() const {
  return BoundsOverlapWithPictureInPictureWindow(GetBoundsInScreen());
}

gfx::Size EmailVerificationPopupView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  int width =
      std::min(views::View::CalculatePreferredSize(available_size).width(),
               kEmailVerificationMaxWidth);
  return gfx::Size(width,
                   GetLayoutManager()->GetPreferredHeightForWidth(this, width));
}

void EmailVerificationPopupView::OnConfirm(const ui::Event& event) {
  if (input_protector_.IsPossiblyUnintendedInteraction(
          event, /*allow_key_events=*/false)) {
    return;
  }
  if (callback_) {
    std::move(callback_).Run(true);
  }
}

void EmailVerificationPopupView::OnCancel(const ui::Event& event) {
  if (callback_) {
    std::move(callback_).Run(false);
  }
}

void EmailVerificationPopupView::OnFaviconLoaded(
    const favicon_base::LargeIconResult& result) {
  if (result.bitmap.is_valid() && icon_view_) {
    gfx::Image image =
        gfx::Image::CreateFrom1xPNGBytes(result.bitmap.bitmap_data);
    icon_view_->SetImage(ui::ImageModel::FromImage(image));
  }
}

BEGIN_METADATA(EmailVerificationPopupView)
END_METADATA

// static
base::WeakPtr<EmailVerificationPopupView> EmailVerificationPopupView::Show(
    base::WeakPtr<EmailVerificationPopupController> controller,
    views::Widget* parent_widget,
    const net::SchemefulSite& issuer_site,
    const std::u16string& email,
    base::OnceCallback<void(bool)> callback) {
  auto* view = new EmailVerificationPopupView(
      controller, parent_widget, issuer_site, email, std::move(callback));
  view->Show();
  return view->GetWeakPtr();
}

}  // namespace autofill
