// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/default_browser/annotated_setter_bubble.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "build/branding_buildflags.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/range/range.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"

namespace default_browser {

namespace {

constexpr int kTitleIconSizeDip = 20;

ui::ImageModel TitleIcon() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const gfx::VectorIcon& icon = vector_icons::kGoogleGLogoIcon;
#else
  const gfx::VectorIcon& icon = vector_icons::kProductIcon;
#endif

  return ui::ImageModel::FromVectorIcon(icon, gfx::kPlaceholderColor,
                                        kTitleIconSizeDip);
}

// Removes the <strong> tags the WebUI strings carry and returns the ranges
// they enclosed.
std::vector<gfx::Range> StripStrongTags(std::u16string& text) {
  static constexpr std::u16string_view kOpen = u"<strong>";
  static constexpr std::u16string_view kClose = u"</strong>";
  std::vector<gfx::Range> ranges;
  for (size_t open = text.find(kOpen); open != std::u16string::npos;
       open = text.find(kOpen, open)) {
    text.erase(open, kOpen.size());
    const size_t close = text.find(kClose, open);

    if (close == std::u16string::npos) {
      break;
    }

    text.erase(close, kClose.size());
    ranges.emplace_back(open, close);
  }
  return ranges;
}

std::unique_ptr<views::StyledLabel> MakeParagraph(int message_id) {
  std::u16string text = l10n_util::GetStringUTF16(message_id);
  const std::vector<gfx::Range> emphasized = StripStrongTags(text);
  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(text);
  label->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  for (const gfx::Range& range : emphasized) {
    views::StyledLabel::RangeStyleInfo style;
    style.text_style = views::style::STYLE_EMPHASIZED;
    label->AddStyleRange(range, style);
  }

  return label;
}

}  // namespace

AnnotatedSetterBubble::AnnotatedSetterBubble(bool can_pin_to_taskbar)
    : views::BubbleDialogDelegate(views::BubbleAnchor(),
                                  views::BubbleBorder::TOP_RIGHT) {
  // The anchor belongs to another process.
  set_has_parent(false);
  SetTitle(l10n_util::GetStringUTF16(
      IDS_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_HEADER_TITLE));
  SetIcon(TitleIcon());
  SetShowIcon(true);
  SetShowCloseButton(true);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  // Focus stays with the Settings window.
  set_close_on_deactivate(false);
  set_margins(gfx::Insets::TLBR(12, 16, 16, 16));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  auto* contents = SetContentsView(std::make_unique<views::BoxLayoutView>());
  contents->SetOrientation(views::BoxLayout::Orientation::kVertical);
  contents->SetBetweenChildSpacing(8);
  contents->AddChildView(MakeParagraph(
      IDS_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_CONTENT_DESCRIPTION));
  contents->AddChildView(MakeParagraph(
      IDS_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_INSTRUCTION_STEP_1));
  if (can_pin_to_taskbar) {
    contents->AddChildView(MakeParagraph(
        IDS_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_INSTRUCTION_STEP_2));
  }
}

AnnotatedSetterBubble::~AnnotatedSetterBubble() = default;

std::unique_ptr<views::Widget> AnnotatedSetterBubble::Show(
    const gfx::Rect& button_rect_dip,
    views::Widget::ClosedCallback on_closed) {
  SetAnchorRect(button_rect_dip);
  std::unique_ptr<views::Widget> widget =
      views::BubbleDialogDelegate::CreateBubble(this, std::move(on_closed));
  // The frame hides the arrow by default.
  GetBubbleFrameView()->SetDisplayVisibleArrow(true);
  SizeToContents();
  widget->ShowInactive();
  return widget;
}

void AnnotatedSetterBubble::OnWidgetInitialized() {
  views::BubbleDialogDelegate::OnWidgetInitialized();
  // Stay above the Settings window.
  GetWidget()->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
}

}  // namespace default_browser
