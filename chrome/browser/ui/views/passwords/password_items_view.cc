// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_items_view.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <utility>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/strong_alias.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/passwords/bubble_controllers/items_bubble_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/color_tracking_icon_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/style/typography.h"

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
  if (std::any_of(credentials.begin(), credentials.end(),
                  [](const password_manager::PasswordForm& form) {
                    return form.in_store ==
                           password_manager::PasswordForm::Store::kAccountStore;
                  })) {
    return MULTI_STORE_PASSWORD_COLUMN_SET;
  }
  return PASSWORD_COLUMN_SET;
}

void BuildColumnSet(views::GridLayout* layout,
                    PasswordItemsViewColumnSetType type_id) {
  DCHECK(!layout->GetColumnSet(type_id));
  views::ColumnSet* column_set = layout->AddColumnSet(type_id);
  // Passwords are split 60/40 (6:4) as the username is more important
  // than obscured password digits. Otherwise two columns are 50/50 (1:1).
  constexpr float kFirstColumnWeight = 60.0f;
  constexpr float kSecondColumnWeight = 40.0f;
  const int between_column_padding =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  // Add favicon column
  if (type_id == PASSWORD_COLUMN_SET ||
      type_id == MULTI_STORE_PASSWORD_COLUMN_SET) {
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                          views::GridLayout::kFixedSize,
                          views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
    column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                 between_column_padding);
  }

  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        kFirstColumnWeight,
                        views::GridLayout::ColumnSize::kFixed, 0, 0);

  if (type_id == PASSWORD_COLUMN_SET ||
      type_id == MULTI_STORE_PASSWORD_COLUMN_SET) {
    column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                 between_column_padding);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                          kSecondColumnWeight,
                          views::GridLayout::ColumnSize::kFixed, 0, 0);
  }
  if (type_id == MULTI_STORE_PASSWORD_COLUMN_SET) {
    // All rows show a store indicator or leave the space blank.
    column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                 between_column_padding);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                          views::GridLayout::kFixedSize,
                          views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
    // Add a column for the vertical bar.
    column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                 between_column_padding);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                          views::GridLayout::kFixedSize,
                          views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  }
  // All rows end with a trailing column for the undo/trash button.
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                               between_column_padding);
  column_set->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
}

void StartRow(views::GridLayout* layout,
              PasswordItemsViewColumnSetType type_id) {
  if (!layout->GetColumnSet(type_id))
    BuildColumnSet(layout, type_id);
  layout->StartRow(views::GridLayout::kFixedSize, type_id);
}

}  // namespace

// An entry for each credential. Relays delete/undo actions associated with
// this password row to parent dialog.
class PasswordItemsView::PasswordRow {
 public:
  PasswordRow(PasswordItemsView* parent,
              const password_manager::PasswordForm* password_form);

  void AddToLayout(views::GridLayout* layout,
                   PasswordItemsViewColumnSetType type_id);

 private:
  void AddUndoRow(views::GridLayout* layout);
  void AddPasswordRow(views::GridLayout* layout,
                      PasswordItemsViewColumnSetType type_id);

  void DeleteButtonPressed();
  void UndoButtonPressed();

  PasswordItemsView* const parent_;
  const password_manager::PasswordForm* const password_form_;
  bool deleted_ = false;

  DISALLOW_COPY_AND_ASSIGN(PasswordRow);
};

PasswordItemsView::PasswordRow::PasswordRow(
    PasswordItemsView* parent,
    const password_manager::PasswordForm* password_form)
    : parent_(parent), password_form_(password_form) {}

void PasswordItemsView::PasswordRow::AddToLayout(
    views::GridLayout* layout,
    PasswordItemsViewColumnSetType type_id) {
  if (deleted_)
    AddUndoRow(layout);
  else
    AddPasswordRow(layout, type_id);
}

void PasswordItemsView::PasswordRow::AddUndoRow(views::GridLayout* layout) {
  StartRow(layout, UNDO_COLUMN_SET);
  layout
      ->AddView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_DELETED),
          views::style::CONTEXT_DIALOG_BODY_TEXT))
      ->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  auto* undo_button = layout->AddView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&PasswordRow::UndoButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_UNDO)));
  undo_button->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_MANAGE_PASSWORDS_UNDO_TOOLTIP, GetDisplayUsername(*password_form_)));
}

