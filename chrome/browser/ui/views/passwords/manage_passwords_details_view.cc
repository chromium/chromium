// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_details_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_view_ids.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/common/password_manager_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/vector_icons.h"

namespace {

using password_manager::ManagePasswordsViewIDs;
using password_manager::metrics_util::PasswordManagementBubbleInteractions;

constexpr int kIconSize = 16;
// TODO(crbug.com/40253695): Row height should be computed from line/icon
// heights and desired paddings, instead of a fixed value to account for font
// size changes. The height of the row in the table layout displaying the
// password details.
constexpr int kDetailRowHeight = 44;
constexpr int kMaxLinesVisibleFromPasswordNote = 7;

void WriteToClipboard(const std::u16string& text, bool is_confidential) {
  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
  scw.WriteText(text);
  if (is_confidential) {
    scw.MarkAsConfidential();
  }
}

// Computes the margins of each row. This adjusts the left margin equal to the
// dialog left margin + the back button insets to make sure all icons are
// vertically aligned with the back button.
gfx::Insets ComputeRowMargins() {
  const auto* const layout_provider = ChromeLayoutProvider::Get();
  gfx::Insets margins = layout_provider->GetInsetsMetric(views::INSETS_DIALOG);
  margins.set_top_bottom(0, 0);
  margins.set_left(
      margins.left() +
      layout_provider->GetInsetsMetric(views::INSETS_VECTOR_IMAGE_BUTTON)
          .left());
  return margins;
}

std::unique_ptr<views::View> CreateIconView(
    const gfx::VectorIcon& vector_icon) {
  // TODO(crbug.com/40253695): Double check if it should always be not
  // accessible and that there is always another way to let user know important
  // information.
  auto icon = std::make_unique<NonAccessibleImageView>();
  icon->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icon, ui::kColorIconSecondary, kIconSize));
  return icon;
}

std::unique_ptr<views::Label> CreateErrorLabel(std::u16string error_msg) {
  // Max width must be set for multi-line labels. Compute the max to be the
  // bubble width subtracting the icon and the empty spaces between the
  // controls.
  const int kErrorLabelMaxWidth =
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
      3 * ChromeLayoutProvider::Get()->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_HORIZONTAL) -
      kIconSize;
  // Use `CONTEXT_DEEMPHASIZED` to make the error message slightly smaller.
  return views::Builder<views::Label>()
      .SetText(std::move(error_msg))
      .SetTextStyle(views::style::STYLE_HINT)
      .SetTextContext(CONTEXT_DEEMPHASIZED)
      .SetEnabledColorId(ui::kColorAlertHighSeverity)
      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
      .SetVisible(false)
      .SetMultiLine(true)
      .SetMaximumWidth(kErrorLabelMaxWidth)
      .Build();
}

// Vertically aligns `textfield` in the middle of the row, such that the text
// inside `textfield` is aligned with the icon in the row. In case of a
// multiline textarea, the first line in the contents is vertically aligned with
// the icon.
void AlignTextfieldWithRowIcon(views::Textfield* textfield) {
  int line_height = views::TypographyProvider::Get().GetLineHeight(
      views::style::CONTEXT_TEXTFIELD, views::style::STYLE_PRIMARY);
  int vertical_padding_inside_textfield =
      2 * ChromeLayoutProvider::Get()->GetDistanceMetric(
              views::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING);
  int remaining_vertical_space =
      kDetailRowHeight - line_height - vertical_padding_inside_textfield;
  if (remaining_vertical_space <= 0) {
    return;
  }
  textfield->SetProperty(views::kMarginsKey,
                         gfx::Insets().set_top((remaining_vertical_space / 2)));
}

// Aligns `error_label` such that the error message is vertically aligned with
// the text in the corropsnding textfield/textarea.
void AlignErrorLabelWithTextFieldContents(views::Label* error_label) {
  // Create a border around the error message that has the left insets matching
  // the inner padding in the textarea above to align the error message with the
  // text in the textarea. The border has zero insets on all other sides.
  gfx::Insets insets;
  int textfield_inner_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING);
  if (base::i18n::IsRTL()) {
    insets.set_right(textfield_inner_padding);
  } else {
    insets.set_left(textfield_inner_padding);
  }
  error_label->SetBorder(views::CreateEmptyBorder(insets));
}

