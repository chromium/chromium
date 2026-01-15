// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/secure_payment_confirmation_dialog_view.h"

#include <memory>

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/browser/ui/views/payments/secure_payment_confirmation_views_util.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/payments/content/payment_ui_observer.h"
#include "components/payments/content/secure_payment_confirmation_model.h"
#include "components/payments/core/sizes.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace payments {

namespace {

class BorderedRowView : public views::View {
  METADATA_HEADER(BorderedRowView, views::View)

 public:
  void OnThemeChanged() override {
    View::OnThemeChanged();
    SetBorder(views::CreateSolidSidedBorder(
        gfx::Insets::TLBR(0, 0, 1, 0),
        GetColorProvider()->GetColor(ui::kColorSeparator)));
  }
};

BEGIN_METADATA(BorderedRowView)
END_METADATA

class DefaultHeaderImage : public NonAccessibleImageView {
  METADATA_HEADER(DefaultHeaderImage, NonAccessibleImageView)

 public:
  DefaultHeaderImage() {
    SetPreferredSize(
        gfx::Size(ChromeLayoutProvider::Get()->GetDistanceMetric(
                      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH),
                  kSecurePaymentConfirmationDefaultHeaderLogoHeight));
    SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  }
  ~DefaultHeaderImage() override = default;

  // NonAccessibleImageView:
  void OnThemeChanged() override {
    NonAccessibleImageView::OnThemeChanged();
    SetImage(ui::ImageModel::FromVectorIcon(
        GetNativeTheme()->preferred_color_scheme() ==
                ui::NativeTheme::PreferredColorScheme::kDark
            ? kSecurePaymentConfirmationHeaderDarkIcon
            : kSecurePaymentConfirmationHeaderIcon,
        ui::kColorDialogBackground));
  }
};

BEGIN_METADATA(DefaultHeaderImage)
END_METADATA

std::unique_ptr<views::View> CreateSpacer(
    views::DistanceMetric vertical_distance) {
  return views::Builder<views::View>()
      .SetPreferredSize(gfx::Size(
          /*width=*/1,
          views::LayoutProvider::Get()->GetDistanceMetric(vertical_distance)))
      .Build();
}

std::unique_ptr<views::View> CreateSeparator(
    views::DistanceMetric vertical_distance) {
  return views::Builder<views::Separator>()
      .SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
          views::LayoutProvider::Get()->GetDistanceMetric(vertical_distance),
          0)))
      .SetColorId(ui::kColorSeparator)
      .Build();
}

void UpdateProgressBarVisiblity(views::BubbleFrameView* bubble_frame_view,
                                bool visible) {
  if (bubble_frame_view) {
    // -1 indicates an infinitely animating progress
    bubble_frame_view->SetProgress(visible ? std::optional<double>(-1)
                                           : std::nullopt);
  }
}

}  // namespace

// static
base::WeakPtr<SecurePaymentConfirmationView>
SecurePaymentConfirmationView::Create(
    const base::WeakPtr<PaymentUIObserver> payment_ui_observer) {
  // On desktop, the SecurePaymentConfirmationView object is memory managed by
  // the views:: machinery. It is deleted when the window is closed and
  // views::DialogDelegateView::DeleteDelegate() is called by its corresponding
  // views::Widget.
  return (new SecurePaymentConfirmationDialogView(
              /*observer_for_test=*/nullptr, payment_ui_observer))
      ->GetWeakPtr();
}

SecurePaymentConfirmationView::SecurePaymentConfirmationView() = default;
SecurePaymentConfirmationView::~SecurePaymentConfirmationView() = default;

SecurePaymentConfirmationDialogView::SecurePaymentConfirmationDialogView(
    base::WeakPtr<ObserverForTest> observer_for_test,
    const base::WeakPtr<PaymentUIObserver> ui_observer_for_test)
    : observer_for_test_(observer_for_test),
      ui_observer_for_test_(ui_observer_for_test) {}
SecurePaymentConfirmationDialogView::~SecurePaymentConfirmationDialogView() =
    default;

