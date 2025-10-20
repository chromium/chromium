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
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/promos/promos_pref_names.h"
#include "chrome/browser/promos/promos_types.h"
#include "chrome/browser/promos/promos_utils.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/promos/ios_promo_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/prefs/pref_service.h"
#include "components/qr_code_generator/bitmap_generator.h"
#include "components/sharing_message/features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/page_navigator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/image_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

// Generates and returns the QRCode image for the given URL `qr_code_url`.
ui::ImageModel CreateQrCodeImage(const std::string& qr_code_url) {
  // Note that the absence of a quiet zone may interfere with decoding
  // of QR codes even for small codes.
  auto qr_image = qr_code_generator::GenerateImage(
      base::as_byte_span(std::string_view(qr_code_url)),
      qr_code_generator::ModuleStyle::kCircles,
      qr_code_generator::LocatorStyle::kRounded,
      qr_code_generator::CenterImage::kProductLogo,
      qr_code_generator::QuietZone::kIncluded);

  // Generating QR code for `kQRCodeURL` should always succeed (e.g. it
  // can't result in input-too-long error or other errors).
  CHECK(qr_image.has_value());
  return ui::ImageModel::FromImageSkia(qr_image.value());
}

// Creates and returns IOSPromoTypeConfigs for the Password bubble.
IOSPromoConstants::IOSPromoTypeConfigs SetUpPasswordBubble(
    IOSPromoBubbleType bubble_type) {
  IOSPromoConstants::IOSPromoTypeConfigs config;
  config.with_header = true;
  config.promo_title_id = IDS_IOS_DESKTOP_PASSWORD_PROMO_BUBBLE_FOOTER_TITLE;
  config.bubble_title_id = IDS_IOS_DESKTOP_PASSWORD_PROMO_BUBBLE_TITLE;
  config.bubble_subtitle_id = IDS_IOS_DESKTOP_PASSWORD_PROMO_BUBBLE_SUBTITLE;
  config.decline_button_text_id =
      IDS_IOS_DESKTOP_PASSWORD_PROMO_BUBBLE_BUTTON_DECLINE;
  switch (bubble_type) {
    case IOSPromoBubbleType::kQRCode:
      config.promo_description_id =
          IDS_IOS_DESKTOP_PASSWORD_PROMO_BUBBLE_FOOTER_DESCRIPTION_QR;
      config.accept_button_text_id =
          IDS_IOS_DESKTOP_PROMO_BUBBLE_BUTTON_ACCEPT_QR;
      config.promo_image = CreateQrCodeImage(
          IOSPromoConstants::kIOSPromoPasswordBubbleQRCodeURL);
      break;
    case IOSPromoBubbleType::kReminder:
      config.accept_button_text_id =
          IDS_IOS_DESKTOP_PROMO_BUBBLE_BUTTON_ACCEPT_REMINDER;
      config.promo_description_id =
          IDS_IOS_DESKTOP_PASSWORD_PROMO_BUBBLE_FOOTER_DESCRIPTION_REMINDER;
      break;
  }
  return config;
}

// Creates and returns IOSPromoTypeConfigs for the Address bubble.
IOSPromoConstants::IOSPromoTypeConfigs SetUpAddressBubble(
    IOSPromoBubbleType bubble_type) {
  CHECK_EQ(bubble_type, IOSPromoBubbleType::kQRCode);
  IOSPromoConstants::IOSPromoTypeConfigs config;
  config.with_header = true;
  config.bubble_title_id = IDS_IOS_DESKTOP_ADDRESS_PROMO_BUBBLE_TITLE;
  config.bubble_subtitle_id = IDS_IOS_DESKTOP_ADDRESS_PROMO_BUBBLE_SUBTITLE;
  config.promo_title_id = IDS_IOS_DESKTOP_ADDRESS_PROMO_BUBBLE_FOOTER_TITLE;
  config.promo_description_id =
      IDS_IOS_DESKTOP_ADDRESS_PROMO_BUBBLE_FOOTER_DESCRIPTION_QR;
  config.decline_button_text_id =
      IDS_IOS_DESKTOP_ADDRESS_PROMO_BUBBLE_BUTTON_DECLINE;
  config.accept_button_text_id = IDS_IOS_DESKTOP_PROMO_BUBBLE_BUTTON_ACCEPT_QR;
  config.promo_image =
      CreateQrCodeImage(IOSPromoConstants::kIOSPromoAddressBubbleQRCodeURL);
  return config;
}

