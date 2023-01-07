// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/user_note/user_note_view.h"

#include <string>

#include "base/i18n/time_formatting.h"
#include "base/unguessable_token.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_notes/browser/user_note_instance.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_menu_model_adapter.h"
#include "ui/base/models/image_model.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_utils.h"

namespace {

constexpr int kUserNoteDateViewId = 180;
constexpr int kUserNoteMenuButtonViewId = 181;
constexpr int kUserNoteQuoteViewId = 182;

// Layout structure:
//
// [note_text     ]  <--- user note body.
std::unique_ptr<views::Label> CreateBodyLabel(const std::u16string note_text) {
  auto user_note_body = std::make_unique<views::Label>(note_text);
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
  views::Label* user_note_date =
      user_note_header->AddChildView(std::make_unique<views::Label>(note_date));
  user_note_date->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  user_note_date->SetMultiLine(false);
  user_note_date->SetTextStyle(views::style::STYLE_HINT);
  user_note_date->SetID(kUserNoteDateViewId);
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
  menu_button->SetID(kUserNoteMenuButtonViewId);
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
    const std::u16string note_quote) {
  auto user_note_quote_container = std::make_unique<views::View>();

  // User note quote
  const int user_note_quote_max_lines = 2;
  const int r_color = 233;
  const int g_color = 210;
  const int b_color = 253;
  user_note_quote_container->SetBackground(
      views::CreateSolidBackground(SkColorSetRGB(r_color, g_color, b_color)));
  auto* user_note_quote = user_note_quote_container->AddChildView(
      std::make_unique<views::Label>(note_quote));
  user_note_quote->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  user_note_quote->SetMultiLine(true);
  user_note_quote->SetMaxLines(user_note_quote_max_lines);
  user_note_quote->SetElideBehavior(gfx::ELIDE_TAIL);
  user_note_quote->SetID(kUserNoteQuoteViewId);
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
// [    cancel (add or save)]  <--- button container
std::unique_ptr<views::View> CreateButtonsView(
    base::RepeatingClosure cancel_callback,
    base::RepeatingClosure action_callback,
    UserNoteView::State state) {
  auto button_container = std::make_unique<views::View>();

  // Cancel button
  gfx::Insets button_padding = gfx::Insets::VH(6, 16);
  auto cancel_button = std::make_unique<views::MdTextButton>(
      cancel_callback, l10n_util::GetStringUTF16(IDS_CANCEL));
  cancel_button->SetCustomPadding(button_padding);
  cancel_button->SetEnabledTextColors(SK_ColorBLUE);
  button_container->AddChildView(std::move(cancel_button));

  // Action button
  auto action_button = std::make_unique<views::MdTextButton>(
      action_callback,
      l10n_util::GetStringUTF16(
          state == UserNoteView::State::kCreating ? IDS_ADD : IDS_SAVE));
  action_button->SetCustomPadding(button_padding);
  action_button->SetProminent(true);
  button_container->AddChildView(std::move(action_button));

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

UserNoteView::UserNoteView(UserNoteUICoordinator* coordinator,
                           user_notes::UserNoteInstance* user_note_instance,
                           UserNoteView::State state)
    : user_note_instance_(user_note_instance),
      coordinator_(coordinator),
      id_(user_note_instance->model().id()),
      rect_(user_note_instance->rect()) {
  // Creates an empty view if the user note instance or or user note model is
  // null.
  if (user_note_instance_ == nullptr)
    return;

  DCHECK(state != UserNoteView::State::kEditing);
  const gfx::Insets new_user_note_contents_insets = gfx::Insets::VH(16, 20);
  const int corner_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
  const int rounded_border_thickness = 1;
  const SkColor border_color =
      UserNoteView::State::kCreating == state ? SK_ColorBLUE : SK_ColorLTGRAY;

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(new_user_note_contents_insets);
  SetBorder(views::CreateRoundedRectBorder(rounded_border_thickness,
                                           corner_radius, border_color));

  CreateOrUpdateNoteView(
      state, user_note_instance_->model().metadata().modification_date(),
      user_note_instance_->model().body().plain_text_value(),
      user_note_instance_->model().target().original_text());
}

UserNoteView::~UserNoteView() = default;

void UserNoteView::SetDefaultOrDetachedState(base::Time date,
                                             const std::u16string content,
                                             const std::u16string quote) {
  if (user_note_header_) {
    user_note_header_->SetVisible(true);
    views::Label* date_view = views::AsViewClass<views::Label>(
        user_note_header_->GetViewByID(kUserNoteDateViewId));
    date_view->SetText(ConvertDate(date));
  } else {
    user_note_header_ = AddChildView(CreateHeaderView(
        ConvertDate(date), base::BindRepeating(&UserNoteView::OnOpenMenu,
                                               base::Unretained(this))));
  }

  if (!quote.empty()) {
    if (user_note_quote_) {
      user_note_quote_->SetVisible(true);
    } else {
      user_note_quote_ = AddChildView(CreateTargetTextView(quote));
    }
  }

  if (user_note_body_) {
    user_note_body_->SetVisible(true);
    user_note_body_->SetText(content);
  } else {
    user_note_body_ = AddChildView(CreateBodyLabel(content));
  }
}

void UserNoteView::SetCreatingOrEditState(const std::u16string content,
                                          UserNoteView::State state) {
  if (text_area_) {
    text_area_->SetVisible(true);
  } else {
    text_area_ = AddChildView(CreateTextarea());
  }

  if (!content.empty())
    text_area_->SetText(content);

  if (button_container_) {
    button_container_->SetVisible(true);
  } else {
    button_container_ = AddChildView(CreateButtonsView(
        base::BindRepeating(&UserNoteView::OnCancelUserNote,
                            base::Unretained(this), state),
        base::BindRepeating(state == UserNoteView::State::kCreating
                                ? &UserNoteView::OnAddUserNote
                                : &UserNoteView::OnSaveUserNote,
                            base::Unretained(this)),
        state));
  }
}

void UserNoteView::CreateOrUpdateNoteView(UserNoteView::State state,
                                          base::Time date,
                                          const std::u16string content,
                                          const std::u16string quote) {
  switch (state) {
    case UserNoteView::State::kDefault:
      SetDefaultOrDetachedState(date, content, /*quote =*/std::u16string());
      break;
    case UserNoteView::State::kCreating:
      SetCreatingOrEditState(/*content =*/std::u16string(), state);
      break;
    case UserNoteView::State::kDetached:
      SetDefaultOrDetachedState(date, content, quote);
      break;
    case UserNoteView::State::kEditing:
      SetCreatingOrEditState(content, state);
      break;
    default:
      SetBorder(nullptr);
      SetLayoutManager(nullptr);
      break;
  }
}

void UserNoteView::OnOpenMenu() {
  auto* user_note_menu_button =
      user_note_header_->GetViewByID(kUserNoteMenuButtonViewId);

  dialog_model_ = std::make_unique<ui::DialogModelMenuModelAdapter>(
      ui::DialogModel::Builder()
          .AddMenuItem(ui::ImageModel(), l10n_util::GetStringUTF16(IDS_EDIT),
                       base::BindRepeating(&UserNoteView::OnEditUserNote,
                                           base::Unretained(this)))
          .AddMenuItem(ui::ImageModel(), l10n_util::GetStringUTF16(IDS_DELETE),
                       base::BindRepeating(&UserNoteView::OnDeleteUserNote,
                                           base::Unretained(this)))
          .AddMenuItem(ui::ImageModel(),
                       l10n_util::GetStringUTF16(IDS_LEARN_MORE_USER_NOTE),
                       base::BindRepeating(&UserNoteView::OnLearnUserNote,
                                           base::Unretained(this)))
          .Build());

  menu_runner_ = std::make_unique<views::MenuRunner>(
      dialog_model_.get(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU,
      base::BindRepeating(&UserNoteView::OnMenuClosed, base::Unretained(this)));
  menu_runner_->RunMenuAt(user_note_menu_button->GetWidget(), nullptr,
                          user_note_menu_button->GetBoundsInScreen(),
                          views::MenuAnchorPosition::kTopLeft,
                          ui::MenuSourceType::MENU_SOURCE_MOUSE);
}

void UserNoteView::OnMenuClosed() {
  menu_runner_.reset();
  dialog_model_ = nullptr;
}

void UserNoteView::OnCancelUserNote(UserNoteView::State state) {
  if (state == UserNoteView::State::kCreating) {
    coordinator_->OnNoteCreationCancelled(user_note_id(), this);
    return;
  }

  // Hide editing state
  GetBorder()->set_color(SK_ColorLTGRAY);
  text_area_->SetVisible(false);
  button_container_->SetVisible(false);

  // Show default/detached state
  user_note_body_->SetVisible(false);
  if (user_note_quote_)
    user_note_quote_->SetVisible(false);
  user_note_header_->SetVisible(false);
}

void UserNoteView::OnAddUserNote() {
  const std::u16string note_content = text_area_->GetText();
  base::Time date = base::Time::Now();

  // Hide creating state
  GetBorder()->set_color(SK_ColorLTGRAY);
  text_area_->SetVisible(false);
  button_container_->SetVisible(false);

  // Show default state
  CreateOrUpdateNoteView(UserNoteView::State::kDefault, date, note_content,
                         /*quote =*/std::u16string());

  coordinator_->OnNoteCreationDone(user_note_id(), note_content);
}

bool UserNoteView::OnMousePressed(const ui::MouseEvent& event) {
  coordinator_->OnNoteSelected(user_note_id());
  return true;
}

void UserNoteView::OnEditUserNote(int event_flags) {
  const std::u16string note_content = user_note_body_->GetText();

  // Hide default/detached state
  GetBorder()->set_color(SK_ColorBLUE);
  user_note_body_->SetVisible(false);
  user_note_header_->SetVisible(false);
  if (user_note_quote_) {
    user_note_quote_->SetVisible(false);

    // Show editing state
    CreateOrUpdateNoteView(UserNoteView::State::kEditing, base::Time(),
                           note_content,
                           /*quote =*/std::u16string());
  }
}

void UserNoteView::OnDeleteUserNote(int event_flags) {
  coordinator_->OnNoteDeleted(user_note_id(), this);
}

void UserNoteView::OnLearnUserNote(int event_flags) {
  // TODO(cheickcisse): Learn more about user note.
}

void UserNoteView::OnSaveUserNote() {
  const std::u16string note_content = text_area_->GetText();
  const std::u16string note_quote =
      user_note_quote_
          ? views::AsViewClass<views::Label>(
                user_note_quote_->GetViewByID(kUserNoteQuoteViewId))
                ->GetText()
          : std::u16string();
  base::Time date = base::Time::Now();

  // Hide editing state
  GetBorder()->set_color(SK_ColorLTGRAY);
  text_area_->SetVisible(false);
  button_container_->SetVisible(false);

  // Show default state
  CreateOrUpdateNoteView(user_note_quote_ ? UserNoteView::State::kDetached
                                          : UserNoteView::State::kDefault,
                         date, note_content, note_quote);

  coordinator_->OnNoteUpdated(user_note_id(), note_content);
}
