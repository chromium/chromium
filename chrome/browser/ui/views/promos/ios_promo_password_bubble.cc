// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/promos/ios_promo_password_bubble.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/promos/promos_pref_names.h"
#include "chrome/browser/promos/promos_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/page_navigator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace constants {
// Margin for QR code image view.
constexpr int kQrCodeMargin = 20;

// Size of QR code image view.
constexpr int kQrCodeImageSize = 100;

// URL used for the QR code within the promo
const char kQRCodeURL[] =
    "https://itunes.apple.com/app/apple-store/"
    "id535886823?pt=9008&ct=saved-passwords-ios-promo-direct-qr&mt=8";

// URL used for the new tab opened by clicking the "Get Started" button.
const char kGetStartedButtonURL[] = "https://google.com/chrome/chrome-for-ios";
}  // namespace constants

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IOSPromoPasswordBubble, kQRCodeView);

// Pointer to BubbleDialogDelegate instance.
views::BubbleDialogDelegate* ios_promo_password_delegate_ = nullptr;

class IOSPromoPasswordBubbleDelegate : public ui::DialogModelDelegate {
 public:
  explicit IOSPromoPasswordBubbleDelegate(Browser* browser)
      : browser_(browser) {}

  // Returns a new QR code generator service if one does not yet exist.
  qrcode_generator::QRImageGenerator& GetQRCodeGenerator() {
    if (!qr_code_service_) {
      qr_code_service_ = std::make_unique<qrcode_generator::QRImageGenerator>();
    }
    return *qr_code_service_;
  }

  // Handler for when the window closes.
  void OnWindowClosing() {
    qr_code_service_.reset();
    ios_promo_password_delegate_ = nullptr;
  }

  // Callback for when the bubble is dismissed.
  void OnDismissal() {
    if (promos_utils::IsActivationCriteriaOverriddenIOSPasswordPromo()) {
      return;
    }

    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForBrowserContext(
            browser_->profile());
    if (tracker) {
      tracker->Dismissed(
          feature_engagement::kIPHiOSPasswordPromoDesktopFeature);
    }

    // User explicitly closed the bubble by clicking "x".
    if (ios_promo_password_delegate_->GetWidget()->closed_reason() ==
        views::Widget::ClosedReason::kCloseButtonClicked) {
      browser_->profile()->GetPrefs()->SetBoolean(
          promos_prefs::kiOSPasswordPromoOptOut, true);
      promos_utils::RecordIOSPasswordPromoUserInteractionHistogram(
          browser_->profile()->GetPrefs()->GetInteger(
              promos_prefs::kiOSPasswordPromoImpressionsCounter),
          promos_utils::DesktopIOSPasswordPromoAction::kExplicitlyClosed);
    } else {
      promos_utils::RecordIOSPasswordPromoUserInteractionHistogram(
          browser_->profile()->GetPrefs()->GetInteger(
              promos_prefs::kiOSPasswordPromoImpressionsCounter),
          promos_utils::DesktopIOSPasswordPromoAction::kDismissed);
    }
  }

  // Callback passed to QR code generation for populating the QR code image in
  // the UI.
  void OnQrCodeGenerated(
      const qrcode_generator::mojom::GenerateQRCodeResponsePtr response) {
    DCHECK(response->error_code ==
           qrcode_generator::mojom::QRCodeGeneratorError::NONE);

    auto qr_code_views = views::ElementTrackerViews::GetInstance()
                             ->GetAllMatchingViewsInAnyContext(
                                 IOSPromoPasswordBubble::kQRCodeView);

    // There should only be one promo at a time.
    DCHECK(qr_code_views.size() == 1);

    views::ImageView* image_view =
        views::AsViewClass<views::ImageView>(qr_code_views.front());

    image_view->SetImage(gfx::ImageSkia::CreateFrom1xBitmap(response->bitmap));
  }

  // Callback for when the "Get started" button is clicked.
  void OnGetStartedButtonClicked() {
    if (!promos_utils::IsActivationCriteriaOverriddenIOSPasswordPromo()) {
      promos_utils::RecordIOSPasswordPromoUserInteractionHistogram(
          browser_->profile()->GetPrefs()->GetInteger(
              promos_prefs::kiOSPasswordPromoImpressionsCounter),
          promos_utils::DesktopIOSPasswordPromoAction::kGetStartedClicked);
    }

    browser_->OpenURL(content::OpenURLParams(
        GURL(constants::kGetStartedButtonURL), content::Referrer(),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false));

    ios_promo_password_delegate_->GetWidget()->Close();
  }

