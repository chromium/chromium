// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_disclaimer_view.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "chrome/grit/component_extension_resources.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

constexpr char kWidgetName[] = "MagicBoostDisclaimerViewWidget";

// Paddings, sizes and insets.
constexpr int kImageWidth = 360;
constexpr int kContainerPadding = 32;
constexpr int kTextContainerBetweenChildSpacing = 16;
constexpr int kContainerBottomPadding = 28;
constexpr int kWidgetWidth = kImageWidth;
constexpr int kWidgetHeight = 600;
constexpr int kBetweenButtonsSpacing = 8;
constexpr int kButtonHeight = 32;
constexpr gfx::Insets kButtonContainerInsets =
    gfx::Insets::TLBR(0,
                      kContainerPadding,
                      kContainerBottomPadding,
                      kContainerPadding);
constexpr gfx::Insets kTextContainerInsets = gfx::Insets(kContainerPadding);
constexpr gfx::Size kImagePreferredSize(/*width=*/kImageWidth, /*height=*/216);

// Placeholder texts
// TODO(b/339528642): Replace with real strings.
const std::u16string title_text = u"Disclaimer title";
const std::u16string secondary_button_text = u"No thanks";
const std::u16string primary_button_text = u"Try it";
const std::u16string body_text =
    u"Body text that is multi-line which means it can span from one line to up "
    u"to three lines for this case.";

// Font lists
const gfx::FontList body_text_font_list =
    gfx::FontList({"Google Sans", "Roboto"},
                  gfx::Font::NORMAL,
                  /*font_size=*/14,
                  gfx::Font::Weight::NORMAL);
const gfx::FontList title_text_font_list =
    gfx::FontList({"Google Sans", "Roboto"},
                  gfx::Font::NORMAL,
                  /*font_size=*/18,
                  gfx::Font::Weight::MEDIUM);

}  // namespace

MagicBoostDisclaimerView::MagicBoostDisclaimerView()
    : chromeos::editor_menu::PreTargetHandlerView(
          chromeos::editor_menu::CardType::kMahiDefaultMenu) {
  views::Builder<views::View>(this)
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      .SetBackground(views::CreateThemedRoundedRectBackground(
          ui::kColorPrimaryBackground,
          views::LayoutProvider::Get()->GetCornerRadiusMetric(
              views::ShapeContextTokens::kMenuRadius)))
      .AddChildren(
          views::Builder<views::ImageView>()
              // TODO(b/339044721): The json file linked to this image id is a
              // placeholder. Update the image json to the actual image.
              .SetImage(
                  ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                      IDR_MAGIC_BOOST_DISCLAIMER_IMAGE))
              .SetPreferredSize(kImagePreferredSize),
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::LayoutOrientation::kVertical)
              .SetProperty(views::kBoxLayoutFlexKey,
                           views::BoxLayoutFlexSpecification())
              .SetBetweenChildSpacing(kTextContainerBetweenChildSpacing)
              .SetBorder(views::CreateEmptyBorder(kTextContainerInsets))
              .AddChildren(views::Builder<views::Label>()
                               .SetFontList(title_text_font_list)
                               .SetEnabledColorId(ui::kColorSysOnSurface)
                               .SetHorizontalAlignment(
                                   gfx::HorizontalAlignment::ALIGN_LEFT)
                               .SetText(title_text),
                           views::Builder<views::Label>()
                               .SetFontList(body_text_font_list)
                               .SetEnabledColorId(ui::kColorSysSurfaceVariant)
                               .SetHorizontalAlignment(
                                   gfx::HorizontalAlignment::ALIGN_LEFT)
                               .SetText(body_text)
                               .SetMultiLine(true),
                           views::Builder<views::Label>()
                               .SetFontList(body_text_font_list)
                               .SetEnabledColorId(ui::kColorSysSurfaceVariant)
                               .SetHorizontalAlignment(
                                   gfx::HorizontalAlignment::ALIGN_LEFT)
                               .SetText(body_text)
                               .SetMultiLine(true)),
          views::Builder<views::BoxLayoutView>()
              .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
              .SetBetweenChildSpacing(kBetweenButtonsSpacing)
              .SetPreferredSize(gfx::Size(kWidgetWidth, kButtonHeight))
              .SetBorder(views::CreateEmptyBorder(kButtonContainerInsets))
              .AddChildren(
                  views::Builder<views::MdTextButton>()
                      .SetText(secondary_button_text)
                      .SetAccessibleName(secondary_button_text)
                      .SetStyle(ui::ButtonStyle::kText)
                      .SetCallback(base::BindRepeating(
                          &MagicBoostDisclaimerView::OnDeclineButtonPressed,
                          weak_ptr_factory_.GetWeakPtr())),
                  views::Builder<views::MdTextButton>()
                      .CopyAddressTo(&accept_button_)
                      .SetText(primary_button_text)
                      .SetAccessibleName(primary_button_text)
                      .SetStyle(ui::ButtonStyle::kProminent)
                      .SetCallback(base::BindRepeating(
                          &MagicBoostDisclaimerView::OnAcceptButtonPressed,
                          weak_ptr_factory_.GetWeakPtr())))

              )
      .BuildChildren();
}

MagicBoostDisclaimerView::~MagicBoostDisclaimerView() = default;

// static
views::UniqueWidgetPtr MagicBoostDisclaimerView::CreateWidget() {
  views::Widget::InitParams params;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_elevation = 2;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.name = GetWidgetName();

  views::UniqueWidgetPtr widget =
      std::make_unique<views::Widget>(std::move(params));
  widget->SetContentsView(std::make_unique<MagicBoostDisclaimerView>());

  // Shows the widget in the middle of the screen.
  // TODO(b/339044721): Set the widget bounds based on different screen size.
  auto bounds = display::Screen::GetScreen()
                    ->GetPrimaryDisplay()
                    .work_area()
                    .CenterPoint();
  widget->SetBounds(gfx::Rect(bounds.x() - kWidgetWidth / 2,
                              bounds.y() - kWidgetHeight / 2, kWidgetWidth,
                              kWidgetHeight));

  return widget;
}

// static
const char* MagicBoostDisclaimerView::GetWidgetName() {
  return kWidgetName;
}

void MagicBoostDisclaimerView::RequestFocus() {
  views::View::RequestFocus();

  accept_button_->RequestFocus();
}

void MagicBoostDisclaimerView::OnAcceptButtonPressed() {
  // TODO(b/339044721): Implement accept action.
}

void MagicBoostDisclaimerView::OnDeclineButtonPressed() {
  // TODO(b/339044721): Implement decline action.
}

BEGIN_METADATA(MagicBoostDisclaimerView)
END_METADATA

}  // namespace chromeos