// Creates and returns IOSPromoTypeConfigs for the Payment bubble.
IOSPromoConstants::IOSPromoTypeConfigs SetUpPaymentBubble(
    IOSPromoBubbleType bubble_type) {
  CHECK_EQ(bubble_type, IOSPromoBubbleType::kQRCode);
  IOSPromoConstants::IOSPromoTypeConfigs config;
  config.with_header = true;
  config.bubble_title_id =
      IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_TITLE_TEXT;
  config.bubble_subtitle_id =
      IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT;
  config.promo_title_id = IDS_IOS_DESKTOP_PAYMENT_PROMO_BUBBLE_FOOTER_TITLE;
  config.promo_description_id =
      IDS_IOS_DESKTOP_PAYMENT_PROMO_BUBBLE_FOOTER_DESCRIPTION_QR;
  config.decline_button_text_id =
      IDS_IOS_DESKTOP_PAYMENT_PROMO_BUBBLE_BUTTON_DECLINE;
  config.accept_button_text_id = IDS_IOS_DESKTOP_PROMO_BUBBLE_BUTTON_ACCEPT_QR;
  config.promo_image =
      CreateQrCodeImage(IOSPromoConstants::kIOSPromoPaymentBubbleQRCodeURL);
  return config;
}

// Creates and returns IOSPromoTypeConfigs for the Enhanced Browsing bubble.
IOSPromoConstants::IOSPromoTypeConfigs SetUpEnhancedBrowsingBubble(
    IOSPromoBubbleType bubble_type) {
  IOSPromoConstants::IOSPromoTypeConfigs config;
  config.with_header = false;
  config.decline_button_text_id = IDS_IOS_DESKTOP_PROMO_BUBBLE_BUTTON_DECLINE;
  switch (bubble_type) {
    case IOSPromoBubbleType::kQRCode:
      config.promo_title_id = IDS_IOS_DESKTOP_ESB_PROMO_BUBBLE_TITLE_QR;
      config.promo_description_id =
          IDS_IOS_DESKTOP_ESB_PROMO_BUBBLE_DESCRIPTION_QR;
      config.accept_button_text_id =
          IDS_IOS_DESKTOP_PROMO_BUBBLE_BUTTON_ACCEPT_QR;
      // TODO(crbug.com/442562546): Create URL for kEnhancedBrowsing promo.
      config.promo_image =
          CreateQrCodeImage(IOSPromoConstants::kIOSPromoPaymentBubbleQRCodeURL);
      break;
    case IOSPromoBubbleType::kReminder:
      config.promo_title_id = IDS_IOS_DESKTOP_ESB_PROMO_BUBBLE_TITLE_REMINDER;
      config.promo_description_id =
          IDS_IOS_DESKTOP_ESB_PROMO_BUBBLE_DESCRIPTION_REMINDER;
      config.accept_button_text_id =
          IDS_IOS_DESKTOP_PROMO_BUBBLE_BUTTON_ACCEPT_REMINDER;
      config.promo_image =
          ui::ImageModel::FromVectorIcon(kEnhancedBrowsingOnIosIcon);
      break;
  }
  return config;
}

// Creates and returns IOSPromoTypeConfigs for the Lens bubble.
IOSPromoConstants::IOSPromoTypeConfigs SetUpLensBubble(
    IOSPromoBubbleType bubble_type) {
  IOSPromoConstants::IOSPromoTypeConfigs config;
  config.with_header = false;
  config.promo_description_id = IDS_IOS_DESKTOP_LENS_PROMO_BUBBLE_DESCRIPTION;
  config.decline_button_text_id = IDS_IOS_DESKTOP_PROMO_BUBBLE_BUTTON_DECLINE;
  switch (bubble_type) {
    case IOSPromoBubbleType::kQRCode:
      config.promo_title_id = IDS_IOS_DESKTOP_LENS_PROMO_BUBBLE_TITLE_QR;
      config.accept_button_text_id =
          IDS_IOS_DESKTOP_PROMO_BUBBLE_BUTTON_ACCEPT_QR;
      // TODO(crbug.com/442562546): Placeholder, set URL for kLens promo.
      config.promo_image = CreateQrCodeImage(
          IOSPromoConstants::kIOSPromoPasswordBubbleQRCodeURL);
      break;
    case IOSPromoBubbleType::kReminder:
      config.promo_title_id = IDS_IOS_DESKTOP_LENS_PROMO_BUBBLE_TITLE_REMINDER;
      config.accept_button_text_id =
          IDS_IOS_DESKTOP_PROMO_BUBBLE_BUTTON_ACCEPT_REMINDER;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      config.promo_image = ui::ImageModel::FromResourceId(IDR_LENS_ON_IOS_ICON);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
      break;
  }
  return config;
}

