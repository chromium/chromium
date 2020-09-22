// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_save_update_view.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/passwords/credentials_item_view.h"
#include "chrome/browser/ui/views/passwords/password_items_view.h"
#include "chrome/browser/ui/views/passwords/password_sign_in_promo_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/storage_partition.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/combobox_model_observer.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/editable_combobox/editable_combobox.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

namespace {

enum PasswordSaveUpdateViewColumnSetType {
  // | | (LEADING, FILL) | | (FILL, FILL) | |
  // Used for the username/password line of the bubble, for the pending view.
  DOUBLE_VIEW_COLUMN_SET_USERNAME,
  DOUBLE_VIEW_COLUMN_SET_PASSWORD,

  // | | (LEADING, FILL) | | (FILL, FILL) | | (TRAILING, FILL) | |
  // Used for the password line of the bubble, for the pending view.
  // Views are label, password and the eye icon.
  TRIPLE_VIEW_COLUMN_SET,
};

// Construct an appropriate ColumnSet for the given |type|, and add it
// to |layout|.
void BuildColumnSet(views::GridLayout* layout,
                    PasswordSaveUpdateViewColumnSetType type) {
  views::ColumnSet* column_set = layout->AddColumnSet(type);
  const int column_divider = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  using ColumnSize = views::GridLayout::ColumnSize;
  switch (type) {
    case DOUBLE_VIEW_COLUMN_SET_USERNAME:
    case DOUBLE_VIEW_COLUMN_SET_PASSWORD:
      column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                            views::GridLayout::kFixedSize,
                            ColumnSize::kUsePreferred, 0, 0);
      column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                   column_divider);
      column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                            1.0, ColumnSize::kUsePreferred, 0, 0);
      break;
    case TRIPLE_VIEW_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                            views::GridLayout::kFixedSize,
                            ColumnSize::kUsePreferred, 0, 0);
      column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                   column_divider);
      column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                            1.0, ColumnSize::kUsePreferred, 0, 0);
      column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                   column_divider);
      column_set->AddColumn(
          views::GridLayout::TRAILING, views::GridLayout::FILL,
          views::GridLayout::kFixedSize, ColumnSize::kUsePreferred, 0, 0);
      break;
  }
}

// Builds a credential row, adds the given elements to the layout.
// |password_view_button| is an optional field. If it is a nullptr, a
// DOUBLE_VIEW_COLUMN_SET_PASSWORD will be used for password row instead of
// TRIPLE_VIEW_COLUMN_SET.
void BuildCredentialRows(
    views::GridLayout* layout,
    std::unique_ptr<views::View> username_field,
    std::unique_ptr<views::View> password_field,
    std::unique_ptr<views::ToggleImageButton> password_view_button) {
  // Username row.
  BuildColumnSet(layout, DOUBLE_VIEW_COLUMN_SET_USERNAME);
  layout->StartRow(views::GridLayout::kFixedSize,
                   DOUBLE_VIEW_COLUMN_SET_USERNAME);
  std::unique_ptr<views::Label> username_label(new views::Label(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USERNAME_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY));
  username_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  std::unique_ptr<views::Label> password_label(new views::Label(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_PASSWORD_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY));
  password_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  int labels_width = std::max(username_label->GetPreferredSize().width(),
                              password_label->GetPreferredSize().width());
  int fields_height = std::max(username_field->GetPreferredSize().height(),
                               password_field->GetPreferredSize().height());

  layout->AddView(std::move(username_label), 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::FILL, labels_width, 0);
  layout->AddView(std::move(username_field), 1, 1, views::GridLayout::FILL,
                  views::GridLayout::FILL, 0, fields_height);

  layout->AddPaddingRow(views::GridLayout::kFixedSize,
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_CONTROL_LIST_VERTICAL));

  // Password row.
  PasswordSaveUpdateViewColumnSetType type =
      password_view_button ? TRIPLE_VIEW_COLUMN_SET
                           : DOUBLE_VIEW_COLUMN_SET_PASSWORD;
  BuildColumnSet(layout, type);
  layout->StartRow(views::GridLayout::kFixedSize, type);
  layout->AddView(std::move(password_label), 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::FILL, labels_width, 0);
  layout->AddView(std::move(password_field), 1, 1, views::GridLayout::FILL,
                  views::GridLayout::FILL, 0, fields_height);
  // The eye icon is also added to the layout if it was passed.
  if (password_view_button) {
    layout->AddView(std::move(password_view_button));
  }
}

// Create a vector which contains only the values in |items| and no elements.
std::vector<base::string16> ToValues(
    const autofill::ValueElementVector& items) {
  std::vector<base::string16> passwords;
  passwords.reserve(items.size());
  for (auto& pair : items)
    passwords.push_back(pair.first);
  return passwords;
}

