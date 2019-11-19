// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_BLE_DEVICE_HOVER_LIST_MODEL_H_
#define CHROME_BROWSER_UI_WEBAUTHN_BLE_DEVICE_HOVER_LIST_MODEL_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/webauthn/hover_list_model.h"
#include "chrome/browser/webauthn/authenticator_list_observer.h"
#include "chrome/browser/webauthn/observable_authenticator_list.h"
#include "ui/gfx/vector_icon_types.h"

class BleDeviceHoverListModel : public HoverListModel,
                                public AuthenticatorListObserver {
 public:
  // Interface that the client should implement to learn when the user selects
  // an authenticator.
  class Delegate {
   public:
    virtual void OnItemSelected(base::StringPiece authenticator_id) = 0;
  };

  // |authenticator_list_| and |delegate_| must outlive |this|, and |delegate_|
  // may be nullptr.
  explicit BleDeviceHoverListModel(
      ObservableAuthenticatorList* authenticator_list,
      Delegate* delegate);
  ~BleDeviceHoverListModel() override;

 private:
  const AuthenticatorReference* GetAuthenticator(int tag) const;

  // HoverListModel:
  bool ShouldShowPlaceholderForEmptyList() const override;
  base::string16 GetPlaceholderText() const override;
  const gfx::VectorIcon* GetPlaceholderIcon() const override;
  base::string16 GetItemText(int item_tag) const override;
  base::string16 GetDescriptionText(int item_tag) const override;
  const gfx::VectorIcon* GetItemIcon(int item_tag) const override;
  std::vector<int> GetItemTags() const override;
  void OnListItemSelected(int item_tag) override;
  size_t GetPreferredItemCount() const override;
  bool StyleForTwoLines() const override;

  // AuthenticatorListObserver:
  void OnAuthenticatorAdded(
      const AuthenticatorReference& authenticator) override;
  void OnAuthenticatorRemoved(
      const AuthenticatorReference& removed_authenticator) override;
  // Invoked when Bluetooth authenticator already included in
  // |authenticator_list_| changes from being in non-pairable state to pairable
  // state.
  void OnAuthenticatorPairingModeChanged(
      const AuthenticatorReference& changed_authenticator) override;

  // Invoked when device address(and the corresponding authenticator id) of the
  // connected BLE authenticator changes due to authenticator's pairing mode
  // change.
  void OnAuthenticatorIdChanged(
      const AuthenticatorReference& changed_authenticator,
      base::StringPiece previous_id) override;

  ObservableAuthenticatorList* const authenticator_list_;
  Delegate* const delegate_;  // Weak, may be nullptr.

  // Map where key corresponds to the tags attached to elements in above
  // |authenticator_list_| and value is the corresponding authenticator id.
  // Elements in authenticator_tags_ are filtered and added to the view
  // component, but the size of the list should be sync with that of
  // |authenticator_list_|.
  std::map<int, std::string> authenticator_tags_;

  DISALLOW_COPY_AND_ASSIGN(BleDeviceHoverListModel);
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_BLE_DEVICE_HOVER_LIST_MODEL_H_