 private:
  // Pointer to the QR code generator service.
  std::unique_ptr<qrcode_generator::QRImageGenerator> qr_code_service_;

  // Pointer to the current Browser;
  raw_ptr<Browser> browser_;
};

// CreateFooter creates the view that is inserted as footer to the bubble.
std::unique_ptr<views::View> CreateFooter(
    IOSPromoPasswordBubble::PromoVariant variant,
    IOSPromoPasswordBubbleDelegate* bubble_delegate) {
  views::LayoutProvider* provider = views::LayoutProvider::Get();

  auto footer_title_container =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .AddChild(views::Builder<views::ImageView>()
                        .SetImage(ui::ResourceBundle::GetSharedInstance()
                                      .GetImageSkiaNamed(IDR_PRODUCT_LOGO_32))
                        .SetImageSize(gfx::Size(20, 20)))
          .AddChild(views::Builder<views::Label>()
                        .SetText(l10n_util::GetStringUTF16(
                            IDS_IOS_PASSWORD_PROMO_BUBBLE_FOOTER_TITLE))
                        .SetTextStyle(views::style::STYLE_PRIMARY)
                        .SetMultiLine(true)
                        .SetHorizontalAlignment(
                            gfx::HorizontalAlignment::ALIGN_TO_HEAD))
          .SetBetweenChildSpacing(provider->GetDistanceMetric(
              views::DistanceMetric::
                  DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING))
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);

  auto footer_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStretch)
          .SetBetweenChildSpacing(provider->GetDistanceMetric(
              views::DistanceMetric::
                  DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT));

  if (variant ==
      IOSPromoPasswordBubble::PromoVariant::GET_STARTED_BUTTON_VARIANT) {
    auto footer_description =
        views::Builder<views::Label>()
            .SetText(l10n_util::GetStringUTF16(
                IDS_IOS_PASSWORD_PROMO_BUBBLE_FOOTER_DESCRIPTION_GENERIC))
            .SetTextContext(ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL)
            .SetTextStyle(views::style::STYLE_SECONDARY)
            .SetMultiLine(true)
            .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);

    auto button_callback = base::BindRepeating(
        &IOSPromoPasswordBubbleDelegate::OnGetStartedButtonClicked,
        base::Unretained(bubble_delegate));
    auto footer_button_container =
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
            .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd)
            .AddChild(views::Builder<views::MdTextButton>()
                          .SetText(l10n_util::GetStringUTF16(
                              IDS_IOS_PASSWORD_PROMO_BUBBLE_BUTTON))
                          .SetIsDefault(true)
                          .SetCallback(button_callback));

    return std::move(footer_view.AddChild(footer_title_container)
                         .AddChild(footer_description)
                         .AddChild(footer_button_container))
        .Build();

  } else if (variant == IOSPromoPasswordBubble::PromoVariant::QR_CODE_VARIANT) {
    auto footer_content_container =
        views::Builder<views::FlexLayoutView>()
            .SetOrientation(views::LayoutOrientation::kHorizontal)
            .SetMainAxisAlignment(views::LayoutAlignment::kStart)
            .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
            .AddChild(
                views::Builder<views::Label>()
                    .SetText(l10n_util::GetStringUTF16(
                        IDS_IOS_PASSWORD_PROMO_BUBBLE_FOOTER_DESCRIPTION_QR))
                    .SetTextContext(
                        ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL)
                    .SetTextStyle(views::style::STYLE_SECONDARY)
                    .SetMultiLine(true)
                    .SetProperty(
                        views::kFlexBehaviorKey,
                        views::FlexSpecification(
                            views::MinimumFlexSizeRule::kScaleToMinimum,
                            views::MaximumFlexSizeRule::kPreferred,
                            /*adjust_height_for_width=*/true))
                    .SetHorizontalAlignment(
                        gfx::HorizontalAlignment::ALIGN_TO_HEAD))
            .AddChild(
                views::Builder<views::ImageView>()
                    .SetHorizontalAlignment(
                        views::ImageView::Alignment::kCenter)
                    .SetHorizontalAlignment(
                        views::ImageView::Alignment::kCenter)
                    .SetImageSize(gfx::Size(constants::kQrCodeImageSize,
                                            constants::kQrCodeImageSize))
                    .SetPreferredSize(gfx::Size(constants::kQrCodeImageSize,
                                                constants::kQrCodeImageSize) +
                                      gfx::Size(constants::kQrCodeMargin,
                                                constants::kQrCodeMargin))
                    .SetProperty(views::kElementIdentifierKey,
                                 IOSPromoPasswordBubble::kQRCodeView)
                    .SetVisible(true)
                    .SetBackground(views::CreateRoundedRectBackground(
                        SK_ColorWHITE,
                        views::LayoutProvider::Get()->GetCornerRadiusMetric(
                            views::Emphasis::kHigh),
                        2)))
            .SetFlexAllocationOrder(views::FlexAllocationOrder::kReverse);

    auto built_footer_view =
        std::move(footer_view.AddChild(footer_title_container)
                      .AddChild(footer_content_container))
            .Build();

    qrcode_generator::mojom::GenerateQRCodeRequestPtr request =
        qrcode_generator::mojom::GenerateQRCodeRequest::New();
    request->data = std::string(constants::kQRCodeURL);
    request->center_image = qrcode_generator::mojom::CenterImage::CHROME_DINO;

    request->render_module_style =
        qrcode_generator::mojom::ModuleStyle::CIRCLES;
    request->render_locator_style =
        qrcode_generator::mojom::LocatorStyle::ROUNDED;

    // Rationale for Unretained(): Closing the bubble destroys qr_code_service_
    // so the callback will not run (see also the doc comment of
    // QRImageGenerator::GenerateQRCode).
    auto callback =
        base::BindOnce(&IOSPromoPasswordBubbleDelegate::OnQrCodeGenerated,
                       base::Unretained(bubble_delegate));
    bubble_delegate->GetQRCodeGenerator().GenerateQRCode(std::move(request),
                                                         std::move(callback));
    return built_footer_view;
  } else {
    NOTREACHED_NORETURN();
  }
}