std::unique_ptr<views::ToggleImageButton> CreatePasswordViewButton(
    views::ButtonListener* listener,
    bool are_passwords_revealed) {
  auto button = std::make_unique<views::ToggleImageButton>(listener);
  button->SetFocusForPlatform();
  button->SetInstallFocusRingOnFocus(true);
  button->SetRequestFocusOnPress(true);
  button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_SHOW_PASSWORD));
  button->SetToggledTooltipText(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_HIDE_PASSWORD));
  button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  button->SetToggled(are_passwords_revealed);
  return button;
}

// Creates an EditableCombobox from |PasswordForm.all_possible_usernames| or
// even just |PasswordForm.username_value|.
std::unique_ptr<views::EditableCombobox> CreateUsernameEditableCombobox(
    const autofill::PasswordForm& form) {
  std::vector<base::string16> usernames = {form.username_value};
  for (const autofill::ValueElementPair& other_possible_username_pair :
       form.all_possible_usernames) {
    if (other_possible_username_pair.first != form.username_value)
      usernames.push_back(other_possible_username_pair.first);
  }
  base::EraseIf(usernames, [](const base::string16& username) {
    return username.empty();
  });
  bool display_arrow = !usernames.empty();
  auto combobox = std::make_unique<views::EditableCombobox>(
      std::make_unique<ui::SimpleComboboxModel>(std::move(usernames)),
      /*filter_on_edit=*/false, /*show_on_empty=*/true,
      views::EditableCombobox::Type::kRegular, views::style::CONTEXT_BUTTON,
      views::style::STYLE_PRIMARY, display_arrow);
  combobox->SetText(form.username_value);
  combobox->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USERNAME_LABEL));
  // In case of long username, ensure that the beginning of value is visible.
  combobox->SelectRange(gfx::Range(0));
  return combobox;
}

// Creates an EditableCombobox from |PasswordForm.all_possible_passwords| or
// even just |PasswordForm.password_value|.
std::unique_ptr<views::EditableCombobox> CreatePasswordEditableCombobox(
    const autofill::PasswordForm& form,
    bool are_passwords_revealed) {
  DCHECK(!form.IsFederatedCredential());
  std::vector<base::string16> passwords =
      form.all_possible_passwords.empty()
          ? std::vector<base::string16>(/*n=*/1, form.password_value)
          : ToValues(form.all_possible_passwords);
  base::EraseIf(passwords, [](const base::string16& password) {
    return password.empty();
  });
  bool display_arrow = !passwords.empty();
  auto combobox = std::make_unique<views::EditableCombobox>(
      std::make_unique<ui::SimpleComboboxModel>(std::move(passwords)),
      /*filter_on_edit=*/false, /*show_on_empty=*/true,
      views::EditableCombobox::Type::kPassword, views::style::CONTEXT_BUTTON,
      STYLE_PRIMARY_MONOSPACED, display_arrow);
  combobox->SetText(form.password_value);
  combobox->RevealPasswords(are_passwords_revealed);
  combobox->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_PASSWORD_LABEL));
  return combobox;
}

std::unique_ptr<views::View> CreateHeaderImage(int image_id) {
  auto image_view = std::make_unique<NonAccessibleImageView>();
  image_view->SetImage(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(image_id));
  gfx::Size preferred_size = image_view->GetPreferredSize();
  if (preferred_size.width()) {
    float scale =
        static_cast<float>(ChromeLayoutProvider::Get()->GetDistanceMetric(
            DISTANCE_BUBBLE_PREFERRED_WIDTH)) /
        preferred_size.width();
    preferred_size = gfx::ScaleToRoundedSize(preferred_size, scale);
    image_view->SetImageSize(preferred_size);
  }
  return image_view;
}

}  // namespace