// Creates a view of the same height as the height of the each row in the table,
// and vertically centers the child view inside it. This is used to wrap icons
// and image buttons to ensure the icons are vertically aligned with the center
// of the first row in the text that lives inside labels in the same row even if
// the text spans multiple lines such as password notes.
//
//                <--child width-->
//       |        |---------------|
//       |        |               |
//       |        |---------------|
//  line height   |  child view   |
//       |        |---------------|
//       |        |               |
//       |        |---------------|
//
std::unique_ptr<views::View> CreateWrappedView(
    std::unique_ptr<views::View> child_view) {
  auto wrapper = std::make_unique<views::BoxLayoutView>();
  wrapper->SetPreferredSize(gfx::Size(child_view->GetPreferredSize().width(),
                                      /*height=*/kDetailRowHeight));
  wrapper->SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  wrapper->AddChildView(std::move(child_view));
  return wrapper;
}

std::unique_ptr<views::FlexLayoutView> CreateDetailsRow(
    const gfx::VectorIcon& row_icon,
    std::unique_ptr<views::View> detail_view) {
  auto row = std::make_unique<views::FlexLayoutView>();
  row->SetCollapseMargins(true);
  row->SetInteriorMargin(ComputeRowMargins());
  row->SetDefault(
      views::kMarginsKey,
      gfx::Insets::VH(0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                             views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  row->SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  row->AddChildView(CreateWrappedView(CreateIconView(row_icon)));

  detail_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithWeight(1));
  detail_view->SetProperty(views::kCrossAxisAlignmentKey,
                           views::LayoutAlignment::kStretch);
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
  views::InstallCircleHighlightPathGenerator(action_button.get());
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
  password_label_ptr->SetProperty(views::kBoxLayoutFlexKey,
                                  views::BoxLayoutFlexSpecification());

  auto* eye_icon = password_label_with_eye_icon_view->AddChildView(
      CreateVectorToggleImageButton(views::Button::PressedCallback()));
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
  views::InstallCircleHighlightPathGenerator(eye_icon);
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

// Creates the label for the note with custom logic for logging
// metrics for selecting and copying text of the note.
class NoteLabel : public views::Label {
 public:
  explicit NoteLabel(std::u16string note)
      : views::Label(note,
                     views::style::CONTEXT_DIALOG_BODY_TEXT,
                     views::style::STYLE_SECONDARY),
        note_(std::move(note)) {
    std::u16string note_to_display =
        note_.empty()
            ? l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_EMPTY_NOTE)
            : note_;
    if (note_.empty()) {
      SetText(note_to_display);
    }
    GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
        IDS_MANAGE_PASSWORDS_NOTE_ACCESSIBLE_NAME, note_to_display));
  }

 public:
  void ExecuteCommand(int command_id, int event_flags) override {
    views::Label::ExecuteCommand(command_id, event_flags);
    if (note_.empty()) {
      return;
    }

    if (command_id == MenuCommands::kCopy && HasSelection()) {
      LogUserInteractionsInPasswordManagementBubble(
          HasFullSelection()
              ? PasswordManagementBubbleInteractions::kNoteFullyCopied
              : PasswordManagementBubbleInteractions::kNotePartiallyCopied);
    }

    if (command_id == MenuCommands::kSelectAll) {
      LogUserInteractionsInPasswordManagementBubble(
          PasswordManagementBubbleInteractions::kNoteFullySelected);
    }
  }

 protected:
  bool OnKeyPressed(const ui::KeyEvent& event) override {
    if (note_.empty()) {
      return views::Label::OnKeyPressed(event);
    }

    const bool alt = event.IsAltDown() || event.IsAltGrDown();
    const bool control = event.IsControlDown() || event.IsCommandDown();

    if (control && !alt) {
      if (event.key_code() == ui::VKEY_A) {
        LogUserInteractionsInPasswordManagementBubble(
            PasswordManagementBubbleInteractions::kNoteFullySelected);
      }

      if (event.key_code() == ui::VKEY_C && HasSelection()) {
        LogUserInteractionsInPasswordManagementBubble(
            HasFullSelection()
                ? PasswordManagementBubbleInteractions::kNoteFullyCopied
                : PasswordManagementBubbleInteractions::kNotePartiallyCopied);
      }
    }

    return views::Label::OnKeyPressed(event);
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    views::Label::OnMouseReleased(event);
    if (note_.empty() || !HasSelection()) {
      return;
    }

    LogUserInteractionsInPasswordManagementBubble(
        HasFullSelection()
            ? PasswordManagementBubbleInteractions::kNoteFullySelected
            : PasswordManagementBubbleInteractions::kNotePartiallySelected);
  }

 private:
  std::u16string note_;
};

