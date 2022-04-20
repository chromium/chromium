// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/user_note/user_note_view.h"

#include "base/i18n/time_formatting.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"

namespace {

// Layout structure:
//
// [note_text     ]  <--- user note body.
std::unique_ptr<views::Label> CreateBodyLabel(const std::string note_text) {
  auto user_note_body =
      std::make_unique<views::Label>(base::UTF8ToUTF16(note_text), 16);
  user_note_body->SetTextStyle(views::style::STYLE_PRIMARY);
  user_note_body->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  user_note_body->SetMultiLine(true);
  user_note_body->SetElideBehavior(gfx::NO_ELIDE);
  const views::FlexSpecification text_flex(
      views::LayoutOrientation::kVertical,
      views::MinimumFlexSizeRule::kPreferred,
      views::MaximumFlexSizeRule::kPreferred, true,
      views::MinimumFlexSizeRule::kScaleToMinimum);
  user_note_body->SetProperty(views::kFlexBehaviorKey, text_flex);

  return user_note_body;
}

// Layout structure:
//
// [date         :]  <--- user note header, which contains a date and
// a 3-dot menu button.
std::unique_ptr<views::View> CreateHeaderView(std::u16string note_date,
                                              base::RepeatingClosure callback) {
  auto user_note_header = std::make_unique<views::View>();

  // User note date
  views::Label* user_note_date = user_note_header->AddChildView(
      std::make_unique<views::Label>(note_date, 10));
  user_note_date->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  user_note_date->SetMultiLine(false);
  user_note_date->SetTextStyle(views::style::STYLE_HINT);
  // User note date layout
  auto& user_note_header_layout =
      user_note_header->SetLayoutManager(std::make_unique<views::FlexLayout>())
          ->SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  user_note_header->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(user_note_header_layout.GetDefaultFlexRule()));

  // Menu button
  views::ImageButton* menu_button = user_note_header->AddChildView(
      views::CreateVectorImageButtonWithNativeTheme(
          callback, kBrowserToolsIcon,
          ChromeLayoutProvider::Get()->GetDistanceMetric(
              ChromeDistanceMetric::
                  DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE)));
  menu_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  menu_button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  views::InstallCircleHighlightPathGenerator(menu_button);
  // Button layout.
  menu_button->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithAlignment(views::LayoutAlignment::kEnd));

  return user_note_header;
}

// Layout structure:
//
// [quote     ]  <--- user note quote.
std::unique_ptr<views::View> CreateTargetTextView(
    const std::string note_quote) {
  auto user_note_quote_container = std::make_unique<views::View>();

  // User note quote
  const int user_note_quote_max_lines = 2;
  const int r_color = 233;
  const int g_color = 210;
  const int b_color = 253;
  user_note_quote_container->SetBackground(
      views::CreateSolidBackground(SkColorSetRGB(r_color, g_color, b_color)));
  auto* user_note_quote = user_note_quote_container->AddChildView(
      std::make_unique<views::Label>(base::UTF8ToUTF16(note_quote), 16));
  user_note_quote->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  user_note_quote->SetMultiLine(true);
  user_note_quote->SetMaxLines(user_note_quote_max_lines);
  user_note_quote->SetElideBehavior(gfx::ELIDE_TAIL);
  // User note quote layout
  const views::FlexSpecification text_flex(
      views::LayoutOrientation::kVertical,
      views::MinimumFlexSizeRule::kPreferred,
      views::MaximumFlexSizeRule::kPreferred, true,
      views::MinimumFlexSizeRule::kScaleToMinimum);
  user_note_quote->SetProperty(views::kFlexBehaviorKey, text_flex);
  auto& user_note_quote_layout =
      user_note_quote_container
          ->SetLayoutManager(std::make_unique<views::FlexLayout>())
          ->SetOrientation(views::LayoutOrientation::kVertical)
          .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
          .SetIgnoreDefaultMainAxisMargins(true);

  user_note_quote_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(user_note_quote_layout.GetDefaultFlexRule()));

  return user_note_quote_container;
}

