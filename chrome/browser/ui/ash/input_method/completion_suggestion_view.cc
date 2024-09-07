// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/completion_suggestion_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/ash/input_method/colors.h"
#include "chrome/browser/ui/ash/input_method/completion_suggestion_label_view.h"
#include "chrome/browser/ui/ash/input_method/suggestion_details.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ui {
namespace ime {

namespace {

const int kAnnotationLabelChildSpacing = 4;
const int kArrowIconSize = 14;
const int kDownIconHorizontalPadding = 2;
const int kDownIconSize = 16;
const int kEnterKeyHorizontalPadding = 2;

std::unique_ptr<views::ImageView> CreateDownIcon() {
  auto icon = std::make_unique<views::ImageView>();
  icon->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      0, kDownIconHorizontalPadding, 0, kDownIconHorizontalPadding)));
  return icon;
}

std::unique_ptr<views::Label> CreateEnterLabel() {
  auto label = std::make_unique<views::Label>();
  label->SetEnabledColor(
      ResolveSemanticColor(cros_styles::ColorName::kTextColorSecondary));
  label->SetText(l10n_util::GetStringUTF16(IDS_SUGGESTION_ENTER_KEY));
  label->SetFontList(gfx::FontList({kFontStyle}, gfx::Font::NORMAL,
                                   kAnnotationFontSize,
                                   gfx::Font::Weight::MEDIUM));
  label->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::VH(0, kEnterKeyHorizontalPadding)));
  return label;
}

std::unique_ptr<views::Label> CreateTabLabel() {
  auto label = std::make_unique<views::Label>();
  label->SetEnabledColor(
      ResolveSemanticColor(cros_styles::ColorName::kTextColorPrimary));
  label->SetText(l10n_util::GetStringUTF16(IDS_SUGGESTION_TAB_KEY));
  label->SetFontList(gfx::FontList({kFontStyle}, gfx::Font::NORMAL,
                                   kAnnotationFontSize,
                                   gfx::Font::Weight::MEDIUM));
  return label;
}

std::unique_ptr<views::View> CreateKeyContainer() {
  auto container = std::make_unique<views::View>();
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  // TODO(crbug/1099044): Use color from ash_color_provider and move SetBorder
  // to OnThemeChanged
  const SkColor kKeyContainerBorderColor =
      SkColorSetA(SK_ColorBLACK, 0x24);  // 14%
  container->SetBorder(views::CreateRoundedRectBorder(
      kAnnotationBorderThickness, kAnnotationCornerRadius, gfx::Insets(),
      kKeyContainerBorderColor));
  return container;
}

}  // namespace

CompletionSuggestionView::CompletionSuggestionView(PressedCallback callback)
    : views::Button(std::move(callback)) {
  suggestion_label_ =
      AddChildView(std::make_unique<CompletionSuggestionLabelView>());
  suggestion_label_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::VH(kPadding / 2, 0)));

  annotation_container_ = AddChildView(CreateAnnotationContainer());
  down_and_enter_annotation_label_ =
      annotation_container_->AddChildView(CreateDownAndEnterAnnotationLabel());
  tab_annotation_label_ =
      annotation_container_->AddChildView(CreateTabAnnotationLabel());

  annotation_container_->SetVisible(false);
  down_and_enter_annotation_label_->SetVisible(false);
  tab_annotation_label_->SetVisible(false);

  SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
}

CompletionSuggestionView::~CompletionSuggestionView() = default;

std::unique_ptr<views::View>
CompletionSuggestionView::CreateAnnotationContainer() {
  auto label = std::make_unique<views::View>();
  label->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  // AnnotationLabel's ChildViews eat events simmilar to StyledLabel.
  // Explicitly sets can_process_events_within_subtree to false for
  // AnnotationLabel's hover to work correctly.
  label->SetCanProcessEventsWithinSubtree(false);
  return label;
}

std::unique_ptr<views::View>
CompletionSuggestionView::CreateDownAndEnterAnnotationLabel() {
  auto label = std::make_unique<views::View>();
  label->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, kAnnotationPaddingLeft, 0, 0)));
  label
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal))
      ->set_between_child_spacing(kAnnotationLabelChildSpacing);
  down_icon_ =
      label->AddChildView(CreateKeyContainer())->AddChildView(CreateDownIcon());
  arrow_icon_ = label->AddChildView(std::make_unique<views::ImageView>());
  label->AddChildView(CreateKeyContainer())->AddChildView(CreateEnterLabel());
  return label;
}

