// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_ACCOUNT_HOVER_LIST_MODEL_H_
#define CHROME_BROWSER_UI_WEBAUTHN_ACCOUNT_HOVER_LIST_MODEL_H_

#include <stddef.h>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webauthn/hover_list_model.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "ui/base/models/image_model.h"

struct AuthenticatorRequestDialogModel;

class AccountHoverListModel : public HoverListModel {
 public:
  // Interface that the client should implement to learn when the user clicks on
  // views that observe the model.
  class Delegate {
   public:
    virtual void CredentialSelected(size_t index) = 0;
  };

  AccountHoverListModel(AuthenticatorRequestDialogModel* dialog_model,
                        Delegate* delegate);

  AccountHoverListModel(const AccountHoverListModel&) = delete;
  AccountHoverListModel& operator=(const AccountHoverListModel&) = delete;

  ~AccountHoverListModel() override;

  // HoverListModel:
  std::vector<int> GetButtonTags() const override;
  std::u16string GetItemText(int item_tag) const override;
  std::u16string GetDescriptionText(int item_tag) const override;
  ui::ImageModel GetItemIcon(int item_tag) const override;
  bool IsButtonEnabled(int item_tag) const override;
  void OnListItemSelected(int item_tag) override;
  size_t GetPreferredItemCount() const override;

 private:
  struct Item {
    Item(std::u16string text,
         std::u16string description,
         ui::ImageModel icon,
         bool enabled);
    Item(const Item&) = delete;
    Item(Item&&);
    Item& operator=(const Item&) = delete;
    Item& operator=(Item&&);
    ~Item();

    std::u16string text;
    std::u16string description;
    ui::ImageModel icon;
    bool enabled;
  };

  std::vector<Item> items_;
  const raw_ptr<Delegate> delegate_;
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_ACCOUNT_HOVER_LIST_MODEL_H_
