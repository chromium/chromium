// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fullscreen_control/subtle_notification_view.h"

#include <memory>

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"
namespace {

// Space between the site info label.
const int kMiddlePaddingPx = 30;

const int kOuterPaddingHorizPx = 40;
const int kOuterPaddingVertPx = 8;

// Partially-transparent background color.
const SkColor kSubtleNotificationBackgroundColor =
    SkColorSetARGB(0xcc, 0x28, 0x2c, 0x32);

// Spacing around the key name.
const int kKeyNameMarginHorizPx = 7;
const int kKeyNameBorderPx = 1;
const int kKeyNameCornerRadius = 2;
const int kKeyNamePaddingPx = 5;

// Spacing between the key name and image, if any.
const int kKeyNameImageSpacingPx = 3;

// The context used to obtain typography for the instruction text. It's not
// really a dialog, but a dialog title is a good fit.
constexpr int kInstructionTextContext = views::style::CONTEXT_DIALOG_TITLE;

// Delimiter indicating there should be a segment displayed as a keyboard key.
constexpr char16_t kKeyNameDelimiter[] = u"|";

}  // namespace

// Class containing the instruction text. Contains fancy styling on the keyboard
// key (not just a simple label).
class SubtleNotificationView::InstructionView : public views::View {
  METADATA_HEADER(InstructionView, views::View)

 public:
  // Creates an InstructionView with specific text. |text| may contain one or
  // more segments delimited by a pair of pipes ('|'); each of these segments
  // will be displayed as a keyboard key. e.g., "Press |Alt|+|Q| to exit" will
  // have "Alt" and "Q" rendered as keys.
  explicit InstructionView(const std::u16string& text);
  InstructionView(const InstructionView&) = delete;
  InstructionView& operator=(const InstructionView&) = delete;

  std::u16string GetText() const;
  void SetText(const std::u16string& text);
  void SetTextAndImages(const std::u16string& text,
                        std::vector<std::unique_ptr<views::View>> key_images);

  base::CallbackListSubscription AddTextChangedCallback(
      views::PropertyChangedCallback callback);

 private:
  // Adds a label to the end of the notification text. If |format_as_key|,
  // surrounds the label in a rounded-rect border to indicate that it is a
  // keyboard key.
  void AddTextSegment(const std::u16string& text,
                      bool format_as_key,
                      std::unique_ptr<views::View> key_image);

  std::u16string text_;
};

SubtleNotificationView::InstructionView::InstructionView(
    const std::u16string& text) {
  // The |between_child_spacing| is the horizontal margin of the key name.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kKeyNameMarginHorizPx));

  SetText(text);
}

std::u16string SubtleNotificationView::InstructionView::GetText() const {
  return text_;
}

void SubtleNotificationView::InstructionView::SetText(
    const std::u16string& text) {
  SetTextAndImages(text, std::vector<std::unique_ptr<views::View>>());
}

void SubtleNotificationView::InstructionView::SetTextAndImages(
    const std::u16string& text,
    std::vector<std::unique_ptr<views::View>> key_images) {
  // Avoid replacing the contents with the same text.
  if (text == text_ && key_images.empty())
    return;

  RemoveAllChildViews();

  // Parse |text|, looking for pipe-delimited segment.
  std::vector<std::u16string> segments = base::SplitString(
      text, kKeyNameDelimiter, base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  // SplitString() returns empty strings for zero-length segments, so given an
  // even number of pipes, there should always be an odd number of segments.
  // The exception is if |text| is entirely empty, in which case the returned
  // list is also empty (rather than containing a single empty string).
  DCHECK(segments.empty() || segments.size() % 2 == 1);

  // Every second segment is formatted as a key, so should have an image
  // specified for it (even if that image is an empty unique_ptr).
  // Or, we can have no key images at all.
  //
  // There should always be one non-key segment preceding each key segment
  // (even if empty, as above) and one segment at the end, so the total number
  // of segments should be double the number of key segments, plus one.
  DCHECK(key_images.empty() || key_images.size() * 2 + 1 == segments.size());

  // Add text segment, alternating between non-key (no border) and key (border)
  // formatting.
  bool format_as_key = false;
  int idx = 0;
  for (const auto& segment : segments) {
    std::unique_ptr<views::View> key_image;
    if (!key_images.empty() && format_as_key) {
      key_image = std::move(key_images[idx]);
      idx++;
    }
    AddTextSegment(segment, format_as_key, std::move(key_image));
    format_as_key = !format_as_key;
  }

  text_ = text;
  OnPropertyChanged(&text_, views::kPropertyEffectsPaint);
}

void SubtleNotificationView::InstructionView::AddTextSegment(
    const std::u16string& text,
    bool format_as_key,
    std::unique_ptr<views::View> key_image) {
  constexpr SkColor kForegroundColor = SK_ColorWHITE;

  views::Label* label = new views::Label(text, kInstructionTextContext);
  label->SetEnabledColor(kForegroundColor);
  label->SetBackgroundColor(kSubtleNotificationBackgroundColor);

  if (!format_as_key) {
    DCHECK(!key_image);
    AddChildView(label);
    return;
  }

  views::View* key = new views::View;
  auto key_name_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(0, kKeyNamePaddingPx), kKeyNameImageSpacingPx);
  key_name_layout->set_minimum_cross_axis_size(
      label->GetPreferredSize({}).height() + kKeyNamePaddingPx * 2);
  key->SetLayoutManager(std::move(key_name_layout));
  if (key_image)
    key->AddChildView(std::move(key_image));
  key->AddChildView(label);
  // The key name has a border around it.
  std::unique_ptr<views::Border> border(views::CreateRoundedRectBorder(
      kKeyNameBorderPx, kKeyNameCornerRadius, kForegroundColor));
  key->SetBorder(std::move(border));
  AddChildView(key);
}