void PasswordItemsView::PasswordRow::AddPasswordRow(
    views::GridLayout* layout,
    PasswordItemsViewColumnSetType type_id) {
  StartRow(layout, type_id);

  if (parent_->favicon_.IsEmpty()) {
    // Use a globe fallback until the actual favicon is loaded.
    layout->AddView(std::make_unique<views::ColorTrackingIconView>(
        kGlobeIcon, gfx::kFaviconSize));
  } else {
    layout->AddView(std::make_unique<views::ImageView>())
        ->SetImage(parent_->favicon_.AsImageSkia());
  }

  layout->AddView(CreateUsernameLabel(*password_form_));
  layout->AddView(CreatePasswordLabel(*password_form_));

  if (type_id == MULTI_STORE_PASSWORD_COLUMN_SET) {
    if (password_form_->in_store ==
        password_manager::PasswordForm::Store::kAccountStore) {
      auto* image_view = layout->AddView(std::make_unique<views::ImageView>());
      image_view->SetImage(gfx::CreateVectorIcon(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
          kGoogleGLogoIcon,
#else
          vector_icons::kSyncIcon,
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
          gfx::kFaviconSize, gfx::kPlaceholderColor));
      image_view->SetAccessibleName(l10n_util::GetStringUTF16(
          IDS_MANAGE_PASSWORDS_ACCOUNT_STORE_ICON_DESCRIPTION));
    } else {
      layout->SkipColumns(1);
    }

    auto* separator = layout->AddView(std::make_unique<views::Separator>());
    separator->SetFocusBehavior(
        LocationBarBubbleDelegateView::FocusBehavior::NEVER);
    separator->SetPreferredHeight(views::style::GetLineHeight(
        views::style::CONTEXT_MENU, views::style::STYLE_SECONDARY));
    separator->SetCanProcessEventsWithinSubtree(false);
  }

  auto* delete_button =
      layout->AddView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&PasswordRow::DeleteButtonPressed,
                              base::Unretained(this)),
          kTrashCanIcon));
  delete_button->SetTooltipText(l10n_util::GetStringFUTF16(
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

  if (controller_.local_credentials().empty()) {
    // A LayoutManager is required for GetHeightForWidth() even without
    // content.
    SetLayoutManager(std::make_unique<views::FillLayout>());
  } else {
    // The request is cancelled when the |controller_| is destructed.
    // |controller_| has the same life time as |this| and hence it's safe to use
    // base::Unretained(this).
    controller_.RequestFavicon(base::BindOnce(
        &PasswordItemsView::OnFaviconReady, base::Unretained(this)));
    for (auto& password_form : controller_.local_credentials()) {
      password_rows_.push_back(
          std::make_unique<PasswordRow>(this, &password_form));
    }

    RecreateLayout();
  }
}

PasswordItemsView::~PasswordItemsView() = default;

PasswordBubbleControllerBase* PasswordItemsView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase* PasswordItemsView::GetController() const {
  return &controller_;
}

void PasswordItemsView::RecreateLayout() {
  // This method should only be used when we have password rows, otherwise the
  // dialog should only show the no-passwords title and doesn't need to be
  // recreated.
  DCHECK(!controller_.local_credentials().empty());

  RemoveAllChildViews(true);

  views::GridLayout* grid_layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  const int vertical_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_CONTROL_LIST_VERTICAL);
  bool first_row = true;
  PasswordItemsViewColumnSetType row_column_set_type =
      InferColumnSetTypeFromCredentials(controller_.local_credentials());
  for (auto& row : password_rows_) {
    if (!first_row)
      grid_layout->AddPaddingRow(views::GridLayout::kFixedSize,
                                 vertical_padding);

    row->AddToLayout(grid_layout, row_column_set_type);
    first_row = false;
  }

  PreferredSizeChanged();
  if (GetBubbleFrameView())
    SizeToContents();
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