// Layout structure:
//
// [    cancel add]  <--- button container
std::unique_ptr<views::View> CreateButtonsView(
    base::RepeatingClosure cancel_callback,
    base::RepeatingClosure add_callback) {
  auto button_container = std::make_unique<views::View>();

  // Cancel button
  gfx::Insets button_padding = gfx::Insets::VH(6, 16);
  auto cancel_button = std::make_unique<views::MdTextButton>(
      cancel_callback, l10n_util::GetStringUTF16(IDS_CANCEL));
  cancel_button->SetCustomPadding(button_padding);
  cancel_button->SetEnabledTextColors(SK_ColorBLUE);
  button_container->AddChildView(std::move(cancel_button));

  // Add button
  auto add_button = std::make_unique<views::MdTextButton>(
      add_callback, l10n_util::GetStringUTF16(IDS_ADD));
  add_button->SetCustomPadding(button_padding);
  add_button->SetProminent(true);
  button_container->AddChildView(std::move(add_button));

  // Button container layout
  ChromeLayoutProvider* const chrome_layout_provider =
      ChromeLayoutProvider::Get();
  auto& button_layout =
      button_container->SetLayoutManager(std::make_unique<views::FlexLayout>())
          ->SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
          .SetDefault(
              views::kMarginsKey,
              gfx::Insets::TLBR(
                  chrome_layout_provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
                  chrome_layout_provider->GetDistanceMetric(
                      views::DISTANCE_RELATED_BUTTON_HORIZONTAL),
                  0, 0))
          .SetIgnoreDefaultMainAxisMargins(true);
  button_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(button_layout.GetDefaultFlexRule()));

  return button_container;
}

std::unique_ptr<views::Textarea> CreateTextarea() {
  auto text_area = std::make_unique<views::Textarea>();

  // Textarea container view
  int default_textarea_width = 60;
  int minimum_textarea_width = 30;
  text_area->SetDefaultWidthInChars(default_textarea_width);
  text_area->SetMinimumWidthInChars(minimum_textarea_width);
  text_area->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_ADD_NEW_USER_NOTE_PLACEHOLDER_TEXT));
  text_area->SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT);
  text_area->SetBackgroundColor(SK_ColorTRANSPARENT);
  text_area->SetBorder(views::NullBorder());
  // Textarea container layout
  text_area->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kVertical,
                               views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred, true,
                               views::MinimumFlexSizeRule::kScaleToMinimum));

  return text_area;
}

std::u16string ConvertDate(base::Time time) {
  std::u16string formated_time = base::TimeFormatWithPattern(time, "MMMMdjmm");
  return formated_time;
}

}  // namespace

UserNoteView::UserNoteView(user_notes::UserNoteInstance* user_note_instance,
                           UserNoteView::State state)
    : user_note_instance_(user_note_instance) {
  // Creates an empty view if the user note instance or or user note model is
  // null.
  if (user_note_instance_ == nullptr)
    return;

  DCHECK(state != UserNoteView::State::kEditing);
  const gfx::Insets new_user_note_contents_insets = gfx::Insets::VH(16, 20);
  const int corner_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
  const int rounded_border_thickness = 1;
  const SkColor border_color = UserNoteView::State::kCreating == state ||
                                       UserNoteView::State::kEditing == state
                                   ? SK_ColorBLUE
                                   : SK_ColorLTGRAY;

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(new_user_note_contents_insets);
  SetBorder(views::CreateRoundedRectBorder(rounded_border_thickness,
                                           corner_radius, border_color));

  switch (state) {
    case UserNoteView::State::kDefault:
      user_note_header_ = AddChildView(CreateHeaderView(
          ConvertDate(
              user_note_instance_->model().metadata().modification_date()),
          base::BindRepeating(&UserNoteView::OnOpenMenu,
                              base::Unretained(this))));
      user_note_body_ = AddChildView(CreateBodyLabel(
          user_note_instance_->model().body().plain_text_value()));
      break;
    case UserNoteView::State::kCreating:
      text_area_ = AddChildView(CreateTextarea());
      button_container_ = AddChildView(CreateButtonsView(
          base::BindRepeating(&UserNoteView::OnCancelNewUserNote,
                              base::Unretained(this)),
          base::BindRepeating(&UserNoteView::OnAddUserNote,
                              base::Unretained(this))));
      break;
    case UserNoteView::State::kOrphaned:
      user_note_header_ = AddChildView(CreateHeaderView(
          ConvertDate(
              user_note_instance_->model().metadata().modification_date()),
          base::BindRepeating(&UserNoteView::OnOpenMenu,
                              base::Unretained(this))));
      user_note_quote_ = AddChildView(CreateTargetTextView(
          user_note_instance_->model().target().original_text()));
      user_note_body_ = AddChildView(CreateBodyLabel(
          user_note_instance_->model().body().plain_text_value()));
      break;
    default:
      SetBorder(nullptr);
      SetLayoutManager(nullptr);
      break;
  }
}

UserNoteView::~UserNoteView() = default;

void UserNoteView::OnOpenMenu() {
  // TODO(cheickcisse): Implement context menu that will allow users to remove,
  // edit and learn more about the user note.
}

void UserNoteView::OnCancelNewUserNote() {
  // TODO(cheickcisse): Cancel adding a new note.
}

void UserNoteView::OnAddUserNote() {
  // TODO(cheickcisse): Add a new note.
}
