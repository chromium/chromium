// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_items_view.h"

#include <memory>
#include <numeric>
#include <utility>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/strong_alias.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/passwords/bubble_controllers/items_bubble_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {

// Column set identifiers for displaying or undoing removal of credentials.
// All of them allocate space differently.
enum PasswordItemsViewColumnSetType {
  // Contains three columns for credential pair and a delete button.
  PASSWORD_COLUMN_SET,
  // Like PASSWORD_COLUMN_SET plus a column for an icon indicating the store,
  // and a vertical bar before the delete button.
  MULTI_STORE_PASSWORD_COLUMN_SET,
  // Contains two columns for text and an undo button.
  UNDO_COLUMN_SET
};

PasswordItemsViewColumnSetType InferColumnSetTypeFromCredentials(
    const std::vector<password_manager::PasswordForm>& credentials) {
  if (base::Contains(credentials,
                     password_manager::PasswordForm::Store::kAccountStore,
                     &password_manager::PasswordForm::in_store)) {
    return MULTI_STORE_PASSWORD_COLUMN_SET;
  }
  return PASSWORD_COLUMN_SET;
}

void BuildColumnSet(views::TableLayout* table_layout,
                    PasswordItemsViewColumnSetType type_id) {
  // Passwords are split 60/40 (6:4) as the username is more important
  // than obscured password digits. Otherwise two columns are 50/50 (1:1).
  constexpr float kFirstColumnWeight = 60.0f;
  constexpr float kSecondColumnWeight = 40.0f;
  const int between_column_padding =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  table_layout
      // favicon column
      ->AddColumn(views::LayoutAlignment::kStretch,
                  views::LayoutAlignment::kStretch,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, between_column_padding)
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kStretch, kFirstColumnWeight,
                 views::TableLayout::ColumnSize::kFixed, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, between_column_padding)
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kStretch, kSecondColumnWeight,
                 views::TableLayout::ColumnSize::kFixed, 0, 0);

  if (type_id == MULTI_STORE_PASSWORD_COLUMN_SET) {
    // All rows show a store indicator or leave the space blank.
    table_layout
        ->AddPaddingColumn(views::TableLayout::kFixedSize,
                           between_column_padding)
        .AddColumn(views::LayoutAlignment::kStretch,
                   views::LayoutAlignment::kStretch,
                   views::TableLayout::kFixedSize,
                   views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
        // Add a column for the vertical bar.
        .AddPaddingColumn(views::TableLayout::kFixedSize,
                          between_column_padding)
        .AddColumn(views::LayoutAlignment::kStretch,
                   views::LayoutAlignment::kCenter,
                   views::TableLayout::kFixedSize,
                   views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
  }
  // All rows end with a trailing column for the undo/trash button.
  table_layout
      ->AddPaddingColumn(views::TableLayout::kFixedSize, between_column_padding)
      .AddColumn(views::LayoutAlignment::kEnd, views::LayoutAlignment::kStretch,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
}

}  // namespace

// An entry for each credential. Relays delete/undo actions associated with
// this password row to parent dialog.
class PasswordItemsView::PasswordRow {
 public:
  PasswordRow(PasswordItemsView* parent,
              const password_manager::PasswordForm* password_form);

  PasswordRow(const PasswordRow&) = delete;
  PasswordRow& operator=(const PasswordRow&) = delete;

  void AddToLayout(views::TableLayout* table_layout,
                   PasswordItemsViewColumnSetType type_id);

 private:
  void AddUndoRow(views::TableLayout* table_layout,
                  PasswordItemsViewColumnSetType type_id);
  void AddPasswordRow(views::TableLayout* table_layout,
                      PasswordItemsViewColumnSetType type_id);

  void DeleteButtonPressed();
  void UndoButtonPressed();

  const raw_ptr<PasswordItemsView> parent_;
  const raw_ptr<const password_manager::PasswordForm, DanglingUntriaged>
      password_form_;
  bool deleted_ = false;
};

PasswordItemsView::PasswordRow::PasswordRow(
    PasswordItemsView* parent,
    const password_manager::PasswordForm* password_form)
    : parent_(parent), password_form_(password_form) {}

void PasswordItemsView::PasswordRow::AddToLayout(
    views::TableLayout* table_layout,
    PasswordItemsViewColumnSetType type_id) {
  if (deleted_)
    AddUndoRow(table_layout, type_id);
  else
    AddPasswordRow(table_layout, type_id);
}

void PasswordItemsView::PasswordRow::AddUndoRow(
    views::TableLayout* table_layout,
    PasswordItemsViewColumnSetType type_id) {
  table_layout->AddRows(1, views::TableLayout::kFixedSize);
  auto* undo_label = parent_->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_DELETED),
      views::style::CONTEXT_DIALOG_BODY_TEXT));
  undo_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  undo_label->SetProperty(
      views::kTableColAndRowSpanKey,
      gfx::Size(type_id == MULTI_STORE_PASSWORD_COLUMN_SET ? 9 : 5, 1));
  auto* undo_button =
      parent_->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(&PasswordRow::UndoButtonPressed,
                              base::Unretained(this)),
          l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_UNDO)));
  undo_button->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_MANAGE_PASSWORDS_UNDO_TOOLTIP, GetDisplayUsername(*password_form_)));
}