void SecurePaymentConfirmationDialogView::ShowDialog(
    content::WebContents* web_contents,
    base::WeakPtr<SecurePaymentConfirmationModel> model,
    VerifyCallback verify_callback,
    AnotherWayCallback another_way_callback,
    CancelCallback cancel_callback,
    OptOutCallback opt_out_callback) {
  DCHECK(model);
  model_ = model;

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  if (base::FeatureList::IsEnabled(
          blink::features::kSecurePaymentConfirmationUxRefresh)) {
    InitViews();
  } else {
    InitChildViews();
  }

  OnModelUpdated();

  verify_callback_ = std::move(verify_callback);
  cancel_callback_ = std::move(cancel_callback);
  another_way_callback_ = std::move(another_way_callback);
  opt_out_callback_ = std::move(opt_out_callback);

  SetAcceptCallback(
      base::BindOnce(&SecurePaymentConfirmationDialogView::OnDialogAccepted,
                     weak_ptr_factory_.GetWeakPtr()));
  SetCancelCallback(
      base::BindOnce(&SecurePaymentConfirmationDialogView::OnDialogCancelled,
                     weak_ptr_factory_.GetWeakPtr()));
  SetCloseCallback(
      base::BindOnce(&SecurePaymentConfirmationDialogView::OnDialogClosed,
                     weak_ptr_factory_.GetWeakPtr()));

  SetModalType(ui::mojom::ModalType::kChild);

  views::Widget* widget =
      constrained_window::ShowWebModalDialogViews(this, web_contents);
  extensions::SecurityDialogTracker::GetInstance()->AddSecurityDialog(widget);
  occlusion_observation_.Observe(widget);

  // The progress bar doesn't exist until after ShowWebModalDialogViews, so we
  // have to update it here in case it starts visible.
  UpdateProgressBarVisiblity(GetBubbleFrameView(),
                             model_->progress_bar_visible());

  // ui_observer_for_test_ is used in platform browsertests.
  if (ui_observer_for_test_) {
    ui_observer_for_test_->OnUIDisplayed();
  }
}

void SecurePaymentConfirmationDialogView::OnDialogAccepted() {
  std::move(verify_callback_).Run();
  if (observer_for_test_) {
    observer_for_test_->OnConfirmButtonPressed();
    observer_for_test_->OnDialogClosed();
  }
}

void SecurePaymentConfirmationDialogView::OnDialogCancelled() {
  std::move(cancel_callback_).Run();
  if (observer_for_test_) {
    observer_for_test_->OnCancelButtonPressed();
    observer_for_test_->OnDialogClosed();
  }
}

void SecurePaymentConfirmationDialogView::OnDialogClosed() {
  // Cancel callback may be null if OnDialogCancelled was already called.
  if (!cancel_callback_) {
    return;
  }

  std::move(cancel_callback_).Run();

  if (observer_for_test_) {
    observer_for_test_->OnDialogClosed();
  }
}

void SecurePaymentConfirmationDialogView::OnAnotherWayClicked() {
  std::move(another_way_callback_).Run();
}

void SecurePaymentConfirmationDialogView::OnOptOutClicked() {
  if (observer_for_test_) {
    observer_for_test_->OnOptOutClicked();
  }
  std::move(opt_out_callback_).Run();
}

