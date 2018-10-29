// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_TRANSPORT_HOVER_LIST_MODEL_H_
#define CHROME_BROWSER_UI_WEBAUTHN_TRANSPORT_HOVER_LIST_MODEL_H_

#include <stddef.h>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/ui/webauthn/hover_list_model.h"
#include "chrome/browser/webauthn/authenticator_transport.h"

class TransportHoverListModel : public HoverListModel {
 public:
  // Interface that the client should implement to learn when the user clicks on
  // views that observe the model.
  class Delegate {
   public:
    virtual void OnItemSelected(AuthenticatorTransport transport) = 0;
  };

  explicit TransportHoverListModel(
      std::vector<AuthenticatorTransport> transport_list,
      Delegate* delegate);
  ~TransportHoverListModel() override;

  // HoverListModel:
  bool ShouldShowPlaceholderForEmptyList() const override;
  base::string16 GetPlaceholderText() const override;
  const gfx::VectorIcon& GetPlaceholderIcon() const override;
  std::vector<int> GetItemTags() const override;
  base::string16 GetItemText(int item_tag) const override;
  const gfx::VectorIcon& GetItemIcon(int item_tag) const override;
  void OnListItemSelected(int item_tag) override;

 private:
  std::vector<AuthenticatorTransport> transport_list_;
  Delegate* const delegate_;  // Weak, may be nullptr.

  DISALLOW_COPY_AND_ASSIGN(TransportHoverListModel);
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_TRANSPORT_HOVER_LIST_MODEL_H_
