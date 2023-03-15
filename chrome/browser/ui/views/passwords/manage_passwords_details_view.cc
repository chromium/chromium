// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_details_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_view_ids.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/common/password_manager_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/vector_icons.h"

namespace {

using password_manager::ManagePasswordsViewIDs;
using password_manager::metrics_util::PasswordManagementBubbleInteractions;

constexpr int kIconSize = 16;
// TODO(crbug.com/1408790): Row height should be computed from line/icon heights
// and desired paddings, instead of a fixed value to account for font size
// changes.
// The height of the row in the table layout displaying the password details.
constexpr int kDetailRowHeight = 44;
constexpr int kMaxLinesVisibleFromPasswordNote = 7;

void WriteToClipboard(const std::u16string& text) {
  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
  scw.WriteText(text);
}

std::unique_ptr<views::View> CreateIconView(
    const gfx::VectorIcon& vector_icon) {
  // TODO(crbug.com/1408790): Double check if it should always be not accessible
  // and that there is always another way to let user know important
  // information.
  auto icon = std::make_unique<NonAccessibleImageView>();
  icon->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icon, ui::kColorIconSecondary, kIconSize));
  return icon;
}

// Creates a view of the same height as the height of the each row in the table,
// and vertically centers the child view inside it. This is used to wrap icons
// and image buttons to ensure the icons are vertically aligned with the center
// of the first row in the text that lives inside labels in the same row even if
// the text spans multiple lines such as password notes.
//
//                <---icon size-->
//       |        |--------------|
//       |        |              |
//       |        |--------------|
//  line height   |  child view  |
//       |        |--------------|
//       |        |              |
//       |        |--------------|
//
std::unique_ptr<views::View> CreateWrappedView(
    std::unique_ptr<views::View> child_view) {
  auto wrapper = std::make_unique<views::BoxLayoutView>();
  wrapper->SetPreferredSize(
      gfx::Size(/*width=*/kIconSize, /*height=*/kDetailRowHeight));
  wrapper->SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  wrapper->AddChildView(std::move(child_view));
  return wrapper;
}

std::unique_ptr<views::FlexLayoutView> CreateDetailsRow(
    const gfx::VectorIcon& row_icon,
    std::unique_ptr<views::View> detail_view) {
  auto row = std::make_unique<views::FlexLayoutView>();
  row->SetCollapseMargins(true);
  row->SetDefault(
      views::kMarginsKey,
      gfx::Insets::VH(0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                             views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  row->SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  row->AddChildView(CreateWrappedView(CreateIconView(row_icon)));

  detail_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithWeight(1));
  row->AddChildView(std::move(detail_view));
  return row;
}

std::unique_ptr<views::View> CreateDetailsRowWithActionButton(
    const gfx::VectorIcon& row_icon,
    std::unique_ptr<views::View> detail_view,
    const gfx::VectorIcon& action_icon,
    const std::u16string& action_button_tooltip_text,
    views::Button::PressedCallback action_button_callback,
    ManagePasswordsViewIDs action_button_id) {
  std::unique_ptr<views::FlexLayoutView> row =
      CreateDetailsRow(row_icon, std::move(detail_view));

  std::unique_ptr<views::ImageButton> action_button =
      CreateVectorImageButtonWithNativeTheme(std::move(action_button_callback),
                                             action_icon, kIconSize);
  action_button->SetTooltipText(action_button_tooltip_text);
  action_button->SetID(static_cast<int>(action_button_id));
  row->AddChildView(CreateWrappedView(std::move(action_button)));
  return row;
}

std::unique_ptr<views::View> CreatePasswordLabelWithEyeIconView(
    std::unique_ptr<views::Label> password_label,
    base::RepeatingClosure on_eye_icon_clicked) {
  auto password_label_with_eye_icon_view =
      std::make_unique<views::BoxLayoutView>();
  auto* password_label_ptr = password_label_with_eye_icon_view->AddChildView(
      std::move(password_label));
  password_label_ptr->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kScaleToMaximum));

  auto* eye_icon = password_label_with_eye_icon_view->AddChildView(
      std::make_unique<views::ToggleImageButton>(
          views::Button::PressedCallback()));
  eye_icon->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_SHOW_PASSWORD));
  eye_icon->SetToggledTooltipText(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_HIDE_PASSWORD));
  eye_icon->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  eye_icon->SetID(
      static_cast<int>(ManagePasswordsViewIDs::kRevealPasswordButton));
  views::SetImageFromVectorIconWithColorId(
      eye_icon, views::kEyeIcon, ui::kColorIcon, ui::kColorIconDisabled);
  views::SetToggledImageFromVectorIconWithColorId(
      eye_icon, views::kEyeCrossedIcon, ui::kColorIcon, ui::kColorIconDisabled);

  eye_icon->SetCallback(
      base::BindRepeating(
          [](views::ToggleImageButton* toggle_button,
             views::Label* password_label) {
            password_label->SetObscured(!password_label->GetObscured());
            toggle_button->SetToggled(!toggle_button->GetToggled());

            if (!password_label->GetObscured()) {
              password_manager::metrics_util::
                  LogUserInteractionsInPasswordManagementBubble(
                      PasswordManagementBubbleInteractions::
                          kPasswordShowButtonClicked);
            }
          },
          eye_icon, password_label_ptr)
          .Then(std::move(on_eye_icon_clicked)));

  return password_label_with_eye_icon_view;
}