void PasswordItemsView::PasswordRow::AddPasswordRow(
    views::TableLayout* table_layout,
    PasswordItemsViewColumnSetType type_id) {
  table_layout->AddRows(1, views::TableLayout::kFixedSize);
  if (parent_->favicon_.IsEmpty()) {
    // Use a globe fallback until the actual favicon is loaded.
    parent_->AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            kGlobeIcon, ui::kColorIcon, gfx::kFaviconSize)));
  } else {
    parent_->AddChildView(std::make_unique<views::ImageView>())
        ->SetImage(parent_->favicon_.AsImageSkia());
  }

  parent_->AddChildView(CreateUsernameLabel(*password_form_));
  parent_->AddChildView(CreatePasswordLabel(*password_form_));

  if (type_id == MULTI_STORE_PASSWORD_COLUMN_SET) {
    if (password_form_->in_store ==
        password_manager::PasswordForm::Store::kAccountStore) {
      auto* image_view =
          parent_->AddChildView(std::make_unique<views::ImageView>());
      image_view->SetImage(ui::ImageModel::FromVectorIcon(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
          vector_icons::kGoogleGLogoIcon,
#else
          vector_icons::kSyncIcon,
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
          gfx::kPlaceholderColor, gfx::kFaviconSize));
      image_view->SetAccessibleName(l10n_util::GetStringUTF16(
          IDS_MANAGE_PASSWORDS_ACCOUNT_STORE_ICON_DESCRIPTION));
    } else {
      parent_->AddChildView(std::make_unique<views::View>());
    }

    auto* separator =
        parent_->AddChildView(std::make_unique<views::Separator>());
    separator->SetFocusBehavior(
        LocationBarBubbleDelegateView::FocusBehavior::NEVER);
    separator->SetPreferredLength(views::style::GetLineHeight(
        views::style::CONTEXT_MENU, views::style::STYLE_SECONDARY));
    separator->SetCanProcessEventsWithinSubtree(false);
  }

  parent_
      ->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&PasswordRow::DeleteButtonPressed,
                              base::Unretained(this)),
          kTrashCanIcon))
      ->SetTooltipText(l10n_util::GetStringFUTF16(
          IDS_MANAGE_PASSWORDS_DELETE, GetDisplayUsername(*password_form_)));
}

void PasswordItemsView::PasswordRow::DeleteButtonPressed() {
  deleted_ = true;
  parent_->NotifyPasswordFormAction(
      *password_form_,
      PasswordBubbleControllerBase::PasswordAction::kRemovePassword);
}

void PasswordItemsView::PasswordRow::UndoButtonPressed() {
  deleted_ = false;
  parent_->NotifyPasswordFormAction(
      *password_form_,
      PasswordBubbleControllerBase::PasswordAction::kAddPassword);
}