void SecurePaymentConfirmationDialogView::OnModelUpdated() {
  UpdateProgressBarVisiblity(GetBubbleFrameView(),
                             model_->progress_bar_visible());

  SetButtonLabel(ui::mojom::DialogButton::kOk, model_->verify_button_label());
  SetButtonEnabled(ui::mojom::DialogButton::kOk,
                   model_->verify_button_enabled());
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 model_->cancel_button_label());
  SetButtonEnabled(ui::mojom::DialogButton::kCancel,
                   model_->cancel_button_enabled());

  SetAccessibleTitle(model_->title());
  UpdateLabelView(DialogViewID::TITLE, model_->title());

  // Update the instrument icon only if it's changed
  if (model_->instrument_icon()) {
    auto* image_view = static_cast<views::ImageView*>(
        GetViewByID(static_cast<int>(DialogViewID::INSTRUMENT_ICON)));
    if (model_->instrument_icon() != instrument_icon_ ||
        model_->instrument_icon()->getGenerationID() !=
            instrument_icon_generation_id_) {
      instrument_icon_generation_id_ =
          model_->instrument_icon()->getGenerationID();
      gfx::ImageSkia image =
          gfx::ImageSkia::CreateFrom1xBitmap(*model_->instrument_icon())
              .DeepCopy();
      image_view->SetImage(ui::ImageModel::FromImageSkia(image));
    }
    if (model_->instrument_icon()->drawsNothing()) {
      image_view->SetImage(ui::ImageModel::FromVectorIcon(
          kCreditCardIcon, ui::kColorDialogForeground,
          kSecurePaymentConfirmationIconDefaultWidthPx));
    }
  }
  instrument_icon_ = model_->instrument_icon();
  UpdateLabelView(DialogViewID::INSTRUMENT_VALUE, model_->instrument_value());

  UpdateLabelView(DialogViewID::TOTAL_VALUE, model_->total_value());

  opt_out_view_->SetVisible(model_->opt_out_visible());

  if (base::FeatureList::IsEnabled(
          blink::features::kSecurePaymentConfirmationUxRefresh)) {
    if (model_->merchant_name().has_value() &&
        model_->merchant_origin().has_value()) {
      UpdateLabelView(DialogViewID::MERCHANT_VALUE,
                      model_->merchant_name().value());
      UpdateLabelView(DialogViewID::MERCHANT_SECONDARY_VALUE,
                      model_->merchant_origin().value());
      GetViewByID(static_cast<int>(DialogViewID::MERCHANT_SECONDARY_VALUE))
          ->SetVisible(true);
    } else {
      UpdateLabelView(
          DialogViewID::MERCHANT_VALUE,
          model_->merchant_name().value_or(
              model_->merchant_origin().value_or(std::u16string())));
      GetViewByID(static_cast<int>(DialogViewID::MERCHANT_SECONDARY_VALUE))
          ->SetVisible(false);
    }
    UpdateLabelView(DialogViewID::INSTRUMENT_SECONDARY_VALUE,
                    model_->instrument_details_value());
    GetViewByID(static_cast<int>(DialogViewID::INSTRUMENT_SECONDARY_VALUE))
        ->SetVisible(!model_->instrument_details_value().empty());
    footer_view_->SetVisible(model_->footer_visible());
  } else {
    UpdateLabelView(DialogViewID::MERCHANT_LABEL, model_->merchant_label());
    UpdateLabelView(DialogViewID::MERCHANT_VALUE,
                    FormatMerchantLabel(model_->merchant_name(),
                                        model_->merchant_origin()));
    UpdateLabelView(DialogViewID::INSTRUMENT_LABEL, model_->instrument_label());
    UpdateLabelView(DialogViewID::TOTAL_LABEL, model_->total_label());
  }
}

void SecurePaymentConfirmationDialogView::UpdateLabelView(
    DialogViewID id,
    const std::u16string& text) {
  static_cast<views::Label*>(GetViewByID(static_cast<int>(id)))->SetText(text);
}

void SecurePaymentConfirmationDialogView::HideDialog() {
  if (GetWidget()) {
    GetWidget()->Close();
  }
}

bool SecurePaymentConfirmationDialogView::ClickOptOutForTesting() {
  if (!model_->opt_out_visible()) {
    return false;
  }
  OnOptOutClicked();
  return true;
}

bool SecurePaymentConfirmationDialogView::ShouldShowCloseButton() const {
  return false;
}

bool SecurePaymentConfirmationDialogView::Accept() {
  views::DialogDelegateView::Accept();

  // Disable the footer/opt-out links to avoid the user clicking on it whilst
  // the WebAuthn dialog is showing over the SPC one. Note that this is only
  // necessarily if the text is visible.
  // TODO(crbug.com/476172795): Even disabled this link still looks clickable
  // (underline disappears, but color doesn't change).
  if (base::FeatureList::IsEnabled(
          blink::features::kSecurePaymentConfirmationUxRefresh)) {
    if (footer_view_->GetVisible()) {
      footer_view_->SetEnabled(false);
    }
  }
  if (opt_out_view_->GetVisible()) {
    opt_out_view_->SetEnabled(false);
  }

  // Returning "false" to keep the dialog open after "Confirm" button is
  // pressed, so the dialog can show a progress bar and wait for the user to use
  // their authenticator device.
  return false;
}

base::WeakPtr<SecurePaymentConfirmationDialogView>
SecurePaymentConfirmationDialogView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void SecurePaymentConfirmationDialogView::InitChildViews() {
  RemoveAllChildViews();

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

  AddChildView(CreateSecurePaymentConfirmationHeaderIcon(
      static_cast<int>(DialogViewID::HEADER_ICON)));

  AddChildView(CreateBodyView());

  // We always create the view for the Opt Out link, but show or hide it
  // depending on whether it was requested. The visibility status is set in
  // OnModelUpdated.
  opt_out_view_ = SetFootnoteView(CreateSecurePaymentConfirmationOptOutView(
      model_->relying_party_id(), model_->opt_out_label(),
      model_->opt_out_link_label(),
      base::BindRepeating(&SecurePaymentConfirmationDialogView::OnOptOutClicked,
                          weak_ptr_factory_.GetWeakPtr())));

  InvalidateLayout();
}