PasswordSaveUpdateView::PasswordSaveUpdateView(
    content::WebContents* web_contents,
    views::View* anchor_view,
    DisplayReason reason)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*auto_dismissable=*/false),
      controller_(
          PasswordsModelDelegateFromWebContents(web_contents),
          reason == AUTOMATIC
              ? PasswordBubbleControllerBase::DisplayReason::kAutomatic
              : PasswordBubbleControllerBase::DisplayReason::kUserAction),
      is_update_bubble_(controller_.state() ==
                        password_manager::ui::PENDING_PASSWORD_UPDATE_STATE),
      are_passwords_revealed_(
          controller_.are_passwords_revealed_when_bubble_is_opened()) {
  // If kEnablePasswordsAccountStorage is enabled, then
  // PasswordSaveUpdateWithAccountStoreView should be used instead of this
  // class.
  DCHECK(!base::FeatureList::IsEnabled(
      password_manager::features::kEnablePasswordsAccountStorage));

  DCHECK(controller_.state() == password_manager::ui::PENDING_PASSWORD_STATE ||
         controller_.state() ==
             password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
  const autofill::PasswordForm& password_form = controller_.pending_password();
  if (password_form.IsFederatedCredential()) {
    // The credential to be saved doesn't contain password but just the identity
    // provider (e.g. "Sign in with Google"). Thus, the layout is different.
    SetLayoutManager(std::make_unique<views::FillLayout>());
    std::pair<base::string16, base::string16> titles =
        GetCredentialLabelsForAccountChooser(password_form);
    CredentialsItemView* credential_view = new CredentialsItemView(
        this, titles.first, titles.second, &password_form,
        content::BrowserContext::GetDefaultStoragePartition(
            controller_.GetProfile())
            ->GetURLLoaderFactoryForBrowserProcess()
            .get());
    credential_view->SetEnabled(false);
    AddChildView(credential_view);
  } else {
    std::unique_ptr<views::EditableCombobox> username_dropdown =
        CreateUsernameEditableCombobox(password_form);
    username_dropdown->set_callback(base::BindRepeating(
        &PasswordSaveUpdateView::OnContentChanged, base::Unretained(this)));
    std::unique_ptr<views::EditableCombobox> password_dropdown =
        CreatePasswordEditableCombobox(password_form, are_passwords_revealed_);
    password_dropdown->set_callback(base::BindRepeating(
        &PasswordSaveUpdateView::OnContentChanged, base::Unretained(this)));

    std::unique_ptr<views::ToggleImageButton> password_view_button =
        CreatePasswordViewButton(this, are_passwords_revealed_);

    views::GridLayout* layout =
        SetLayoutManager(std::make_unique<views::GridLayout>());

    username_dropdown_ = username_dropdown.get();
    password_dropdown_ = password_dropdown.get();
    password_view_button_ = password_view_button.get();
    BuildCredentialRows(layout, std::move(username_dropdown),
                        std::move(password_dropdown),
                        std::move(password_view_button));
  }

  SetShowIcon(false);
  SetFootnoteView(CreateFooterView());
  SetCancelCallback(base::BindOnce(&PasswordSaveUpdateView::OnDialogCancelled,
                                   base::Unretained(this)));
  UpdateBubbleUIElements();
}

views::View* PasswordSaveUpdateView::GetUsernameTextfieldForTest() const {
  return username_dropdown_->GetTextfieldForTest();
}

PasswordSaveUpdateView::~PasswordSaveUpdateView() = default;

PasswordBubbleControllerBase* PasswordSaveUpdateView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase* PasswordSaveUpdateView::GetController()
    const {
  return &controller_;
}

bool PasswordSaveUpdateView::Accept() {
  UpdateUsernameAndPasswordInModel();
  controller_.OnSaveClicked();
  if (controller_.ReplaceToShowPromotionIfNeeded()) {
    ReplaceWithPromo();
    return false;  // Keep open.
  }
  return true;
}

void PasswordSaveUpdateView::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  DCHECK(sender == password_view_button_);
  TogglePasswordVisibility();
}

gfx::Size PasswordSaveUpdateView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

views::View* PasswordSaveUpdateView::GetInitiallyFocusedView() {
  if (username_dropdown_ && username_dropdown_->GetText().empty())
    return username_dropdown_;
  View* initial_view = PasswordBubbleViewBase::GetInitiallyFocusedView();
  // |initial_view| will normally be the 'Save' button, but in case it's not
  // focusable, we return nullptr so the Widget doesn't give focus to the next
  // focusable View, which would be |username_dropdown_|, and which would bring
  // up the menu without a user interaction. We only allow initial focus on
  // |username_dropdown_| above, when the text is empty.
  return (initial_view && initial_view->IsFocusable()) ? initial_view : nullptr;
}

bool PasswordSaveUpdateView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return button != ui::DIALOG_BUTTON_OK ||
         controller_.pending_password().IsFederatedCredential() ||
         !controller_.pending_password().password_value.empty();
}

gfx::ImageSkia PasswordSaveUpdateView::GetWindowIcon() {
  return gfx::ImageSkia();
}

bool PasswordSaveUpdateView::ShouldShowCloseButton() const {
  return true;
}

void PasswordSaveUpdateView::AddedToWidget() {
  static_cast<views::Label*>(GetBubbleFrameView()->title())
      ->SetAllowCharacterBreak(true);
}