PasswordItemsView::PasswordItemsView(content::WebContents* web_contents,
                                     views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/true),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  SetButtons(ui::DIALOG_BUTTON_OK);
  SetExtraView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          [](PasswordItemsView* items) {
            items->controller_.OnManageClicked(
                password_manager::ManagePasswordsReferrer::
                    kManagePasswordsBubble);
            items->CloseBubble();
          },
          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS_BUTTON)));

  SetFootnoteView(CreateFooterView());

  auto& local_credentials = controller_.local_credentials();

  if (local_credentials.empty()) {
    // A LayoutManager is required for GetHeightForWidth() even without
    // content.
    SetUseDefaultFillLayout(true);
  } else {
    // The request is cancelled when the |controller_| is destructed.
    // |controller_| has the same life time as |this| and hence it's safe to use
    // base::Unretained(this).
    controller_.RequestFavicon(base::BindOnce(
        &PasswordItemsView::OnFaviconReady, base::Unretained(this)));
    for (auto& password_form : local_credentials) {
      password_rows_.push_back(
          std::make_unique<PasswordRow>(this, &password_form));
    }
    RecreateLayout();
  }

  SetShowIcon(true);
}

PasswordItemsView::~PasswordItemsView() = default;

PasswordBubbleControllerBase* PasswordItemsView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase* PasswordItemsView::GetController() const {
  return &controller_;
}

ui::ImageModel PasswordItemsView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                        ui::kColorIcon);
}

void PasswordItemsView::RecreateLayout() {
  // This method should only be used when we have password rows, otherwise the
  // dialog should only show the no-passwords title and doesn't need to be
  // recreated.
  DCHECK(!controller_.local_credentials().empty());

  RemoveAllChildViews();

  auto* table_layout = SetLayoutManager(std::make_unique<views::TableLayout>());
  BuildColumnSet(table_layout, InferColumnSetTypeFromCredentials(
                                   controller_.local_credentials()));

  const int vertical_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_CONTROL_LIST_VERTICAL);
  bool first = true;
  for (auto& row : password_rows_) {
    if (!first) {
      table_layout->AddPaddingRow(views::TableLayout::kFixedSize,
                                  vertical_padding);
    }
    row->AddToLayout(table_layout, InferColumnSetTypeFromCredentials(
                                       controller_.local_credentials()));
    first = false;
  }

  PreferredSizeChanged();
  if (GetBubbleFrameView())
    SizeToContents();
}

std::unique_ptr<views::View> PasswordItemsView::CreateFooterView() {
  base::RepeatingClosure open_password_manager_closure = base::BindRepeating(
      [](PasswordItemsView* dialog) {
        dialog->controller_.OnGooglePasswordManagerLinkClicked();
      },
      base::Unretained(this));

  switch (controller_.GetPasswordSyncState()) {
    case password_manager::SyncState::kNotSyncing:
      return CreateGooglePasswordManagerLabel(
          /*text_message_id=*/
          IDS_PASSWORD_BUBBLES_FOOTER_SAVING_ON_DEVICE,
          /*link_message_id=*/
          IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SAVING_ON_DEVICE,
          open_password_manager_closure);
    case password_manager::SyncState::kSyncingNormalEncryption:
    case password_manager::SyncState::kSyncingWithCustomPassphrase:
      return CreateGooglePasswordManagerLabel(
          /*text_message_id=*/
          IDS_PASSWORD_BUBBLES_FOOTER_SYNCED_TO_ACCOUNT,
          /*link_message_id=*/
          IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT,
          controller_.GetPrimaryAccountEmail(), open_password_manager_closure);
    case password_manager::SyncState::kAccountPasswordsActiveNormalEncryption:
      // Account store users have a special footer in the management bubble
      // since they might have a mix of synced and non-synced passwords.
      return CreateGooglePasswordManagerLabel(
          /*text_message_id=*/
          IDS_PASSWORD_MANAGEMENT_BUBBLE_FOOTER_ACCOUNT_STORE_USERS,
          /*link_message_id=*/
          IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT,
          open_password_manager_closure);
  }
}

void PasswordItemsView::NotifyPasswordFormAction(
    const password_manager::PasswordForm& password_form,
    PasswordBubbleControllerBase::PasswordAction action) {
  RecreateLayout();
  // After the view is consistent, notify the model that the password needs to
  // be updated (either removed or put back into the store, as appropriate.
  controller_.OnPasswordAction(password_form, action);
}

void PasswordItemsView::OnFaviconReady(const gfx::Image& favicon) {
  if (!favicon.IsEmpty()) {
    favicon_ = favicon;
    RecreateLayout();
  }
}