std::unique_ptr<views::View> CreateNoteLabel(
    const password_manager::PasswordForm& form) {
  auto note_label =
      std::make_unique<NoteLabel>(form.GetNoteWithEmptyUniqueDisplayName());
  note_label->SetMultiLine(true);
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

  int line_height = views::TypographyProvider::Get().GetLineHeight(
      note_label->GetTextContext(), note_label->GetTextStyle());
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
    raw_ptr<views::Textfield>* textfield,
    raw_ptr<views::Label>* error_label) {
  DCHECK(form.username_value.empty());
  const auto* const layout_provider = ChromeLayoutProvider::Get();
  auto row = std::make_unique<views::FlexLayoutView>();
  row->SetCollapseMargins(true);
  row->SetInteriorMargin(ComputeRowMargins());
  row->SetDefault(
      views::kMarginsKey,
      gfx::Insets::VH(0, layout_provider->GetDistanceMetric(
                             views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  row->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  row->AddChildView(CreateWrappedView(CreateIconView(kAccountCircleIcon)));
  auto* username_with_error_label_view =
      row->AddChildView(std::make_unique<views::BoxLayoutView>());
  username_with_error_label_view->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  username_with_error_label_view->SetBetweenChildSpacing(
      layout_provider->GetDistanceMetric(
          DISTANCE_CONTENT_LIST_VERTICAL_SINGLE));
  username_with_error_label_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));
  *textfield = username_with_error_label_view->AddChildView(
      std::make_unique<views::Textfield>());
  (*textfield)
      ->GetViewAccessibility()
      .SetName(
          l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_USERNAME_TEXTFIELD));
  (*textfield)
      ->SetID(static_cast<int>(ManagePasswordsViewIDs::kUsernameTextField));
  AlignTextfieldWithRowIcon(*textfield);
  *error_label = username_with_error_label_view->AddChildView(
      CreateErrorLabel(l10n_util::GetStringFUTF16(
          IDS_PASSWORD_MANAGER_UI_USERNAME_ALREADY_USED,
          base::UTF8ToUTF16(password_manager::GetShownOrigin(
              url::Origin::Create(form.url))))));
  AlignErrorLabelWithTextFieldContents(*error_label);
  return row;
}

std::unique_ptr<views::View> CreateEditNoteRow(
    const password_manager::PasswordForm& form,
    raw_ptr<views::Textarea>* textarea,
    raw_ptr<views::Label>* error_label) {
  const auto* const layout_provider = ChromeLayoutProvider::Get();
  auto row = std::make_unique<views::FlexLayoutView>();
  row->SetCollapseMargins(true);
  row->SetInteriorMargin(ComputeRowMargins());
  row->SetDefault(
      views::kMarginsKey,
      gfx::Insets::VH(0, layout_provider->GetDistanceMetric(
                             views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  row->SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  row->AddChildView(CreateWrappedView(CreateIconView(kNotesIcon)));
  auto* note_with_error_label_view =
      row->AddChildView(std::make_unique<views::BoxLayoutView>());
  note_with_error_label_view->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  note_with_error_label_view->SetBetweenChildSpacing(
      layout_provider->GetDistanceMetric(
          DISTANCE_CONTENT_LIST_VERTICAL_SINGLE));
  note_with_error_label_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));

  *textarea = note_with_error_label_view->AddChildView(
      std::make_unique<views::Textarea>());
  (*textarea)->SetText(form.GetNoteWithEmptyUniqueDisplayName());
  (*textarea)->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_NOTE_TEXTFIELD));
  int line_height = views::TypographyProvider::Get().GetLineHeight(
      views::style::CONTEXT_TEXTFIELD, views::style::STYLE_PRIMARY);
  (*textarea)->SetPreferredSize(
      gfx::Size(0, kMaxLinesVisibleFromPasswordNote * line_height +
                       2 * ChromeLayoutProvider::Get()->GetDistanceMetric(
                               views::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING)));

  (*textarea)->SetID(static_cast<int>(ManagePasswordsViewIDs::kNoteTextarea));
  AlignTextfieldWithRowIcon(*textarea);
  *error_label = note_with_error_label_view->AddChildView(
      CreateErrorLabel(l10n_util::GetStringFUTF16(
          IDS_PASSWORD_MANAGER_UI_NOTE_CHARACTER_COUNT_WARNING,
          base::NumberToString16(
              password_manager::constants::kMaxPasswordNoteLength))));
  AlignErrorLabelWithTextFieldContents(*error_label);
  return row;
}

