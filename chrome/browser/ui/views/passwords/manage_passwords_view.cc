// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_view.h"

#include <memory>
#include <optional>
#include <utility>

#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/page_switcher_view.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

ManagePasswordsView::ManagePasswordsView(content::WebContents* web_contents,
                                         views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/true),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  DCHECK(base::FeatureList::IsEnabled(
      password_manager::features::kRevampedPasswordManagementBubble));
  SetButtons(ui::DIALOG_BUTTON_NONE);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  page_container_ = AddChildView(
      std::make_unique<PageSwitcherView>(CreatePasswordListView()));

  SetFootnoteView(CreateFooterView());
}

ManagePasswordsView::~ManagePasswordsView() = default;

PasswordBubbleControllerBase* ManagePasswordsView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase* ManagePasswordsView::GetController() const {
  return &controller_;
}

ui::ImageModel ManagePasswordsView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                        ui::kColorIcon);
}

void ManagePasswordsView::AddedToWidget() {
  // Since PasswordBubbleViewBase creates the bubble using
  // BubbleDialogDelegateView::CreateBubble() *after* the construction of the
  // ManagePasswordsView, the title view cannot be set in the constructor.
  GetBubbleFrameView()->SetTitleView(CreatePasswordListTitleView());
}

std::unique_ptr<views::View> ManagePasswordsView::CreatePasswordListTitleView()
    const {
  const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  auto header = std::make_unique<views::BoxLayoutView>();
  // Set the space between the icon and title similar to the default behavior in
  // BubbleFrameView::Layout().
  header->SetBetweenChildSpacing(
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG_TITLE).left());
  header->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          GooglePasswordManagerVectorIcon(), ui::kColorIcon,
          layout_provider->GetDistanceMetric(
              DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE))));
  // TODO(crbug.com/1382017): refactor to use the title provided by the
  // controller instead.
  header->AddChildView(views::BubbleFrameView::CreateDefaultTitleLabel(
      u"Saved passwords for this site"));
  return header;
}

std::unique_ptr<views::View> ManagePasswordsView::CreatePasswordListView()
    const {
  auto container_view = std::make_unique<views::BoxLayoutView>();
  container_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  // TODO(crbug.com/1382017): List all saved passwords here.
  return container_view;
}

std::unique_ptr<views::View> ManagePasswordsView::CreateFooterView() {
  base::RepeatingClosure open_password_manager_closure = base::BindRepeating(
      [](ManagePasswordsView* dialog) {
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