base::CallbackListSubscription
SubtleNotificationView::InstructionView::AddTextChangedCallback(
    views::PropertyChangedCallback callback) {
  return AddPropertyChangedCallback(&text_, std::move(callback));
}

BEGIN_METADATA(SubtleNotificationView, InstructionView)
ADD_PROPERTY_METADATA(std::u16string, Text)
END_METADATA

SubtleNotificationView::SubtleNotificationView() : instruction_view_(nullptr) {
  auto bubble_border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::NO_SHADOW);
  bubble_border->SetColor(kSubtleNotificationBackgroundColor);
  SetBackground(std::make_unique<views::BubbleBackground>(bubble_border.get()));
  SetBorder(std::move(bubble_border));

  instruction_view_ = new InstructionView(std::u16string());

  int outer_padding_horiz = kOuterPaddingHorizPx;
  int outer_padding_vert = kOuterPaddingVertPx;
  AddChildView(instruction_view_.get());

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(outer_padding_vert, outer_padding_horiz),
      kMiddlePaddingPx));

  GetViewAccessibility().SetRole(ax::mojom::Role::kAlert);
  UpdateAccessibleName();
  text_changed_callback_ = instruction_view_->AddTextChangedCallback(
      base::BindRepeating(&SubtleNotificationView::OnInstructionViewTextChanged,
                          base::Unretained(this)));
}

SubtleNotificationView::~SubtleNotificationView() {}

void SubtleNotificationView::UpdateContent(
    const std::u16string& instruction_text) {
  instruction_view_->SetText(instruction_text);
  instruction_view_->SetVisible(!instruction_text.empty());
  DeprecatedLayoutImmediately();
}

void SubtleNotificationView::UpdateContent(
    const std::u16string& instruction_text,
    std::vector<std::unique_ptr<views::View>> key_images) {
  instruction_view_->SetTextAndImages(instruction_text, std::move(key_images));
  instruction_view_->SetVisible(!instruction_text.empty());
  DeprecatedLayoutImmediately();
}

// static
views::Widget* SubtleNotificationView::CreatePopupWidget(
    gfx::NativeView parent_view,
    std::unique_ptr<SubtleNotificationView> view) {
  // Initialize the popup.
  views::Widget* popup = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
#if BUILDFLAG(IS_WIN)
  // On Windows, this widget isn't parented on purpose to avoid it being
  // obscured by other topmost widgets. See crbug.com/1431043. Setting
  // `parent_view` as the context instead of the parent to meet Aura's
  // requirement for widgets to have either a parent_view or a context.
  // TODO(crbug.com/40066609): Aura should respect the fine-grained levels of
  // topmost windows defined in ZOrderLevel.
  params.context = parent_view;
#else
  params.parent = parent_view;
#endif

  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.z_order = ui::ZOrderLevel::kSecuritySurface;
  params.accept_events = false;
  popup->Init(std::move(params));
  popup->SetContentsView(std::move(view));
  // We set layout manager to nullptr to prevent the widget from sizing its
  // contents to the same size as itself. This prevents the widget contents from
  // shrinking while we animate the height of the popup to give the impression
  // that it is sliding off the top of the screen.
  // TODO(mgiuca): This probably isn't necessary now that there is no slide
  // animation. Remove it.
  popup->GetRootView()->SetLayoutManager(nullptr);

  return popup;
}

void SubtleNotificationView::OnInstructionViewTextChanged() {
  UpdateAccessibleName();
}

void SubtleNotificationView::UpdateAccessibleName() {
  std::u16string accessible_name;
  base::RemoveChars(instruction_view_->GetText(), kKeyNameDelimiter,
                    &accessible_name);
  GetViewAccessibility().SetName(accessible_name);
}

std::u16string SubtleNotificationView::GetInstructionTextForTest() const {
  return instruction_view_->GetText();
}

BEGIN_METADATA(SubtleNotificationView)
END_METADATA
