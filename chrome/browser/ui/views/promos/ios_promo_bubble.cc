// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/promos/ios_promo_bubble.h"

#include <memory>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/promos/promos_pref_names.h"
#include "chrome/browser/promos/promos_types.h"
#include "chrome/browser/promos/promos_utils.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/promos/ios_promo_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/prefs/pref_service.h"
#include "components/qr_code_generator/bitmap_generator.h"
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

// Pointer to BubbleDialogDelegate instance.
views::BubbleDialogDelegate* ios_promo_delegate_ = nullptr;

class IOSPromoBubbleDelegate : public ui::DialogModelDelegate {
 public:
  IOSPromoBubbleDelegate(Profile* profile, IOSPromoType promo_type)
      : profile_(profile),
        promo_type_(promo_type),
        ios_promo_prefs_config_(promos_utils::IOSPromoPrefsConfig(promo_type)) {
  }

  // Handler for when the window closes.
  void OnWindowClosing() { ios_promo_delegate_ = nullptr; }

  // Callback for when the bubble is dismissed.
  void OnDismissal() {
    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForBrowserContext(profile_);
    if (tracker && ios_promo_prefs_config_.promo_feature) {
      tracker->Dismissed(*ios_promo_prefs_config_.promo_feature);
    }
    // Don't record a histogram if either of the buttons' callbacks have run
    // and a histogram has already been recorded.
    if (!impression_histogram_already_recorded_) {
      RecordIOSDesktopPromoUserInteractionHistogram(
          promo_type_,
          profile_->GetPrefs()->GetInteger(
              ios_promo_prefs_config_.promo_impressions_counter_pref_name),
          promos_utils::DesktopIOSPromoAction::kDismissed);
    }
  }

  // Callback for when the "No thanks" button is clicked.
  void OnNoThanksButtonClicked() {
    impression_histogram_already_recorded_ = true;

    profile_->GetPrefs()->SetBoolean(
        ios_promo_prefs_config_.promo_opt_out_pref_name, true);

    promos_utils::RecordIOSDesktopPromoUserInteractionHistogram(
        promo_type_,
        profile_->GetPrefs()->GetInteger(
            ios_promo_prefs_config_.promo_impressions_counter_pref_name),
        promos_utils::DesktopIOSPromoAction::kNoThanksClicked);

    ios_promo_delegate_->GetWidget()->Close();
  }

 private:
  // Pointer to the current Profile.
  const raw_ptr<Profile> profile_;

  // Flag tracking whether the impression histogram has already been
  // recorded.
  bool impression_histogram_already_recorded_ = false;

  // Promo type for the current promo bubble.
  const IOSPromoType promo_type_;

  // The structure that holds the configurations of the current promo type.
  const promos_utils::IOSPromoPrefsConfig ios_promo_prefs_config_;
};