void PasswordSaveUpdateView::OnThemeChanged() {
  PasswordBubbleViewBase::OnThemeChanged();
  int id = color_utils::IsDark(GetBubbleFrameView()->GetBackgroundColor())
               ? IDR_SAVE_PASSWORD_MULTI_DEVICE_DARK
               : IDR_SAVE_PASSWORD_MULTI_DEVICE;
  GetBubbleFrameView()->SetHeaderView(CreateHeaderImage(id));
  if (password_view_button_) {
    auto* theme = GetNativeTheme();
    const SkColor icon_color =
        theme->GetSystemColor(ui::NativeTheme::kColorId_DefaultIconColor);
    const SkColor disabled_icon_color =
        theme->GetSystemColor(ui::NativeTheme::kColorId_DisabledIconColor);
    views::SetImageFromVectorIconWithColor(password_view_button_, kEyeIcon,
                                           GetDefaultSizeOfVectorIcon(kEyeIcon),
                                           icon_color);
    views::SetToggledImageFromVectorIconWithColor(
        password_view_button_, kEyeCrossedIcon,
        GetDefaultSizeOfVectorIcon(kEyeCrossedIcon), icon_color,
        disabled_icon_color);
  }
}

void PasswordSaveUpdateView::TogglePasswordVisibility() {
  if (!are_passwords_revealed_ && !controller_.RevealPasswords())
    return;

  are_passwords_revealed_ = !are_passwords_revealed_;
  password_view_button_->SetToggled(are_passwords_revealed_);
  DCHECK(password_dropdown_);
  password_dropdown_->RevealPasswords(are_passwords_revealed_);
}

void PasswordSaveUpdateView::UpdateUsernameAndPasswordInModel() {
  if (!username_dropdown_ && !password_dropdown_)
    return;
  base::string16 new_username = controller_.pending_password().username_value;
  base::string16 new_password = controller_.pending_password().password_value;
  if (username_dropdown_) {
    new_username = username_dropdown_->GetText();
    base::TrimString(new_username, base::ASCIIToUTF16(" "), &new_username);
  }
  if (password_dropdown_)
    new_password = password_dropdown_->GetText();
  controller_.OnCredentialEdited(std::move(new_username),
                                 std::move(new_password));
}

void PasswordSaveUpdateView::ReplaceWithPromo() {
#if defined(OS_CHROMEOS)
  NOTREACHED();
#else
  RemoveAllChildViews(true);
  username_dropdown_ = nullptr;
  password_dropdown_ = nullptr;
  password_view_button_ = nullptr;
  SetLayoutManager(std::make_unique<views::FillLayout>());
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));
  if (controller_.state() == password_manager::ui::CHROME_SIGN_IN_PROMO_STATE) {
    sign_in_promo_ = new PasswordSignInPromoView(controller_.GetWebContents());
    AddChildView(sign_in_promo_);
  } else {
    NOTREACHED();
  }
  GetWidget()->UpdateWindowIcon();
  SetTitle(controller_.GetTitle());
  UpdateBubbleUIElements();
  DialogModelChanged();

  SizeToContents();
#endif  // defined(OS_CHROMEOS)
}

void PasswordSaveUpdateView::UpdateBubbleUIElements() {
  if (sign_in_promo_) {
    SetButtons(ui::DIALOG_BUTTON_NONE);
    return;
  }
  SetButtons((ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL));
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(controller_.IsCurrentStateUpdate()
                                    ? IDS_PASSWORD_MANAGER_UPDATE_BUTTON
                                    : IDS_PASSWORD_MANAGER_SAVE_BUTTON));
  SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(
          is_update_bubble_ ? IDS_PASSWORD_MANAGER_CANCEL_BUTTON
                            : IDS_PASSWORD_MANAGER_BUBBLE_BLOCKLIST_BUTTON));

  SetTitle(controller_.GetTitle());
}

std::unique_ptr<views::View> PasswordSaveUpdateView::CreateFooterView() {
  if (!controller_.ShouldShowFooter())
    return nullptr;
  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD_FOOTER),
      ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
      views::style::STYLE_SECONDARY);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

void PasswordSaveUpdateView::OnDialogCancelled() {
  UpdateUsernameAndPasswordInModel();
  if (is_update_bubble_)
    controller_.OnNopeUpdateClicked();
  else
    controller_.OnNeverForThisSiteClicked();
}

void PasswordSaveUpdateView::OnContentChanged() {
  bool is_update_state_before = controller_.IsCurrentStateUpdate();
  bool is_ok_button_enabled_before =
      IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK);
  UpdateUsernameAndPasswordInModel();
  // Maybe the buttons should be updated.
  if (is_update_state_before != controller_.IsCurrentStateUpdate() ||
      is_ok_button_enabled_before !=
          IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK)) {
    UpdateBubbleUIElements();
    DialogModelChanged();
    // TODO(ellyjones): This should not be necessary; DialogModelChanged()
    // implies a re-layout of the dialog.
    GetWidget()->GetRootView()->Layout();
  }
}