// Creates the body including the title, the set of merchant, instrument, and
// total rows.
// +------------------------------------------+
// | Title                                    |
// |                                          |
// | merchant label      value                |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
// | instrument label    [icon] value         |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
// | total label         value                |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
std::unique_ptr<views::View>
SecurePaymentConfirmationDialogView::CreateBodyView() {
  auto body_view = std::make_unique<views::BoxLayoutView>();
  body_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  body_view->SetInsideBorderInsets(
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));
  body_view->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
  body_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  std::unique_ptr<views::Label> title_text =
      CreateSecurePaymentConfirmationTitleLabel(model_->title());
  title_text->SetID(static_cast<int>(DialogViewID::TITLE));
  body_view->AddChildView(std::move(title_text));

  body_view->AddChildView(
      CreateSpacer(views::DISTANCE_RELATED_CONTROL_VERTICAL));

  body_view->AddChildView(CreateRowView(
      model_->merchant_label(), DialogViewID::MERCHANT_LABEL,
      FormatMerchantLabel(model_->merchant_name(), model_->merchant_origin()),
      DialogViewID::MERCHANT_VALUE));

  body_view->AddChildView(
      CreateRowView(model_->instrument_label(), DialogViewID::INSTRUMENT_LABEL,
                    model_->instrument_value(), DialogViewID::INSTRUMENT_VALUE,
                    model_->instrument_icon(), DialogViewID::INSTRUMENT_ICON));

  body_view->AddChildView(
      CreateRowView(model_->total_label(), DialogViewID::TOTAL_LABEL,
                    model_->total_value(), DialogViewID::TOTAL_VALUE));

  return body_view;
}