std::unique_ptr<views::View> CreateNoteLabel(
    const password_manager::PasswordForm& form) {
  std::u16string note_to_display =
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_EMPTY_NOTE);
  absl::optional<std::u16string> note =
      form.GetNoteWithEmptyUniqueDisplayName();
  // TODO(crbug.com/1408790): Consider adding another API to the password form
  // that returns the value directly instead of having to check whether a value
  // is set or not in all UI surfaces.
  if (note.has_value() && !note.value().empty()) {
    note_to_display = note.value();
  }

  auto note_label = std::make_unique<views::Label>(
      std::move(note_to_display), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  note_label->SetMultiLine(true);
  // TODO(crbug.com/1382017): Review string with UX and use internationalized
  // string.
  note_label->SetAccessibleName(u"Password Note");
  note_label->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_TOP);
  note_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  note_label->SetSelectable(true);
  int kNoteLabelMaxWidth = views::LayoutProvider::Get()->GetDistanceMetric(
                               views::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                           4 * ChromeLayoutProvider::Get()->GetDistanceMetric(
                                   views::DISTANCE_RELATED_CONTROL_HORIZONTAL) -
                           2 * kIconSize;
  note_label->SetMaximumWidth(kNoteLabelMaxWidth);
  note_label->SetID(static_cast<int>(ManagePasswordsViewIDs::kNoteLabel));

  int line_height = views::style::GetLineHeight(note_label->GetTextContext(),
                                                note_label->GetTextStyle());
  int vertical_margin = (kDetailRowHeight - line_height) / 2;
  auto scroll_view = std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled);
  scroll_view->SetProperty(views::kMarginsKey,
                           gfx::Insets::VH(vertical_margin, 0));
  scroll_view->SetContents(std::move(note_label));
  scroll_view->ClipHeightTo(line_height,
                            kMaxLinesVisibleFromPasswordNote * line_height);
  return scroll_view;
}

std::unique_ptr<views::View> CreateEditUsernameRow(
    const password_manager::PasswordForm& form,
    raw_ptr<views::Textfield>* textfield) {
  DCHECK(form.username_value.empty());
  auto row = std::make_unique<views::FlexLayoutView>();
  row->SetCollapseMargins(true);
  row->SetDefault(
      views::kMarginsKey,
      gfx::Insets::VH(0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                             views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  row->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  row->AddChildView(CreateWrappedView(CreateIconView(kAccountCircleIcon)));

  *textfield = row->AddChildView(std::make_unique<views::Textfield>());
  // TODO(crbug.com/1382017): use internationalized string.
  (*textfield)->SetAccessibleName(u"Username");
  (*textfield)
      ->SetID(static_cast<int>(ManagePasswordsViewIDs::kUsernameTextField));
  (*textfield)
      ->SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kUnbounded));
  return row;
}

