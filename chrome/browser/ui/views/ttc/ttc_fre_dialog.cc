// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ttc/ttc_fre_dialog.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "base/check_deref.h"
#include "chrome/grit/theme_resources.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ttc {

namespace {

constexpr char kDialogName[] = "TtcFreDialog";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr int kBannerTopPadding = 20;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

constexpr int kCardRowIconSize = 16;
constexpr gfx::Insets kCardRowInsets = gfx::Insets::VH(12, 16);

std::unique_ptr<views::View> CreateCardRow(
    const gfx::VectorIcon& icon,
    int message_id,
    const gfx::RoundedCornersF& corners) {
  auto row = std::make_unique<views::BoxLayoutView>();
  row->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  row->SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart);
  row->SetBetweenChildSpacing(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  row->SetInsideBorderInsets(kCardRowInsets);
  row->SetBackground(
      views::CreateRoundedRectBackground(ui::kColorSysSurface2, corners));

  auto* const icon_view = row->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          icon, ui::kColorSysPrimary, kCardRowIconSize)));

  auto* const label = row->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(message_id), views::style::CONTEXT_LABEL,
      views::style::STYLE_BODY_4));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(ui::kColorSysOnSurface);
  label->SetBackgroundColor(row->background()->color());
  row->SetFlexForView(label, 1);

  icon_view->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          std::max(0, (label->GetLineHeight() - kCardRowIconSize) / 2), 0, 0,
          0));

  return row;
}

std::unique_ptr<views::View> CreateCard() {
  const ChromeLayoutProvider* const layout_provider =
      ChromeLayoutProvider::Get();
  const float corner_radius = layout_provider->GetCornerRadiusMetric(
      ChromeShapeContextTokens::kFeatureFirstRunInfoBoxRoundedBorderRadius);

  auto card = std::make_unique<views::BoxLayoutView>();
  card->SetOrientation(views::BoxLayout::Orientation::kVertical);
  card->SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStretch);
  card->SetBetweenChildSpacing(layout_provider->GetDistanceMetric(
      DISTANCE_FEATURE_FIRST_RUN_INFO_BOX_VERTICAL));

  card->AddChildView(
      CreateCardRow(kCursorSparkIcon, IDS_TTC_FRE_ACTIONS,
                    gfx::RoundedCornersF(corner_radius, corner_radius, 0, 0)));
  card->AddChildView(CreateCardRow(
      kScreensaverAutoIcon, IDS_TTC_FRE_DATA_SHARING, gfx::RoundedCornersF()));
  card->AddChildView(
      CreateCardRow(vector_icons::kMicIcon, IDS_TTC_FRE_MICROPHONE,
                    gfx::RoundedCornersF(0, 0, corner_radius, corner_radius)));

  return card;
}

std::unique_ptr<views::View> CreateDisclaimer(
    base::RepeatingClosure learn_more_callback) {
  const std::u16string link_text = l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  size_t link_offset = 0;
  const std::u16string text = l10n_util::GetStringFUTF16(
      IDS_TTC_FRE_DISCLAIMER, link_text, &link_offset);

  auto disclaimer = std::make_unique<views::StyledLabel>();
  disclaimer->SetText(text);
  disclaimer->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  disclaimer->SetDefaultTextStyle(views::style::STYLE_BODY_5);
  disclaimer->SetDefaultEnabledColorId(ui::kColorSysOnSurfaceVariant);

  auto link = std::make_unique<views::Link>(
      link_text, views::style::CONTEXT_LABEL, views::style::STYLE_LINK_5);
  link->SetCallback(std::move(learn_more_callback));
  link->SetProperty(views::kElementIdentifierKey,
                    TtcFreDialog::kLearnMoreLinkElementId);
  views::StyledLabel::RangeStyleInfo link_style;
  link_style.custom_view = link.get();
  disclaimer->AddCustomView(std::move(link));
  disclaimer->AddStyleRange(
      gfx::Range(link_offset, link_offset + link_text.length()), link_style);

  return disclaimer;
}