// CreateFooter creates the view that is inserted as footer to the bubble.
std::unique_ptr<views::View> CreateFooter(
    IOSPromoBubbleDelegate* bubble_delegate,
    const IOSPromoConstants::IOSPromoTypeConfigs& ios_promo_config) {
  views::LayoutProvider* provider = views::LayoutProvider::Get();

  auto footer_title_container =
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(ios_promo_config.promo_title_id))
          .SetTextStyle(views::style::STYLE_BODY_2_MEDIUM)
          .SetMultiLine(true)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD)
          .SetProperty(views::kMarginsKey,
                       gfx::Insets::TLBR(
                           (views::LayoutProvider::Get()->GetDistanceMetric(
                               views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT)),
                           0,

                           (views::LayoutProvider::Get()->GetDistanceMetric(
                               views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT)),
                           0));

  auto footer_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetCrossAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStretch)
          .SetBetweenChildSpacing(provider->GetDistanceMetric(
              views::DistanceMetric::DISTANCE_VECTOR_ICON_PADDING));

  auto decline_button_callback =
      base::BindRepeating(&IOSPromoBubbleDelegate::OnNoThanksButtonClicked,
                          base::Unretained(bubble_delegate));

  auto decline_button = views::Builder<views::MdTextButton>()
                            .SetText(l10n_util::GetStringUTF16(
                                ios_promo_config.decline_button_text_id))
                            .SetIsDefault(false)
                            .SetCallback(decline_button_callback);

  auto description_label =
      views::Builder<views::Label>()
          .SetText(
              l10n_util::GetStringUTF16(ios_promo_config.promo_description_id))
          .SetTextContext(views::style::CONTEXT_BUBBLE_FOOTER)
          .SetTextStyle(views::style::STYLE_DISABLED)
          .SetMultiLine(true)
          .SetProperty(views::kFlexBehaviorKey,
                       views::FlexSpecification(
                           views::MinimumFlexSizeRule::kScaleToMinimum,
                           views::MaximumFlexSizeRule::kPreferred,
                           /*adjust_height_for_width=*/true))
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);

  auto label_and_button_container =
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kVertical)
          .SetCrossAxisAlignment(views::LayoutAlignment::kEnd)
          .AddChild(description_label)
          .AddChild(decline_button)
          .SetProperty(views::kFlexBehaviorKey,
                       views::FlexSpecification(
                           views::MinimumFlexSizeRule::kScaleToMinimum,
                           views::MaximumFlexSizeRule::kPreferred,
                           /*adjust_height_for_width=*/true))
          .SetProperty(views::kMarginsKey,
                       gfx::Insets::TLBR(
                           0,
                           (views::LayoutProvider::Get()->GetDistanceMetric(
                               views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT)),
                           0, 0));

  views::ImageView* image_view;

  auto qr_code_container =
      views::Builder<views::ImageView>()
          .CopyAddressTo(&image_view)
          .SetHorizontalAlignment(views::ImageView::Alignment::kLeading)
          .SetImageSize(gfx::Size(IOSPromoConstants::kQrCodeImageSize,
                                  IOSPromoConstants::kQrCodeImageSize))
          .SetBorder(views::CreateRoundedRectBorder(
              /*thickness=*/2,
              views::LayoutProvider::Get()->GetCornerRadiusMetric(
                  views::Emphasis::kHigh),
              SK_ColorWHITE))
          .SetVisible(true);

  auto footer_content_container =
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
          .AddChild(qr_code_container)
          .AddChild(label_and_button_container);

  auto built_footer_view =
      std::move(footer_view.AddChild(footer_title_container)
                    .AddChild(footer_content_container))
          .Build();

  // Note that the absence of a quiet zone may interfere with decoding
  // of QR codes even for small codes.
  auto qr_image = qr_code_generator::GenerateImage(
      base::as_byte_span(std::string_view(ios_promo_config.promo_qr_code_url)),
      qr_code_generator::ModuleStyle::kCircles,
      qr_code_generator::LocatorStyle::kRounded,
      qr_code_generator::CenterImage::kProductLogo,
      qr_code_generator::QuietZone::kIncluded);

  // Generating QR code for `kQRCodeURL` should always succeed (e.g. it
  // can't result in input-too-long error or other errors).
  CHECK(qr_image.has_value());

  image_view->SetImage(qr_image.value());

  return built_footer_view;
}

