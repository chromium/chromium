// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/ios_promo_bubble_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/promos/ios_promo_trigger_service.h"
#include "chrome/browser/ui/promos/ios_promo_trigger_service_factory.h"
#include "chrome/browser/ui/promos/ios_promos_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/promos/ios_promo_bubble.h"
#include "chrome/browser/ui/views/promos/ios_promo_constants.h"
#include "chrome/browser/ui/views/user_education/impl/browser_user_education_context.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/desktop_to_mobile_promos/desktop_to_mobile_promos_metrics.h"
#include "components/desktop_to_mobile_promos/promos_types.h"
#include "components/sync_device_info/device_info.h"
#include "components/user_education/views/help_bubble_views.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/common/referrer.h"
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
#include "url/gurl.h"

using desktop_to_mobile_promos::DesktopPromoActionType;
using desktop_to_mobile_promos::DesktopPromoBubbleType;
using desktop_to_mobile_promos::LogDesktopPromoAction;
using desktop_to_mobile_promos::LogDesktopPromoBubbleCreated;
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

using desktop_to_mobile_promos::BubbleType;
using desktop_to_mobile_promos::PromoType;

// static
std::unique_ptr<IOSPromoBubbleView> IOSPromoBubbleView::Create(
    PromoType promo_type,
    const scoped_refptr<user_education::UserEducationContext>& context,
    user_education::FeaturePromoSpecification::BuildHelpBubbleParams params) {
  Profile* profile = context->AsA<BrowserUserEducationContext>()
                         ->GetBrowserView()
                         .GetProfile();
  IOSPromoTriggerService* service =
      IOSPromoTriggerServiceFactory::GetForProfile(profile);
  // If the user has a synced iOS device, show the reminder bubble. Otherwise,
  // show the QR code bubble.
  BubbleType promo_bubble_type = (service && service->GetIOSDeviceToRemind())
                                     ? BubbleType::kReminder
                                     : BubbleType::kQRCode;

  auto* const anchor_element = params.anchor_element.get();
  return std::make_unique<IOSPromoBubbleView>(
      profile, promo_type, promo_bubble_type,
      anchor_element->AsA<views::TrackedElementViews>()->view(),
      user_education::HelpBubbleViews::TranslateArrow(params.arrow));
}

IOSPromoBubbleView::IOSPromoBubbleView(Profile* profile,
                                       PromoType promo_type,
                                       BubbleType promo_bubble_type,
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

  LogDesktopPromoBubbleCreated(promo_type_, promo_bubble_type_);
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
  LogDesktopPromoAction(promo_type_, promo_bubble_type_,
                        DesktopPromoActionType::kCancel);

  NotifyUserAction(CustomHelpBubbleUi::UserAction::kCancel);
  return true;
}

bool IOSPromoBubbleView::Accept() {
  LogDesktopPromoAction(promo_type_, promo_bubble_type_,
                        DesktopPromoActionType::kAccept);

  switch (promo_bubble_type_) {
    case BubbleType::kReminder: {
      // Send the reminder to the iOS device and update the promo bubble with
      // the confirmation messaging.
      IOSPromoTriggerService* trigger_service =
          IOSPromoTriggerServiceFactory::GetForProfile(profile_);
      ios_device_info_ = trigger_service->GetIOSDeviceToRemind();
      if (!ios_device_info_) {
        return true;
      }

      trigger_service->SetReminderForIOSDevice(promo_type_,
                                               ios_device_info_->guid());
      promo_bubble_type_ = BubbleType::kReminderConfirmation;
      ShowReminderConfirmation();
      // Return false to prevent the bubble from closing on accept.
      return false;
    }
    case BubbleType::kQRCode: {
      content::OpenURLParams params(GURL(config_.qr_code_url),
                                    content::Referrer(),
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                    ui::PAGE_TRANSITION_LINK, false);

      if (open_url_callback_) {
        open_url_callback_.Run(params);
        return true;
      }

      if (views::View* anchor_view = GetAnchorView()) {
        if (BrowserView* browser_view =
                BrowserView::GetBrowserViewForNativeWindow(
                    anchor_view->GetWidget()->GetNativeWindow())) {
          browser_view->browser()->OpenURL(params,
                                           /*navigation_handle_callback=*/{});
        }
      }
      return true;
    }
    case BubbleType::kReminderConfirmation:
      return true;
  }
}

void IOSPromoBubbleView::SetOpenUrlCallbackForTesting(  // IN-TEST
    OpenUrlCallback callback) {
  open_url_callback_ = std::move(callback);
}

void IOSPromoBubbleView::SetWidth(views::DistanceMetric metric) {
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(metric));
}
void IOSPromoBubbleView::OnDismissal() {
  LogDesktopPromoAction(promo_type_, promo_bubble_type_,
                        DesktopPromoActionType::kDismiss);

  NotifyUserAction(CustomHelpBubbleUi::UserAction::kDismiss);
}

void IOSPromoBubbleView::ShowReminderConfirmation() {
  if (!ios_device_info_) {
    return;
  }

  config_ = IOSPromoBubble::SetUpBubble(promo_type_,
                                        BubbleType::kReminderConfirmation);

  std::u16string device_name;
  switch (ios_device_info_->form_factor()) {
    case syncer::DeviceInfo::FormFactor::kTablet:
      device_name = l10n_util::GetStringUTF16(IDS_IOS_DEVICE_TYPE_IPAD);
      break;
    case syncer::DeviceInfo::FormFactor::kPhone:
      device_name = l10n_util::GetStringUTF16(IDS_IOS_DEVICE_TYPE_IPHONE);
      break;
    default:
      NOTREACHED();
  }

  // Update the title.
  SetTitle(l10n_util::GetStringFUTF16(config_.promo_title_id, device_name));

  // Update description based on promo type.
  if (auto* description = views::AsViewClass<views::Label>(
          GetViewByID(IOSPromoConstants::kDescriptionLabelID))) {
    description->SetText(GetConfirmationDescriptionText(device_name));
  }

  if (auto* image = views::AsViewClass<views::ImageView>(
          GetViewByID(IOSPromoConstants::kImageViewID))) {
    image->SetImage(config_.promo_image);
  }

  // Reconfigure buttons to remove the kCancel button and update the title of
  // the kOk button.
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(config_.accept_button_text_id));
  SetCloseCallback(
      base::BindOnce(&IOSPromoBubbleView::OnDismissal, base::Unretained(this)));

  LogDesktopPromoBubbleCreated(promo_type_, promo_bubble_type_);

  // Trigger a resize and layout pass.
  GetWidget()->UpdateWindowTitle();
  SizeToContents();
}

std::u16string IOSPromoBubbleView::GetConfirmationDescriptionText(
    const std::u16string& device_name) {
  switch (promo_type_) {
    case PromoType::kPassword:
      return l10n_util::GetStringFUTF16(config_.promo_description_id,
                                        device_name);
    case PromoType::kEnhancedBrowsing:
    case PromoType::kLens:
      return l10n_util::GetStringFUTF16(
          config_.promo_description_id,
          l10n_util::GetStringUTF16(config_.feature_name_id));
    case PromoType::kAddress:
    case PromoType::kPayment:
      // These types do not have a reminder flow.
      NOTREACHED();
  }
}

BEGIN_METADATA(IOSPromoBubbleView)
END_METADATA