std::unique_ptr<views::View> CreateBody(
    base::RepeatingClosure learn_more_callback) {
  auto body = std::make_unique<views::BoxLayoutView>();
  body->SetOrientation(views::BoxLayout::Orientation::kVertical);
  body->SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStretch);
  body->SetBetweenChildSpacing(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL));
  body->AddChildView(CreateCard());
  body->AddChildView(CreateDisclaimer(std::move(learn_more_callback)));
  return body;
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void AddBanner(views::BubbleDialogModelHost* host) {
  auto banner = std::make_unique<views::ImageView>(
      ui::ImageModel::FromResourceId(IDR_TTC_FRE_BANNER));
  banner->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(kBannerTopPadding, 0, 0, 0)));
  CHECK_DEREF(host->GetBubbleFrameView()).SetHeaderView(std::move(banner));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TtcFreDialog, kDialogElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TtcFreDialog, kAllowButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TtcFreDialog, kCancelButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(TtcFreDialog, kLearnMoreLinkElementId);

TtcFreDialog::TtcFreDialog(gfx::NativeWindow parent,
                           ResultCallback on_closed,
                           base::RepeatingClosure on_learn_more)
    : on_closed_(std::move(on_closed)) {
  CHECK(parent);
  CHECK(on_closed_);
  CHECK(on_learn_more);

  std::unique_ptr<ui::DialogModel> model =
      ui::DialogModel::Builder()
          .SetInternalName(kDialogName)
          .SetElementIdentifier(kDialogElementId)
          .SetTitle(l10n_util::GetStringUTF16(IDS_TTC_FRE_TITLE))
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  CreateBody(std::move(on_learn_more)),
                  views::BubbleDialogModelHost::FieldType::kText))
          .AddOkButton(
              base::DoNothing(),
              ui::DialogModel::Button::Params()
                  .SetLabel(l10n_util::GetStringUTF16(IDS_TTC_FRE_ALLOW))
                  .SetId(kAllowButtonElementId))
          .AddCancelButton(base::DoNothing(),
                           ui::DialogModel::Button::Params()
                               .SetLabel(l10n_util::GetStringUTF16(IDS_CANCEL))
                               .SetId(kCancelButtonElementId))
          .SetEnableInputProtection(true)
          .Build();

  std::unique_ptr<views::BubbleDialogModelHost> host =
      views::BubbleDialogModelHost::CreateModal(std::move(model),
                                                ui::mojom::ModalType::kWindow);
  const views::LayoutProvider* const layout_provider =
      views::LayoutProvider::Get();
  host->set_fixed_width(layout_provider->GetDistanceMetric(
      views::DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH));
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const gfx::Insets dialog_insets =
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG);
  host->set_frame_margins(
      {.title =
           gfx::Insets::TLBR(layout_provider->GetDistanceMetric(
                                 views::DISTANCE_RELATED_CONTROL_VERTICAL),
                             dialog_insets.left(), 0, dialog_insets.right())});
  host->RegisterWidgetInitializedCallback(
      base::BindOnce(&AddBanner, base::Unretained(host.get())));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  host->SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  widget_ = base::WrapUnique(constrained_window::CreateBrowserModalDialogViews(
      std::move(host), parent));
  widget_->MakeCloseSynchronous(
      base::BindOnce(&TtcFreDialog::OnClosed, weak_ptr_factory_.GetWeakPtr()));
}

TtcFreDialog::~TtcFreDialog() = default;

void TtcFreDialog::Show() {
  if (widget_) {
    widget_->Show();
  }
}

void TtcFreDialog::Activate() {
  if (widget_) {
    widget_->Activate();
  }
}

void TtcFreDialog::OnClosed(views::Widget::ClosedReason reason) {
  widget_.reset();
  // May destroy `this`.
  std::move(on_closed_)
      .Run(reason == views::Widget::ClosedReason::kAcceptButtonClicked);
}

}  // namespace ttc