// static
IOSPromoConstants::IOSPromoTypeConfigs IOSPromoBubble::SetUpBubble(
    IOSPromoType promo_type) {
  IOSPromoConstants::IOSPromoTypeConfigs ios_promo_config;
  switch (promo_type) {
    case IOSPromoType::kPassword:
      // Set up iOS Password Promo Bubble.
      ios_promo_config.promo_qr_code_url =
          IOSPromoConstants::kPasswordBubbleQRCodeURL;
      ios_promo_config.bubble_title_id =
          IDS_IOS_DESKTOP_PASSWORD_PROMO_BUBBLE_TITLE;
      ios_promo_config.bubble_subtitle_id =
          IDS_IOS_DESKTOP_PASSWORD_PROMO_BUBBLE_SUBTITLE;
      ios_promo_config.promo_title_id =
          IDS_IOS_DESKTOP_PASSWORD_PROMO_BUBBLE_FOOTER_TITLE;
      ios_promo_config.promo_description_id =
          IDS_IOS_DESKTOP_PASSWORD_PROMO_BUBBLE_FOOTER_DESCRIPTION_QR;
      ios_promo_config.decline_button_text_id =
          IDS_IOS_DESKTOP_PASSWORD_PROMO_BUBBLE_BUTTON_DECLINE;
      break;
    case IOSPromoType::kAddress:
      // Set up iOS Address Promo Bubble.
      ios_promo_config.promo_qr_code_url =
          IOSPromoConstants::kAddressBubbleQRCodeURL;
      ios_promo_config.bubble_title_id =
          IDS_IOS_DESKTOP_ADDRESS_PROMO_BUBBLE_TITLE;
      ios_promo_config.bubble_subtitle_id =
          IDS_IOS_DESKTOP_ADDRESS_PROMO_BUBBLE_SUBTITLE;
      ios_promo_config.promo_title_id =
          IDS_IOS_DESKTOP_ADDRESS_PROMO_BUBBLE_FOOTER_TITLE;
      ios_promo_config.promo_description_id =
          IDS_IOS_DESKTOP_ADDRESS_PROMO_BUBBLE_FOOTER_DESCRIPTION_QR;
      ios_promo_config.decline_button_text_id =
          IDS_IOS_DESKTOP_ADDRESS_PROMO_BUBBLE_BUTTON_DECLINE;
      break;
    case IOSPromoType::kPayment:
      // Set up iOS Payment Promo Bubble.
      ios_promo_config.promo_qr_code_url =
          IOSPromoConstants::kPaymentBubbleQRCodeURL;
      ios_promo_config.bubble_title_id =
          IDS_IOS_DESKTOP_PAYMENT_PROMO_BUBBLE_TITLE;
      ios_promo_config.bubble_subtitle_id =
          IDS_IOS_DESKTOP_PAYMENT_PROMO_BUBBLE_SUBTITLE;
      ios_promo_config.promo_title_id =
          IDS_IOS_DESKTOP_PAYMENT_PROMO_BUBBLE_FOOTER_TITLE;
      ios_promo_config.promo_description_id =
          IDS_IOS_DESKTOP_PAYMENT_PROMO_BUBBLE_FOOTER_DESCRIPTION_QR;
      ios_promo_config.decline_button_text_id =
          IDS_IOS_DESKTOP_PAYMENT_PROMO_BUBBLE_BUTTON_DECLINE;
      break;
  }
  return ios_promo_config;
}

// static
void IOSPromoBubble::ShowPromoBubble(views::View* anchor_view,
                                     PageActionIconView* highlighted_button,
                                     Profile* profile,
                                     IOSPromoType promo_type) {
  IOSPromoConstants::IOSPromoTypeConfigs ios_promo_config =
      SetUpBubble(promo_type);

  if (ios_promo_delegate_) {
    return;
  }

  auto bubble_delegate_unique =
      std::make_unique<IOSPromoBubbleDelegate>(profile, promo_type);
  IOSPromoBubbleDelegate* bubble_delegate = bubble_delegate_unique.get();

  auto dialog_model_builder =
      ui::DialogModel::Builder(std::move(bubble_delegate_unique));

  dialog_model_builder.SetDialogDestroyingCallback(
      base::BindOnce(&IOSPromoBubbleDelegate::OnWindowClosing,
                     base::Unretained(bubble_delegate)));
  dialog_model_builder.SetCloseActionCallback(base::BindOnce(
      &IOSPromoBubbleDelegate::OnDismissal, base::Unretained(bubble_delegate)));

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  auto banner_image = ui::ImageModel::FromImageSkia(
      *bundle.GetImageSkiaNamed(IDR_SUCCESS_GREEN_CHECKMARK));
  dialog_model_builder.SetBannerImage(banner_image);

  dialog_model_builder.SetTitle(
      l10n_util::GetStringUTF16(ios_promo_config.bubble_title_id));

  dialog_model_builder.SetSubtitle(
      l10n_util::GetStringUTF16(ios_promo_config.bubble_subtitle_id));

  auto promo_bubble = std::make_unique<views::BubbleDialogModelHost>(
      dialog_model_builder.Build(), anchor_view,
      views::BubbleBorder::TOP_RIGHT);

  ios_promo_delegate_ = promo_bubble.get();

  promo_bubble->SetHighlightedButton(highlighted_button);
  promo_bubble->SetFootnoteView(
      CreateFooter(bubble_delegate, ios_promo_config));

  views::Widget* const widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(promo_bubble));
  widget->Show();
}

// static
void IOSPromoBubble::Hide() {
  if (ios_promo_delegate_) {
    ios_promo_delegate_->GetWidget()->Close();
  }
}
