// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_LIST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_LIST_VIEW_H_

#include "base/functional/callback_forward.h"
#include "components/password_manager/core/browser/password_form.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/image_model.h"
#include "ui/views/layout/box_layout_view.h"

// A view that displays a list of credentials in a list view, together with an
// entry to navigate to the password manager. Used in the ManagePasswordsView.
class ManagePasswordsListView : public views::BoxLayoutView {
  METADATA_HEADER(ManagePasswordsListView, views::BoxLayoutView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTopView);

  // `credentials` is the list of credentials this view is displaying. `favicon`
  // is the icon to be displayed next to each row. `on_row_clicked_callback` is
  // invoked upon clicking a row in the list passing the credential that was
  // clicked as a param. `on_navigate_to_settings_clicked_callback` is a
  // callback that informs the embedder that the manage password entry has been
  // clicked.
  ManagePasswordsListView(
      base::span<std::unique_ptr<password_manager::PasswordForm> const>
          credentials,
      ui::ImageModel favicon,
      base::RepeatingCallback<void(password_manager::PasswordForm)>
          on_row_clicked_callback,
      base::RepeatingClosure on_navigate_to_settings_clicked_callback,
      bool is_account_storage_available);

  ManagePasswordsListView(const ManagePasswordsListView&) = delete;
  ManagePasswordsListView& operator=(const ManagePasswordsListView&) = delete;

  ~ManagePasswordsListView() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_LIST_VIEW_H_