std::unique_ptr<RichHoverButton> CreateManagePasswordRow(
    base::RepeatingClosure on_manage_password_clicked_callback) {
  auto manage_password_row = std::make_unique<RichHoverButton>(
      /*callback=*/
      std::move(on_manage_password_clicked_callback),
      /*main_image_icon=*/
      ui::ImageModel::FromVectorIcon(vector_icons::kSettingsIcon,
                                     ui::kColorIcon),
      /*title_text=*/
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORD_BUTTON),
      /*secondary_text=*/std::u16string(),
      /*tooltip_text=*/
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORD_BUTTON),
      /*subtitle_text=*/std::u16string(),
      /*action_image_icon=*/
      ui::ImageModel::FromVectorIcon(vector_icons::kLaunchIcon,
                                     ui::kColorIconSecondary,
                                     GetLayoutConstant(PAGE_INFO_ICON_SIZE)),
      /*state_icon=*/std::nullopt);
  manage_password_row->SetID(static_cast<int>(
      password_manager::ManagePasswordsViewIDs::kManagePasswordButton));
  return manage_password_row;
}

}  // namespace

// static
std::unique_ptr<views::View> ManagePasswordsDetailsView::CreateTitleView(
    const password_manager::PasswordForm& password_form,
    std::optional<base::RepeatingClosure> on_back_clicked_callback) {
  const auto* const layout_provider = ChromeLayoutProvider::Get();
  auto header = std::make_unique<views::BoxLayoutView>();
  // Set the space between the icon and title similar to the space in the row
  // below to make sure the bubble title and the labels below are vertically
  // aligned. In the rows below the distance between the icon and the text is
  // DISTANCE_RELATED_CONTROL_HORIZONTAL. But the icon in the title has a border
  // of size INSETS_VECTOR_IMAGE_BUTTON to have space for the focus ring, and
  // hence this is subtracted here.
  header->SetBetweenChildSpacing(
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL) -
      layout_provider->GetInsetsMetric(views::INSETS_VECTOR_IMAGE_BUTTON)
          .right());

  if (on_back_clicked_callback) {
    auto back_button = views::CreateVectorImageButtonWithNativeTheme(
        *on_back_clicked_callback, vector_icons::kArrowBackIcon);
    back_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
    views::InstallCircleHighlightPathGenerator(back_button.get());
    back_button->SetProperty(views::kElementIdentifierKey, kBackButton);
    header->AddChildView(std::move(back_button));
  }

  std::string shown_origin = password_manager::GetShownOrigin(
      password_manager::CredentialUIEntry(password_form));
  header->AddChildView(views::BubbleFrameView::CreateDefaultTitleLabel(
      base::UTF8ToUTF16(shown_origin)));
  return header;
}

