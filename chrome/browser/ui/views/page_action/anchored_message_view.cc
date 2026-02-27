// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/anchored_message_view.h"

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace page_actions {

namespace {

// ChipContainerView holds the clickable chip of the anchored message, similar
// to the Suggestion Chip version of the Page Action View.
class ChipContainerView : public views::View {
  METADATA_HEADER(ChipContainerView, views::View)
 public:
  ChipContainerView(const std::u16string& label_text,
                    const ui::ImageModel& icon,
                    base::RepeatingClosure callback)
      : icon_(icon), callback_(callback) {
    icon_view_ = AddChildView(std::make_unique<views::ImageView>());
    label_ = AddChildView(std::make_unique<views::Label>(label_text));
    label_->SetAutoColorReadabilityEnabled(false);

    SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 4))
        ->set_cross_axis_alignment(
            views::BoxLayout::CrossAxisAlignment::kCenter);
    SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 16, 0, 0));
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(4, 15)));
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    const ui::ColorProvider* color_provider = GetColorProvider();

    if (!color_provider) {
      return;
    }

    SetBackground(views::CreateRoundedRectBackground(
        color_provider->GetColor(ui::kColorSysPrimary), 18));
    if (icon_view_) {
      // Assuming `icon_` is a vector icon, re-colorize it.
      if (icon_.IsVectorIcon()) {
        ui::ImageModel colored_icon = ui::ImageModel::FromVectorIcon(
            *icon_.GetVectorIcon().vector_icon(),
            color_provider->GetColor(ui::kColorSysOnPrimary), 20);
        icon_view_->SetImage(colored_icon);
      } else {
        icon_view_->SetImage(icon_);
      }
      icon_view_->SetImageSize(gfx::Size(20, 20));
    }
    if (label_) {
      label_->SetEnabledColor(color_provider->GetColor(ui::kColorSysOnPrimary));
    }
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (event.IsOnlyLeftMouseButton()) {
      CHECK(callback_);
      callback_.Run();
    }
    // If the event has been handled, this will never be reached, so we can just
    // always return false.
    return false;
  }

 private:
  raw_ptr<views::ImageView> icon_view_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
  ui::ImageModel icon_;
  base::RepeatingClosure callback_;
};

BEGIN_METADATA(ChipContainerView)
END_METADATA

}  // namespace

AnchoredMessageBubbleView::AnchoredMessageBubbleView(
    views::BubbleAnchor parent,
    const PageActionModelInterface& model,
    base::RepeatingClosure chip_callback,
    base::RepeatingClosure close_callback)
    : BubbleDialogDelegate(parent,
                           views::BubbleBorder::Arrow::TOP_RIGHT,
                           views::BubbleBorder::DIALOG_SHADOW,
                           true),
      label_text_(model.GetAnchoredMessageText()),
      show_close_button_(model.GetAnchoredMessageCloseIcon()),
      chip_callback_(std::move(chip_callback)),
      close_callback_(std::move(close_callback)) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetBackgroundColor(ui::kColorSysSurface);
  set_corner_radius(26);
  set_margins(gfx::Insets::TLBR(8, 16, 8, 16));

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  label_ = AddChildView(std::make_unique<views::Label>(label_text_));
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetVerticalAlignment(gfx::ALIGN_MIDDLE);
  label_->SetMultiLine(false);

  chip_container_ = AddChildView(std::make_unique<ChipContainerView>(
      model.GetText(), model.GetImage(),
      base::BindRepeating(&AnchoredMessageBubbleView::ChipCallback,
                          base::Unretained(this))));

  if (show_close_button_) {
    close_button_ =
        AddChildView(std::make_unique<views::ImageButton>(close_callback_));
    close_button_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(vector_icons::kCloseRoundedIcon,
                                       ui::kColorIcon, 16));
    close_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_CLOSE));
    close_button_->SetProperty(views::kMarginsKey,
                               gfx::Insets::TLBR(0, 8, 0, 0));
  }
}

void AnchoredMessageBubbleView::OnThemeChanged() {
  views::View::OnThemeChanged();
  const ui::ColorProvider* color_provider = GetColorProvider();

  if (!color_provider) {
    return;
  }

  label_->SetEnabledColor(color_provider->GetColor(ui::kColorSysOnSurface));

  if (close_button_) {
    close_button_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            vector_icons::kCloseRoundedIcon,
            color_provider->GetColor(ui::kColorSysOnSurfaceVariant), 16));
  }
}

bool AnchoredMessageBubbleView::CanActivate() const {
  return false;  // This widget should not take focus
}

views::View* AnchoredMessageBubbleView::GetContentsView() {
  return this;
}

views::Widget* AnchoredMessageBubbleView::GetWidget() {
  return View::GetWidget();
}

const views::Widget* AnchoredMessageBubbleView::GetWidget() const {
  return View::GetWidget();
}

AnchoredMessageBubbleView::~AnchoredMessageBubbleView() {
  SetAnchorView(nullptr);
}

void AnchoredMessageBubbleView::ChipCallback() {
  CHECK(close_callback_);
  CHECK(chip_callback_);
  close_callback_.Run();
  chip_callback_.Run();
}

BEGIN_METADATA(AnchoredMessageBubbleView)
END_METADATA

}  // namespace page_actions
