// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_TRANSPORT_HOVER_LIST_MODEL_H_
#define CHROME_BROWSER_UI_WEBAUTHN_TRANSPORT_HOVER_LIST_MODEL_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "chrome/browser/ui/webauthn/hover_list_model.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

namespace gfx {
struct VectorIcon;
}

class TransportHoverListModel : public HoverListModel {
 public:
  explicit TransportHoverListModel(
      base::span<const AuthenticatorRequestDialogModel::Mechanism> mechanisms);

  TransportHoverListModel(const TransportHoverListModel&) = delete;
  TransportHoverListModel& operator=(const TransportHoverListModel&) = delete;

  ~TransportHoverListModel() override;

  // HoverListModel:
  bool ShouldShowPlaceholderForEmptyList() const override;
  std::u16string GetPlaceholderText() const override;
  const gfx::VectorIcon* GetPlaceholderIcon() const override;
  std::vector<int> GetThrobberTags() const override;
  std::vector<int> GetButtonTags() const override;
  std::u16string GetItemText(int item_tag) const override;
  std::u16string GetDescriptionText(int item_tag) const override;
  const gfx::VectorIcon* GetItemIcon(int item_tag) const override;
  void OnListItemSelected(int item_tag) override;
  size_t GetPreferredItemCount() const override;
  bool StyleForTwoLines() const override;

 private:
  const base::span<const AuthenticatorRequestDialogModel::Mechanism>
      mechanisms_;
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_TRANSPORT_HOVER_LIST_MODEL_H_