std::unique_ptr<views::View> CreateEditNoteRow(
    const password_manager::PasswordForm& form,
    raw_ptr<views::Textarea>* textarea) {
  auto row = std::make_unique<views::FlexLayoutView>();
  row->SetCollapseMargins(true);
  row->SetDefault(
      views::kMarginsKey,
      gfx::Insets::VH(0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                             views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  row->SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  row->AddChildView(CreateWrappedView(CreateIconView(kNotesIcon)));

  *textarea = row->AddChildView(std::make_unique<views::Textarea>());
  (*textarea)->SetText(
      form.GetNoteWithEmptyUniqueDisplayName().value_or(std::u16string()));
  // TODO(crbug.com/1382017): use internationalized string.
  (*textarea)->SetAccessibleName(u"Password Note");
  int line_height = views::style::GetLineHeight(views::style::CONTEXT_TEXTFIELD,
                                                views::style::STYLE_PRIMARY);
  (*textarea)->SetPreferredSize(
      gfx::Size(0, kMaxLinesVisibleFromPasswordNote * line_height +
                       2 * ChromeLayoutProvider::Get()->GetDistanceMetric(
                               views::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING)));
  (*textarea)->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));
  (*textarea)->SetID(static_cast<int>(ManagePasswordsViewIDs::kNoteTextarea));
  return row;
}

}  // namespace

// static
std::unique_ptr<views::View> ManagePasswordsDetailsView::CreateTitleView(
    const password_manager::PasswordForm& password_form,
    base::RepeatingClosure on_back_clicked_callback) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  auto header = std::make_unique<views::BoxLayoutView>();
  // Set the space between the icons and title similar to the default behavior
  // in BubbleFrameView::Layout().
  header->SetBetweenChildSpacing(
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG_TITLE).left());

  auto back_button = views::CreateVectorImageButtonWithNativeTheme(
      on_back_clicked_callback, vector_icons::kArrowBackIcon);
  back_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  views::InstallCircleHighlightPathGenerator(back_button.get());
  header->AddChildView(std::move(back_button));

  std::string shown_origin =
      password_manager::GetShownOriginAndLinkUrl(password_form).first;
  header->AddChildView(views::BubbleFrameView::CreateDefaultTitleLabel(
      base::UTF8ToUTF16(shown_origin)));
  return header;
}

ManagePasswordsDetailsView::ManagePasswordsDetailsView(
    password_manager::PasswordForm password_form,
    base::RepeatingClosure switched_to_edit_mode_callback,
    base::RepeatingClosure on_activity_callback,
    base::RepeatingCallback<void(bool)> on_input_validation_callback)
    : switched_to_edit_mode_callback_(
          std::move(switched_to_edit_mode_callback)),
      on_activity_callback_(std::move(on_activity_callback)),
      on_input_validation_callback_(std::move(on_input_validation_callback)) {
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  std::unique_ptr<views::Label> username_label =
      CreateUsernameLabel(password_form);
  username_label->SetID(
      static_cast<int>(ManagePasswordsViewIDs::kUsernameLabel));
  if (!password_form.username_value.empty()) {
    auto copy_username_button_callback =
        base::BindRepeating(&WriteToClipboard, password_form.username_value)
            .Then(on_activity_callback_)
            .Then(base::BindRepeating(
                &password_manager::metrics_util::
                    LogUserInteractionsInPasswordManagementBubble,
                PasswordManagementBubbleInteractions::
                    kUsernameCopyButtonClicked));
    AddChildView(CreateDetailsRowWithActionButton(
        kAccountCircleIcon, std::move(username_label),
        vector_icons::kContentCopyIcon,
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UI_COPY_USERNAME),
        std::move(copy_username_button_callback),
        ManagePasswordsViewIDs::kCopyUsernameButton));
  } else {
    read_username_row_ = AddChildView(CreateDetailsRowWithActionButton(
        kAccountCircleIcon, std::move(username_label), vector_icons::kEditIcon,
        l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_EDIT_USERNAME_TOOLTIP),
        base::BindRepeating(
            &ManagePasswordsDetailsView::SwitchToEditUsernameMode,
            base::Unretained(this)),
        ManagePasswordsViewIDs::kEditUsernameButton));
    edit_username_row_ = AddChildView(
        CreateEditUsernameRow(password_form, &username_textfield_));
    text_changed_subscriptions_.push_back(
        username_textfield_->AddTextChangedCallback(
            base::BindRepeating(&ManagePasswordsDetailsView::OnUserInputChanged,
                                base::Unretained(this))));
    edit_username_row_->SetVisible(false);
  }

  std::unique_ptr<views::Label> password_label =
      CreatePasswordLabel(password_form);
  password_label->SetID(
      static_cast<int>(ManagePasswordsViewIDs::kPasswordLabel));
  if (!password_form.federation_origin.opaque()) {
    // Federated credentials, there is no note and no copy password button.
    AddChildView(CreateDetailsRow(kKeyIcon, std::move(password_label)));
    return;
  }
  auto copy_password_button_callback =
      base::BindRepeating(&WriteToClipboard, password_form.password_value)
          .Then(on_activity_callback_)
          .Then(base::BindRepeating(
              &password_manager::metrics_util::
                  LogUserInteractionsInPasswordManagementBubble,
              PasswordManagementBubbleInteractions::
                  kPasswordCopyButtonClicked));
  AddChildView(CreateDetailsRowWithActionButton(
      kKeyIcon,
      CreatePasswordLabelWithEyeIconView(std::move(password_label),
                                         on_activity_callback_),
      vector_icons::kContentCopyIcon,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UI_COPY_PASSWORD),
      std::move(copy_password_button_callback),
      ManagePasswordsViewIDs::kCopyPasswordButton));

  // Add two rows: one for reading the note which is visible by default, and
  // another to edit the note, which is hidden by default. Clicking the Edit
  // icon next to the note row will hide the read row, and show the edit row.
  read_note_row_ = AddChildView(CreateDetailsRowWithActionButton(
      kNotesIcon, CreateNoteLabel(password_form), vector_icons::kEditIcon,
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_EDIT_NOTE_TOOLTIP),
      base::BindRepeating(&ManagePasswordsDetailsView::SwitchToEditNoteMode,
                          base::Unretained(this)),
      ManagePasswordsViewIDs::kEditNoteButton));
  edit_note_row_ =
      AddChildView(CreateEditNoteRow(password_form, &note_textarea_));
  text_changed_subscriptions_.push_back(note_textarea_->AddTextChangedCallback(
      base::BindRepeating(&ManagePasswordsDetailsView::OnUserInputChanged,
                          base::Unretained(this))));
  edit_note_row_->SetVisible(false);
}