ManagePasswordsDetailsView::ManagePasswordsDetailsView(
    password_manager::PasswordForm password_form,
    bool allow_empty_username_edit,
    base::RepeatingCallback<bool(const std::u16string&)>
        username_exists_callback,
    base::RepeatingClosure switched_to_edit_mode_callback,
    base::RepeatingClosure on_activity_callback,
    base::RepeatingCallback<void(bool)> on_input_validation_callback,
    base::RepeatingClosure on_manage_password_clicked_callback)
    : username_exists_callback_(std::move(username_exists_callback)),
      switched_to_edit_mode_callback_(
          std::move(switched_to_edit_mode_callback)),
      on_activity_callback_(std::move(on_activity_callback)),
      on_input_validation_callback_(std::move(on_input_validation_callback)) {
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  std::unique_ptr<views::Label> username_label =
      CreateUsernameLabel(password_form);
  username_label->SetID(
      static_cast<int>(ManagePasswordsViewIDs::kUsernameLabel));
  username_label->GetViewAccessibility().SetName(
      l10n_util::GetStringFUTF16(IDS_MANAGE_PASSWORDS_USERNAME_ACCESSIBLE_NAME,
                                 username_label->GetText()));
  if (!password_form.username_value.empty()) {
    auto copy_username_button_callback =
        base::BindRepeating(&WriteToClipboard, password_form.username_value,
                            /*is_confidential=*/false)
            .Then(on_activity_callback_)
            .Then(base::BindRepeating(
                &password_manager::metrics_util::
                    LogUserInteractionsInPasswordManagementBubble,
                PasswordManagementBubbleInteractions::
                    kUsernameCopyButtonClicked));
    AddChildView(CreateDetailsRowWithActionButton(
        kAccountCircleIcon, std::move(username_label), kCopyIcon,
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UI_COPY_USERNAME),
        std::move(copy_username_button_callback),
        ManagePasswordsViewIDs::kCopyUsernameButton));
  } else if (allow_empty_username_edit) {
    read_username_row_ = AddChildView(CreateDetailsRowWithActionButton(
        kAccountCircleIcon, std::move(username_label), vector_icons::kEditIcon,
        l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_EDIT_USERNAME_TOOLTIP),
        base::BindRepeating(
            &ManagePasswordsDetailsView::SwitchToEditUsernameMode,
            base::Unretained(this)),
        ManagePasswordsViewIDs::kEditUsernameButton));
    edit_username_row_ = AddChildView(CreateEditUsernameRow(
        password_form, &username_textfield_, &username_error_label_));
    text_changed_subscriptions_.push_back(
        username_textfield_->AddTextChangedCallback(
            base::BindRepeating(&ManagePasswordsDetailsView::OnUserInputChanged,
                                base::Unretained(this))));
    edit_username_row_->SetVisible(false);
  } else {
    AddChildView(
        CreateDetailsRow(kAccountCircleIcon, std::move(username_label)));
  }

  std::unique_ptr<views::Label> password_label =
      CreatePasswordLabel(password_form);
  password_label->SetID(
      static_cast<int>(ManagePasswordsViewIDs::kPasswordLabel));
  if (password_form.IsFederatedCredential()) {
    // Federated credentials, there is no note and no copy password button.
    AddChildView(CreateDetailsRow(vector_icons::kPasswordManagerIcon,
                                  std::move(password_label)));
    return;
  }
  auto copy_password_button_callback =
      base::BindRepeating(&WriteToClipboard, password_form.password_value,
                          /*is_confidential=*/true)
          .Then(on_activity_callback_)
          .Then(base::BindRepeating(
              &password_manager::metrics_util::
                  LogUserInteractionsInPasswordManagementBubble,
              PasswordManagementBubbleInteractions::
                  kPasswordCopyButtonClicked));
  AddChildView(CreateDetailsRowWithActionButton(
      vector_icons::kPasswordManagerIcon,
      CreatePasswordLabelWithEyeIconView(std::move(password_label),
                                         on_activity_callback_),
      kCopyIcon,
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
  edit_note_row_ = AddChildView(
      CreateEditNoteRow(password_form, &note_textarea_, &note_error_label_));
  text_changed_subscriptions_.push_back(note_textarea_->AddTextChangedCallback(
      base::BindRepeating(&ManagePasswordsDetailsView::OnUserInputChanged,
                          base::Unretained(this))));
  edit_note_row_->SetVisible(false);

  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordManualFallbackAvailable)) {
    separator_row_ =
        AddChildView(views::Builder<views::Separator>()
                         .SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
                             ChromeLayoutProvider::Get()->GetDistanceMetric(
                                 DISTANCE_CONTENT_LIST_VERTICAL_SINGLE),
                             0)))
                         .Build());

    manage_password_row_ = AddChildView(CreateManagePasswordRow(
        std::move(on_manage_password_clicked_callback)));
  } else {
    // We need the bottom padding only if the "Manage password" button is not
    // added to the layout.
    SetInsideBorderInsets(
        gfx::Insets().set_bottom(ChromeLayoutProvider::Get()
                                     ->GetInsetsMetric(views::INSETS_DIALOG)
                                     .bottom()));
  }

  SetProperty(views::kElementIdentifierKey, kTopView);
}