// Returns the config for the given `promo_type` and `bubble_type`.
IOSPromoConstants::IOSPromoTypeConfigs SetUpBubble(
    IOSPromoType promo_type,
    IOSPromoBubbleType bubble_type) {
  switch (promo_type) {
    case IOSPromoType::kPassword:
      return SetUpPasswordBubble(bubble_type);
    case IOSPromoType::kAddress:
      return SetUpAddressBubble(bubble_type);
    case IOSPromoType::kPayment:
      return SetUpPaymentBubble(bubble_type);
    case IOSPromoType::kEnhancedBrowsing:
      return SetUpEnhancedBrowsingBubble(bubble_type);
    case IOSPromoType::kLens:
      return SetUpLensBubble(bubble_type);
  }
}
}  // namespace

// Pointer to BubbleDialogDelegate instance.
views::BubbleDialogDelegate* IOSPromoBubble::ios_promo_delegate_ = nullptr;
IOSPromoType IOSPromoBubble::current_promo_type_;

class IOSPromoBubble::IOSPromoBubbleDelegate : public ui::DialogModelDelegate {
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

  // Callback for when the primary action/acceptance button is clicked.
  void AcceptButtonClicked(IOSPromoBubbleType bubble_type) {
    // TODO(crbug.com/438769954): Handle user action and record metrics.
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

// static
std::unique_ptr<views::View> IOSPromoBubble::CreateContentView(
    IOSPromoBubble::IOSPromoBubbleDelegate* bubble_delegate,
    const IOSPromoConstants::IOSPromoTypeConfigs& ios_promo_config,
    bool with_title,
    IOSPromoBubbleType bubble_type) {
  auto content_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetBetweenChildSpacing(
              views::LayoutProvider::Get()->GetDistanceMetric(
                  views::DistanceMetric::DISTANCE_UNRELATED_CONTROL_VERTICAL))
          .Build();

  if (with_title) {
    auto title_view =
        views::Builder<views::Label>()
            .SetText(l10n_util::GetStringUTF16(ios_promo_config.promo_title_id))
            .SetTextStyle(views::style::STYLE_BODY_2_MEDIUM)
            .SetMultiLine(true)
            .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD)
            .SetProperty(
                views::kMarginsKey,
                gfx::Insets::TLBR(
                    (views::LayoutProvider::Get()->GetDistanceMetric(
                        views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT)),
                    0, 0, 0))
            .Build();
    content_view->AddChildView(std::move(title_view));
  }

  content_view->AddChildView(
      CreateImageAndBodyTextView(ios_promo_config, bubble_type));
  content_view->AddChildView(
      CreateButtonsView(bubble_delegate, ios_promo_config, bubble_type));

  return content_view;
}

// static
std::unique_ptr<views::View> IOSPromoBubble::CreateButtonsView(
    IOSPromoBubble::IOSPromoBubbleDelegate* bubble_delegate,
    const IOSPromoConstants::IOSPromoTypeConfigs& ios_promo_config,
    IOSPromoBubbleType bubble_type) {
  auto decline_button_callback = base::BindRepeating(
      &IOSPromoBubble::IOSPromoBubbleDelegate::OnNoThanksButtonClicked,
      base::Unretained(bubble_delegate));

  auto decline_button = views::Builder<views::MdTextButton>()
                            .SetText(l10n_util::GetStringUTF16(
                                ios_promo_config.decline_button_text_id))
                            .SetIsDefault(false)
                            .SetCallback(decline_button_callback);

  auto button_container_builder =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd)
          .SetBetweenChildSpacing(
              views::LayoutProvider::Get()->GetDistanceMetric(
                  views::DistanceMetric::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  button_container_builder.AddChild(decline_button);

  if (MobilePromoOnDesktopTypeEnabled() !=
      MobilePromoOnDesktopPromoType::kDisabled) {
    auto accept_button_callback = base::BindRepeating(
        &IOSPromoBubble::IOSPromoBubbleDelegate::AcceptButtonClicked,
        base::Unretained(bubble_delegate), bubble_type);
    auto accept_button = views::Builder<views::MdTextButton>()
                             .SetText(l10n_util::GetStringUTF16(
                                 ios_promo_config.accept_button_text_id))
                             .SetIsDefault(true)
                             .SetCallback(accept_button_callback);
    button_container_builder.AddChild(accept_button);
  }

  return std::move(button_container_builder).Build();
}

// static
std::unique_ptr<views::View> IOSPromoBubble::CreateImageAndBodyTextView(
    const IOSPromoConstants::IOSPromoTypeConfigs& ios_promo_config,
    IOSPromoBubbleType bubble_type) {
  views::Builder<views::View> image_view;
  if (!ios_promo_config.promo_image.IsEmpty()) {
    auto image_view_builder =
        views::Builder<views::ImageView>()
            .SetImage(ios_promo_config.promo_image)
            .SetImageSize(gfx::Size(IOSPromoConstants::kImageSize,
                                    IOSPromoConstants::kImageSize))
            .SetCornerRadius(
                views::LayoutProvider::Get()->GetCornerRadiusMetric(
                    views::Emphasis::kHigh));
    auto image_container_builder =
        views::Builder<views::View>()
            .SetLayoutManager(std::make_unique<views::FillLayout>())
            .AddChild(std::move(image_view_builder));

    // Add a border if the image is a QRCode.
    if (bubble_type == IOSPromoBubbleType::kQRCode) {
      image_container_builder.SetBorder(views::CreateRoundedRectBorder(
          /*thickness=*/1,
          views::LayoutProvider::Get()->GetCornerRadiusMetric(
              views::Emphasis::kHigh),
          SK_ColorLTGRAY));
    }

    image_view = std::move(image_container_builder);
  }

  auto description_label =
      views::Builder<views::Label>()
          .SetText(
              l10n_util::GetStringUTF16(ios_promo_config.promo_description_id))
          .SetTextContext(views::style::CONTEXT_BUBBLE_FOOTER)
          .SetTextStyle(views::style::STYLE_SECONDARY)
          .SetEnabledColor(kColorDesktopToIOSPromoFooterSubtitleLabel)
          .SetMultiLine(true)
          .SetProperty(views::kFlexBehaviorKey,
                       views::FlexSpecification(
                           views::MinimumFlexSizeRule::kScaleToMinimum,
                           views::MaximumFlexSizeRule::kPreferred,
                           /*adjust_height_for_width=*/true))
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);

  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
      .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
      .SetBetweenChildSpacing(views::LayoutProvider::Get()->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_RELATED_CONTROL_HORIZONTAL))
      .AddChild(std::move(image_view))
      .AddChild(description_label)
      .Build();
}

