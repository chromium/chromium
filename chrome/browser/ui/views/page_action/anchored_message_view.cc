// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/anchored_message_view.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
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
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace page_actions {

// ChipContainerView holds the clickable chip of the anchored message, similar
// to the Suggestion Chip version of the Page Action View.
class ChipContainerView : public views::View {
  METADATA_HEADER(ChipContainerView, views::View)
 public:
  ChipContainerView(const std::u16string& label_text,
                    const std::optional<ui::ImageModel>& icon,
                    base::RepeatingClosure callback)
      : callback_(callback) {
    icon_view_ = AddChildView(std::make_unique<views::ImageView>());
    icon_view_->SetVisible(false);
    label_ = AddChildView(std::make_unique<views::Label>());
    label_->SetVisible(false);
    label_->SetAutoColorReadabilityEnabled(false);

    SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 4))
        ->set_cross_axis_alignment(
            views::BoxLayout::CrossAxisAlignment::kCenter);
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(4, 15)));

    Update(label_text, icon);
  }

  ~ChipContainerView() override = default;

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    const ui::ColorProvider* color_provider = GetColorProvider();

    if (!color_provider) {
      return;
    }

    SetBackground(views::CreateRoundedRectBackground(
        color_provider->GetColor(ui::kColorSysPrimary), 18));
    if (icon_) {
      // Assuming `icon_` is a vector icon, re-colorize it.
      if (icon_->IsVectorIcon()) {
        ui::ImageModel colored_icon = ui::ImageModel::FromVectorIcon(
            *icon_->GetVectorIcon().vector_icon(),
            color_provider->GetColor(ui::kColorSysOnPrimary), 20);
        icon_view_->SetImage(colored_icon);
      } else {
        icon_view_->SetImage(icon_.value());
      }
      icon_view_->SetImageSize(gfx::Size(20, 20));
    }
    label_->SetEnabledColor(color_provider->GetColor(ui::kColorSysOnPrimary));
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

 public:
  void Update(const std::u16string& label_text,
              const std::optional<ui::ImageModel>& icon) {
    icon_ = icon;
    icon_view_->SetVisible(icon_ != std::nullopt);
    label_->SetVisible(label_text.size() > 0);
    label_->SetText(label_text);
    OnThemeChanged();
  }

 private:
  raw_ptr<views::ImageView> icon_view_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
  std::optional<ui::ImageModel> icon_;
  base::RepeatingClosure callback_;
};

BEGIN_METADATA(ChipContainerView)
END_METADATA

AnchoredMessageBubbleView::AnchoredMessageBubbleView(
    views::BubbleAnchor parent,
    const PageActionModelInterface& model,
    base::RepeatingClosure chip_callback,
    base::RepeatingClosure close_callback)
    : BubbleDialogDelegate(parent,
                           views::BubbleBorder::Arrow::TOP_RIGHT,
                           views::BubbleBorder::DIALOG_SHADOW,
                           true),
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

  icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  icon_view_->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, 12));
  icon_view_->SetImageSize(gfx::Size(20, 20));

  label_ = AddChildView(std::make_unique<views::Label>());
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetVerticalAlignment(gfx::ALIGN_MIDDLE);
  label_->SetMultiLine(false);

  chip_container_ = AddChildView(std::make_unique<ChipContainerView>(
      std::u16string(), std::nullopt,
      base::BindRepeating(&AnchoredMessageBubbleView::ChipCallback,
                          base::Unretained(this))));

  close_button_ =
      AddChildView(std::make_unique<views::ImageButton>(close_callback_));
  close_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kCloseRoundedIcon,
                                     ui::kColorIcon, 16));
  close_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_CLOSE));
  close_button_->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 8, 0, 0));

  UpdateContent(model);
}

void AnchoredMessageBubbleView::UpdateContent(
    const PageActionModelInterface& model) {
  icon_ = model.GetAnchoredMessageIcon();
  if (icon_) {
    icon_view_->SetImage(icon_.value());
    icon_view_->SetVisible(true);
  } else {
    icon_view_->SetVisible(false);
  }

  label_text_ = model.GetAnchoredMessageText();
  label_->SetText(label_text_);
  label_->SetVisible(!label_text_.empty());

  std::optional<ui::ImageModel> chip_icon =
      icon_ ? std::nullopt : std::optional<ui::ImageModel>(model.GetImage());
  bool show_chip = chip_icon || model.GetText().size() > 0;

  chip_container_->Update(model.GetText(), chip_icon);
  chip_container_->SetVisible(show_chip);

  show_close_button_ = model.GetAnchoredMessageCloseIcon();
  close_button_->SetVisible(show_close_button_);

  // Update margins dynamically to avoid excessive spacing when some components
  // are hidden.
  bool add_padding_before_chip =
      icon_view_->GetVisible() || label_->GetVisible();
  chip_container_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, add_padding_before_chip ? 16 : 0, 0, 0));

  bool add_padding_before_close_icon =
      add_padding_before_chip || chip_container_->GetVisible();
  close_button_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, add_padding_before_close_icon ? 8 : 0, 0, 0));

  OnThemeChanged();
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
  // Copy callbacks to locals before invoking: close_callback_ destroys the
  // bubble (and |this|), so member access after that is a use-after-free.
  auto chip_callback = chip_callback_;
  auto close_callback = close_callback_;
  close_callback.Run();
  chip_callback.Run();
}

BEGIN_METADATA(AnchoredMessageBubbleView)
END_METADATA

}  // namespace page_actions