std::unique_ptr<views::View>
CompletionSuggestionView::CreateTabAnnotationLabel() {
  auto label = std::make_unique<views::View>();
  label->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, kAnnotationPaddingLeft, 0, 0)));
  label->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  label->AddChildView(CreateTabLabel());
  return label;
}

void CompletionSuggestionView::SetView(const SuggestionDetails& details) {
  SetSuggestionText(details.text, details.confirmed_length);
  suggestion_width_ =
      suggestion_label_
          ->GetPreferredSize(views::SizeBounds(suggestion_label_->width(), {}))
          .width();
  down_and_enter_annotation_label_->SetVisible(details.show_accept_annotation);
  tab_annotation_label_->SetVisible(details.show_quick_accept_annotation);
  annotation_container_->SetVisible(details.show_accept_annotation ||
                                    details.show_quick_accept_annotation);
}

void CompletionSuggestionView::SetSuggestionText(
    const std::u16string& text,
    const size_t confirmed_length) {
  suggestion_label_->SetPrefixAndPrediction(text.substr(0, confirmed_length),
                                            text.substr(confirmed_length));

  // Because this view is accessibility-focusable, it must have an accessible
  // name so that screen readers know what to speak/braille when it claims
  // focus. We can accomplish this by setting the labelled-by relationship so
  // that it points to `suggestion_label_`. That will cause this view's
  // accessible name to be the same as the label text.
  GetViewAccessibility().SetName(*suggestion_label_);
}

void CompletionSuggestionView::SetHighlighted(bool highlighted) {
  if (highlighted_ == highlighted) {
    return;
  }

  highlighted_ = highlighted;
  if (highlighted) {
    NotifyAccessibilityEvent(ax::mojom::Event::kSelection, false);
    // TODO(crbug/1099044): Use System Color for button highlight.
    SetBackground(views::CreateSolidBackground(
        ResolveSemanticColor(kButtonHighlightColor)));
  } else {
    SetBackground(nullptr);
  }
  SchedulePaint();
}

void CompletionSuggestionView::OnThemeChanged() {
  down_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      kKeyboardArrowDownIcon, ui::kColorIcon, kDownIconSize));
  arrow_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      kKeyboardArrowRightIcon, ui::kColorIcon, kArrowIconSize));
  views::View::OnThemeChanged();
}

void CompletionSuggestionView::Layout(PassKey) {
  int left = kPadding;

  suggestion_label_->SetBounds(left, 0, suggestion_width_, height());

  if (annotation_container_->GetVisible()) {
    int annotation_left = left + suggestion_width_;
    int container_right = bounds().right();
    int annotation_width = container_right - annotation_left - kPadding;
    annotation_container_->SetBounds(annotation_left, kAnnotationPaddingTop,
                                     annotation_width,
                                     kAnnotationPaddingBottom);
  }
}

gfx::Size CompletionSuggestionView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size;
  gfx::Size suggestion_size = suggestion_label_->GetPreferredSize(
      views::SizeBounds(suggestion_width_, {}));
  suggestion_size.SetToMax(gfx::Size(suggestion_width_, 0));
  size.Enlarge(suggestion_size.width() + 2 * kPadding, 0);
  size.SetToMax(suggestion_size);
  if (annotation_container_->GetVisible()) {
    views::SizeBound available_width =
        std::max<views::SizeBound>(0, available_size.width() - size.width());
    gfx::Size annotation_size = annotation_container_->GetPreferredSize(
        views::SizeBounds(available_width, {}));
    size.Enlarge(annotation_size.width(), 0);
  }
  if (min_width_ > size.width()) {
    size.Enlarge(min_width_ - size.width(), 0);
  }
  return size;
}

void CompletionSuggestionView::SetMinWidth(int min_width) {
  min_width_ = min_width;
}

gfx::Point CompletionSuggestionView::GetAnchorOrigin() const {
  return gfx::Point(suggestion_label_->GetPrefixWidthPx() + kPadding, 0);
}

std::u16string CompletionSuggestionView::GetSuggestionForTesting() {
  return suggestion_label_->GetText();
}

CompletionSuggestionLabelView*
CompletionSuggestionView::suggestion_label_for_testing() const {
  return suggestion_label_;
}

BEGIN_METADATA(CompletionSuggestionView)
END_METADATA

}  // namespace ime
}  // namespace ui