ManagePasswordsDetailsView::~ManagePasswordsDetailsView() = default;

void ManagePasswordsDetailsView::SwitchToReadingMode() {
  read_note_row_->SetVisible(true);
  edit_note_row_->SetVisible(false);
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordManualFallbackAvailable)) {
    // The "Manage password" button should be visible only in the reading mode.
    // The bottom padding should be absent in this mode to achieve the same
    // appearance of the button as in the `ManagerPasswordsView`.
    separator_row_->SetVisible(true);
    manage_password_row_->SetVisible(true);
    SetInsideBorderInsets(gfx::Insets());
  }

  on_activity_callback_.Run();
}

std::optional<std::u16string>
ManagePasswordsDetailsView::GetUserEnteredUsernameValue() const {
  if (username_textfield_) {
    return username_textfield_->GetText();
  }
  return std::nullopt;
}

std::optional<std::u16string>
ManagePasswordsDetailsView::GetUserEnteredPasswordNoteValue() const {
  if (note_textarea_) {
    return note_textarea_->GetText();
  }
  return std::nullopt;
}

void ManagePasswordsDetailsView::SwitchToEditUsernameMode() {
  DCHECK(read_username_row_);
  DCHECK(edit_username_row_);
  read_username_row_->SetVisible(false);
  edit_username_row_->SetVisible(true);
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordManualFallbackAvailable)) {
    // The "Manage passwords" button should not be visible in the editor mode.
    // The bottom padding is added to offset the dialog buttons shown by the
    // `ManagePasswordsView`.
    separator_row_->SetVisible(false);
    manage_password_row_->SetVisible(false);
    SetInsideBorderInsets(
        gfx::Insets().set_bottom(ChromeLayoutProvider::Get()
                                     ->GetInsetsMetric(views::INSETS_DIALOG)
                                     .bottom()));
  }

  switched_to_edit_mode_callback_.Run();
  DCHECK(username_textfield_);
  username_textfield_->RequestFocus();
  on_activity_callback_.Run();
  LogUserInteractionsInPasswordManagementBubble(
      PasswordManagementBubbleInteractions::kUsernameEditButtonClicked);
}

void ManagePasswordsDetailsView::SwitchToEditNoteMode() {
  read_note_row_->SetVisible(false);
  edit_note_row_->SetVisible(true);
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordManualFallbackAvailable)) {
    // The "Manage passwords" button should not be visible in the editor mode.
    // The bottom padding is added to offset the dialog buttons shown by the
    // `ManagePasswordsView`.
    separator_row_->SetVisible(false);
    manage_password_row_->SetVisible(false);
    SetInsideBorderInsets(
        gfx::Insets().set_bottom(ChromeLayoutProvider::Get()
                                     ->GetInsetsMetric(views::INSETS_DIALOG)
                                     .bottom()));
  }

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
    is_username_invalid =
        !username_textfield_->GetText().empty() &&
        username_exists_callback_.Run(username_textfield_->GetText());
    username_error_label_->SetVisible(is_username_invalid);
    username_textfield_->SetInvalid(is_username_invalid);
  }

  bool is_note_invalid =
      note_textarea_->IsDrawn() &&
      note_textarea_->GetText().length() >
          password_manager::constants::kMaxPasswordNoteLength;
  if (note_textarea_->IsDrawn()) {
    note_textarea_->SetInvalid(is_note_invalid);
    note_error_label_->SetVisible(is_note_invalid);
  }

  // During validation error label may have changed it's visibility status, and
  // hence invoke `switched_to_edit_mode_callback_` to force redraw the bubble
  // to readjust the size. Ideally should have be a dedicated callback for
  // informing the embedders that size may have change, but since validation can
  // only occue in edit mode, it seems approipriate to reuse the same callback
  // instead of introducing another one.
  switched_to_edit_mode_callback_.Run();

  on_input_validation_callback_.Run(is_username_invalid || is_note_invalid);
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ManagePasswordsDetailsView, kTopView);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ManagePasswordsDetailsView, kBackButton);

BEGIN_METADATA(ManagePasswordsDetailsView)
END_METADATA