ManagePasswordsDetailsView::~ManagePasswordsDetailsView() = default;

void ManagePasswordsDetailsView::SwitchToReadingMode() {
  read_note_row_->SetVisible(true);
  edit_note_row_->SetVisible(false);
  on_activity_callback_.Run();
}

absl::optional<std::u16string>
ManagePasswordsDetailsView::GetUserEnteredUsernameValue() const {
  if (username_textfield_) {
    return username_textfield_->GetText();
  }
  return absl::nullopt;
}

absl::optional<std::u16string>
ManagePasswordsDetailsView::GetUserEnteredPasswordNoteValue() const {
  if (note_textarea_) {
    return note_textarea_->GetText();
  }
  return absl::nullopt;
}

void ManagePasswordsDetailsView::SwitchToEditUsernameMode() {
  DCHECK(read_username_row_);
  DCHECK(edit_username_row_);
  read_username_row_->SetVisible(false);
  edit_username_row_->SetVisible(true);
  switched_to_edit_mode_callback_.Run();
  DCHECK(username_textfield_);
  username_textfield_->RequestFocus();
  on_activity_callback_.Run();
  LogUserInteractionsInPasswordManagementBubble(
      PasswordManagementBubbleInteractions::kUsernameEditButtonClicked);
  // Invoke OnUserInputChanged() to validate the current input. Relevant only
  // for empty username to make sure the bubble is opened showing the username
  // as invalid.
  OnUserInputChanged();
}

void ManagePasswordsDetailsView::SwitchToEditNoteMode() {
  read_note_row_->SetVisible(false);
  edit_note_row_->SetVisible(true);
  switched_to_edit_mode_callback_.Run();
  DCHECK(note_textarea_);
  on_activity_callback_.Run();
  note_textarea_->RequestFocus();
  LogUserInteractionsInPasswordManagementBubble(
      PasswordManagementBubbleInteractions::kNoteEditButtonClicked);
}

void ManagePasswordsDetailsView::OnUserInputChanged() {
  CHECK(note_textarea_);
  on_activity_callback_.Run();
  bool is_username_invalid = false;
  if (username_textfield_ && username_textfield_->IsDrawn()) {
    is_username_invalid = username_textfield_->GetText().empty();
  }
  bool is_note_invalid =
      note_textarea_->IsDrawn() &&
      note_textarea_->GetText().length() >
          password_manager::constants::kMaxPasswordNoteLength;

  if (username_textfield_ && username_textfield_->IsDrawn()) {
    username_textfield_->SetInvalid(is_username_invalid);
  }
  if (note_textarea_->IsDrawn()) {
    note_textarea_->SetInvalid(is_note_invalid);
  }

  on_input_validation_callback_.Run(is_username_invalid || is_note_invalid);
}