// Creates a row of data with |label|, |value|, and optionally |icon|.
// +------------------------------------------+
// | label      [icon] value                  |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+ <-- border
std::unique_ptr<views::View> SecurePaymentConfirmationDialogView::CreateRowView(
    const std::u16string& label,
    DialogViewID label_id,
    const std::u16string& value,
    DialogViewID value_id,
    const SkBitmap* icon,
    DialogViewID icon_id) {
  auto row = std::make_unique<BorderedRowView>();

  views::TableLayout* layout =
      row->SetLayoutManager(std::make_unique<views::TableLayout>());

  // Label column
  constexpr int kLabelColumnWidth = 80;
  layout->AddColumn(
      views::LayoutAlignment::kStart, views::LayoutAlignment::kCenter,
      views::TableLayout::kFixedSize, views::TableLayout::ColumnSize::kFixed,
      kLabelColumnWidth, 0);

  constexpr int kPaddingAfterLabel = 24;
  layout->AddPaddingColumn(views::TableLayout::kFixedSize, kPaddingAfterLabel);

  // Icon column
  if (icon) {
    layout->AddColumn(
        views::LayoutAlignment::kStart, views::LayoutAlignment::kCenter,
        views::TableLayout::kFixedSize,
        views::TableLayout::ColumnSize::kUsePreferred,
        /*fixed_width=*/0,
        /*min_width=*/kSecurePaymentConfirmationIconDefaultWidthPx);
    layout->AddPaddingColumn(views::TableLayout::kFixedSize,
                             ChromeLayoutProvider::Get()->GetDistanceMetric(
                                 views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  }

  // Value column
  layout->AddColumn(views::LayoutAlignment::kStretch,
                    views::LayoutAlignment::kCenter, 1.0f,
                    views::TableLayout::ColumnSize::kUsePreferred, 0, 0);

  layout->AddRows(1, views::TableLayout::kFixedSize, kPaymentInfoRowHeight);

  std::unique_ptr<views::Label> label_text = std::make_unique<views::Label>(
      label, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_EMPHASIZED_SECONDARY);
  label_text->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  label_text->SetLineHeight(kDescriptionLineHeight);
  label_text->SetID(static_cast<int>(label_id));
  row->AddChildView(std::move(label_text));

  if (icon) {
    gfx::ImageSkia skia_icon;

    // TODO(crbug.com/333945861): CreateRowView shouldn't need to know to do
    // clever things with the instrument icon. The empty icon should be resolved
    // before calling this method.
    if (icon_id == DialogViewID::INSTRUMENT_ICON) {
      instrument_icon_ = model_->instrument_icon();
      instrument_icon_generation_id_ =
          model_->instrument_icon()->getGenerationID();

      // The instrument icon may be empty, if it couldn't be downloaded/decoded
      // and iconMustBeShown was set to false. In that case, use a default icon.
      // The actual display color is set based on the theme in OnThemeChanged.
      if (instrument_icon_->drawsNothing()) {
        skia_icon = gfx::CreateVectorIcon(
            kCreditCardIcon, kSecurePaymentConfirmationIconDefaultWidthPx,
            gfx::kPlaceholderColor);
      } else {
        skia_icon =
            gfx::ImageSkia::CreateFrom1xBitmap(*model_->instrument_icon())
                .DeepCopy();
      }
    } else {
      skia_icon = gfx::ImageSkia::CreateFrom1xBitmap(*icon).DeepCopy();
    }

    std::unique_ptr<views::ImageView> icon_view =
        CreateSecurePaymentConfirmationIconView(std::move(skia_icon));
    icon_view->SetID(static_cast<int>(icon_id));
    row->AddChildView(std::move(icon_view));
  }

  std::unique_ptr<views::Label> value_text = std::make_unique<views::Label>(
      value, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY);
  value_text->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  value_text->SetLineHeight(kDescriptionLineHeight);
  value_text->SetID(static_cast<int>(value_id));
  row->AddChildView(std::move(value_text));

  return row;
}

void SecurePaymentConfirmationDialogView::InitViews() {
  RemoveAllChildViews();

  SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG),
          0))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  // Header
  AddChildView(CreateHeaderView());
  AddChildView(CreateSpacer(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  // Title
  std::unique_ptr<views::Label> title = std::make_unique<views::Label>(
      model_->title(), views::style::CONTEXT_DIALOG_TITLE,
      views::style::STYLE_HEADLINE_4);
  title->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  title->SetID(static_cast<int>(DialogViewID::TITLE));
  AddChildView(std::move(title));
  AddChildView(CreateSpacer(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  // Merchant Row
  if (model_->merchant_name().has_value() &&
      model_->merchant_origin().has_value()) {
    AddChildView(CreateNewRowView(
        ui::ImageModel::FromVectorIcon(
            vector_icons::kStorefrontIcon, ui::kColorSysOnSurfaceSubtle,
            kSecurePaymentConfirmationIconDefaultWidthPx),
        DialogViewID::MERCHANT_ICON, model_->merchant_name().value(),
        DialogViewID::MERCHANT_VALUE, model_->merchant_origin().value(),
        DialogViewID::MERCHANT_SECONDARY_VALUE));
  } else {
    AddChildView(CreateNewRowView(
        ui::ImageModel::FromVectorIcon(
            vector_icons::kStorefrontIcon, ui::kColorSysOnSurfaceSubtle,
            kSecurePaymentConfirmationIconDefaultWidthPx),
        DialogViewID::MERCHANT_ICON,
        model_->merchant_name().value_or(
            model_->merchant_origin().value_or(std::u16string())),
        DialogViewID::MERCHANT_VALUE, std::u16string(),
        DialogViewID::MERCHANT_SECONDARY_VALUE));
  }
  AddChildView(CreateSeparator(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  // Provided instrument icon and its generation ID are stored to be
  // compared against updates later.
  instrument_icon_ = model_->instrument_icon();
  instrument_icon_generation_id_ = model_->instrument_icon()->getGenerationID();

  // Instrument Row
  ui::ImageModel instrument_icon;
  if (model_->instrument_icon()->drawsNothing()) {
    instrument_icon = ui::ImageModel::FromVectorIcon(
        kCreditCardIcon, ui::kColorSysOnSurfaceSubtle,
        kSecurePaymentConfirmationIconDefaultWidthPx);
  } else {
    instrument_icon = ui::ImageModel::FromImageSkia(
        gfx::ImageSkia::CreateFrom1xBitmap(*model_->instrument_icon())
            .DeepCopy());
  }
  AddChildView(CreateNewRowView(
      std::move(instrument_icon), DialogViewID::INSTRUMENT_LABEL,
      model_->instrument_value(), DialogViewID::INSTRUMENT_VALUE,
      model_->instrument_details_value(),
      DialogViewID::INSTRUMENT_SECONDARY_VALUE));
  AddChildView(CreateSeparator(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  // Total Row
  AddChildView(CreateNewRowView(
      ui::ImageModel::FromVectorIcon(
          vector_icons::kPaymentsIcon, ui::kColorSysOnSurfaceSubtle,
          kSecurePaymentConfirmationIconDefaultWidthPx),
      DialogViewID::TOTAL_ICON, model_->total_value(),
      DialogViewID::TOTAL_VALUE));

  // Footer
  footer_view_ = AddChildView(CreateFooterView());
  footer_view_->SetVisible(model_->footer_visible());

  // Opt out
  opt_out_view_ = AddChildView(CreateOptOutView());
  opt_out_view_->SetVisible(model_->opt_out_visible());

  InvalidateLayout();
}

std::unique_ptr<views::View>
SecurePaymentConfirmationDialogView::CreateHeaderView() {
  auto container = std::make_unique<views::BoxLayoutView>();
  container->SetOrientation(views::LayoutOrientation::kHorizontal);
  container->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
  container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Default to header image if no logos are provided.
  if (model_->header_logos().empty()) {
    auto image = std::make_unique<DefaultHeaderImage>();
    image->SetID(static_cast<int>(DialogViewID::HEADER_ICON));

    container->AddChildView(std::move(image));
    return container;
  }

  // Center the logo if there is only one.
  if (model_->header_logos().size() == 1) {
    auto logo = std::make_unique<views::ImageView>();
    logo->SetImage(
        ui::ImageModel::FromImageSkia(gfx::ImageSkia::CreateFrom1xBitmap(
            *model_->header_logos().at(0)->icon)));
    logo->SetImageSize(
        gfx::Size(payments::kSecurePaymentConfirmationHeaderLogoWidth,
                  payments::kSecurePaymentConfirmationHeaderLogoHeight));
    logo->SetAccessibleName(model_->header_logos().at(0)->label);

    container->AddChildView(std::move(logo));
    return container;
  }

  // If there are multiple logos, left align the first and right align the
  // second. Containers are used to achieve the alignment.
  auto* left_logo_container =
      container->AddChildView(std::make_unique<views::BoxLayoutView>());
  left_logo_container->SetOrientation(views::LayoutOrientation::kHorizontal);
  left_logo_container->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  left_logo_container->SetPreferredSize(
      gfx::Size(ChromeLayoutProvider::Get()->GetDistanceMetric(
                    views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) /
                    2,
                payments::kSecurePaymentConfirmationHeaderLogoHeight));
  auto* left_logo =
      left_logo_container->AddChildView(std::make_unique<views::ImageView>());
  left_logo->SetImage(ui::ImageModel::FromImageSkia(
      gfx::ImageSkia::CreateFrom1xBitmap(*model_->header_logos().at(0)->icon)));
  left_logo->SetImageSize(
      gfx::Size(payments::kSecurePaymentConfirmationHeaderLogoWidth,
                payments::kSecurePaymentConfirmationHeaderLogoHeight));
  left_logo->SetAccessibleName(model_->header_logos().at(0)->label);

  auto* right_logo_container =
      container->AddChildView(std::make_unique<views::BoxLayoutView>());
  right_logo_container->SetOrientation(views::LayoutOrientation::kHorizontal);
  right_logo_container->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  right_logo_container->SetPreferredSize(
      gfx::Size(ChromeLayoutProvider::Get()->GetDistanceMetric(
                    views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) /
                    2,
                payments::kSecurePaymentConfirmationHeaderLogoHeight));
  auto* right_logo =
      right_logo_container->AddChildView(std::make_unique<views::ImageView>());
  right_logo->SetHorizontalAlignment(views::ImageView::Alignment::kTrailing);
  right_logo->SetImage(ui::ImageModel::FromImageSkia(
      gfx::ImageSkia::CreateFrom1xBitmap(*model_->header_logos().at(1)->icon)));
  right_logo->SetImageSize(
      gfx::Size(payments::kSecurePaymentConfirmationHeaderLogoWidth,
                payments::kSecurePaymentConfirmationHeaderLogoHeight));
  right_logo->SetAccessibleName(model_->header_logos().at(1)->label);

  return container;
}

// Creates a row of data with |icon|, |value|, and optionally
// |secondary value|.
// +------------------------------------------+
// |  icon    value                           |
// |          secondary value                 |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+ <-- border
std::unique_ptr<views::View>
SecurePaymentConfirmationDialogView::CreateNewRowView(
    ui::ImageModel icon,
    DialogViewID icon_id,
    const std::u16string& value,
    DialogViewID value_id,
    const std::u16string& secondary_value,
    DialogViewID secondary_value_id) {
  auto row = std::make_unique<views::BoxLayoutView>();
  row->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  row->SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);

  auto icon_container = std::make_unique<views::BoxLayoutView>();
  icon_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  icon_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  icon_container->SetMinimumCrossAxisSize(
      kSecurePaymentConfirmationIconMaximumWidthPx);
  icon_container->SetInsideBorderInsets(
      gfx::Insets().set_right(views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  std::unique_ptr<views::ImageView> icon_view =
      CreateSecurePaymentConfirmationIconView(std::move(icon));
  icon_view->SetID(static_cast<int>(icon_id));
  icon_container->AddChildView(std::move(icon_view));
  row->AddChildView(std::move(icon_container));

  auto value_container = std::make_unique<views::BoxLayoutView>();
  value_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  std::unique_ptr<views::Label> value_view = std::make_unique<views::Label>(
      value, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_BODY_3_MEDIUM);
  value_view->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  value_view->SetID(static_cast<int>(value_id));
  value_container->AddChildView(std::move(value_view));

  std::unique_ptr<views::Label> secondary_value_view =
      std::make_unique<views::Label>(secondary_value,
                                     views::style::CONTEXT_DIALOG_BODY_TEXT,
                                     views::style::STYLE_BODY_4);
  secondary_value_view->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  secondary_value_view->SetID(static_cast<int>(secondary_value_id));
  // Always create the secondary value view, but set visibility based on value
  // presence. This is so a value can be added later on model updates.
  secondary_value_view->SetVisible(!secondary_value.empty());
  value_container->AddChildView(std::move(secondary_value_view));

  row->AddChildView(std::move(value_container));

  return row;
}

std::unique_ptr<views::View>
SecurePaymentConfirmationDialogView::CreateFooterView() {
  std::vector<std::u16string> substitutions{model_->footer_link_label()};
  std::vector<size_t> offsets;
  std::u16string text = base::ReplaceStringPlaceholders(
      model_->footer_label(), substitutions, &offsets);

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &SecurePaymentConfirmationDialogView::OnAnotherWayClicked,
          weak_ptr_factory_.GetWeakPtr()));

  return views::Builder<views::StyledLabel>()
      .SetText(text)
      .SetTextContext(ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL)
      .SetDefaultTextStyle(views::style::STYLE_BODY_4)
      .SetProperty(
          views::kMarginsKey,
          gfx::Insets().set_top(views::LayoutProvider::Get()->GetDistanceMetric(
              views::DISTANCE_UNRELATED_CONTROL_VERTICAL)))
      .AddStyleRange(
          gfx::Range(offsets[0],
                     offsets[0] + model_->footer_link_label().length()),
          link_style)
      .Build();
}

std::unique_ptr<views::View>
SecurePaymentConfirmationDialogView::CreateOptOutView() {
  std::vector<std::u16string> substitutions{
      model_->opt_out_authenticator_label(), model_->relying_party_id(),
      model_->opt_out_link_label()};
  std::vector<size_t> offsets;
  std::u16string text = base::ReplaceStringPlaceholders(
      model_->opt_out_label(), substitutions, &offsets);

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &SecurePaymentConfirmationDialogView::OnOptOutClicked,
          weak_ptr_factory_.GetWeakPtr()));

  return views::Builder<views::StyledLabel>()
      .SetText(text)
      .SetTextContext(ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL)
      .SetDefaultTextStyle(views::style::STYLE_BODY_4)
      .SetProperty(
          views::kMarginsKey,
          gfx::Insets().set_top(views::LayoutProvider::Get()->GetDistanceMetric(
              views::DISTANCE_UNRELATED_CONTROL_VERTICAL)))
      .AddStyleRange(
          gfx::Range(offsets[2],
                     offsets[2] + model_->opt_out_link_label().length()),
          link_style)
      .Build();
}

void SecurePaymentConfirmationDialogView::OnOcclusionStateChanged(
    bool occluded) {
  if (occluded) {
    SetEnabled(false);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&SecurePaymentConfirmationDialogView::HideDialog,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

BEGIN_METADATA(SecurePaymentConfirmationDialogView)
END_METADATA

}  // namespace payments