// static
void IOSPromoPasswordBubble::ShowBubble(views::View* anchor_view,
                                        PageActionIconView* highlighted_button,
                                        PromoVariant variant,
                                        Browser* browser) {
  if (ios_promo_password_delegate_) {
    return;
  }

  auto bubble_delegate_unique =
      std::make_unique<IOSPromoPasswordBubbleDelegate>(browser);
  IOSPromoPasswordBubbleDelegate* bubble_delegate =
      bubble_delegate_unique.get();

  auto dialog_model_builder =
      ui::DialogModel::Builder(std::move(bubble_delegate_unique));

  dialog_model_builder.SetDialogDestroyingCallback(
      base::BindOnce(&IOSPromoPasswordBubbleDelegate::OnWindowClosing,
                     base::Unretained(bubble_delegate)));
  dialog_model_builder.SetCloseActionCallback(
      base::BindOnce(&IOSPromoPasswordBubbleDelegate::OnDismissal,
                     base::Unretained(bubble_delegate)));

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  auto banner_image = ui::ImageModel::FromImageSkia(
      *bundle.GetImageSkiaNamed(IDR_SUCCESS_GREEN_CHECKMARK));
  dialog_model_builder.SetBannerImage(banner_image);

  dialog_model_builder.SetTitle(
      l10n_util::GetStringUTF16(IDS_IOS_PASSWORD_PROMO_BUBBLE_TITLE));

  auto subtitle = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_IOS_PASSWORD_PROMO_BUBBLE_SUBTITLE),
      ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
      views::style::STYLE_SECONDARY);
  subtitle->SetMultiLine(true);
  subtitle->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);

  dialog_model_builder.AddCustomField(
      std::make_unique<views::BubbleDialogModelHost::CustomView>(
          std::move(subtitle), views::BubbleDialogModelHost::FieldType::kText));

  auto promo_bubble = std::make_unique<views::BubbleDialogModelHost>(
      dialog_model_builder.Build(), anchor_view,
      views::BubbleBorder::TOP_RIGHT);

  ios_promo_password_delegate_ = promo_bubble.get();

  promo_bubble->SetHighlightedButton(highlighted_button);
  promo_bubble->SetFootnoteView(CreateFooter(variant, bubble_delegate));

  views::Widget* const widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(promo_bubble));
  widget->Show();
}

// static
void IOSPromoPasswordBubble::Hide() {
  if (ios_promo_password_delegate_) {
    ios_promo_password_delegate_->GetWidget()->Close();
  }
}