// static
void IOSPromoBubble::ShowPromoBubble(views::View* anchor_view,
                                     views::Button* highlighted_button,
                                     Profile* profile,
                                     IOSPromoType promo_type,
                                     IOSPromoBubbleType bubble_type) {
  IOSPromoConstants::IOSPromoTypeConfigs ios_promo_config =
      SetUpBubble(promo_type, bubble_type);

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

  if (ios_promo_config.with_header) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    auto banner_image = ui::ImageModel::FromImageSkia(
        *bundle.GetImageSkiaNamed(IDR_SUCCESS_GREEN_CHECKMARK));
    dialog_model_builder.SetBannerImage(banner_image);
    dialog_model_builder.SetTitle(
        l10n_util::GetStringUTF16(ios_promo_config.bubble_title_id));
    dialog_model_builder.SetSubtitle(
        l10n_util::GetStringUTF16(ios_promo_config.bubble_subtitle_id));
  } else {
    dialog_model_builder.SetTitle(
        l10n_util::GetStringUTF16(ios_promo_config.promo_title_id));
    dialog_model_builder.AddCustomField(
        std::make_unique<views::BubbleDialogModelHost::CustomView>(
            CreateContentView(bubble_delegate, ios_promo_config,
                              /*with_title=*/false, bubble_type),
            views::BubbleDialogModelHost::FieldType::kControl));
  }

  auto promo_bubble = std::make_unique<views::BubbleDialogModelHost>(
      dialog_model_builder.Build(), anchor_view,
      views::BubbleBorder::TOP_RIGHT);

  if (ios_promo_config.with_header) {
    promo_bubble->SetFootnoteView(CreateContentView(
        bubble_delegate, ios_promo_config, /*with_title=*/true, bubble_type));
  }

  ios_promo_delegate_ = promo_bubble.get();
  current_promo_type_ = promo_type;

  if (highlighted_button) {
    promo_bubble->SetHighlightedButton(highlighted_button);
  } else {
    promo_bubble->set_highlight_button_when_shown(false);
  }

  views::Widget* const widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(promo_bubble));
  widget->Show();

  // `highlighted_button` can be null when the promo_bubble's page action is
  // anchored to the right hand side/RHS of the omnibox.
  if (highlighted_button) {
    highlighted_button->SetVisible(true);
  }
}

// static
void IOSPromoBubble::Hide() {
  if (ios_promo_delegate_) {
    ios_promo_delegate_->GetWidget()->Close();
  }
}

// static
bool IOSPromoBubble::IsPromoTypeVisible(IOSPromoType promo_type) {
  if (!ios_promo_delegate_) {
    return false;
  }

  return current_promo_type_ == promo_type;
}
