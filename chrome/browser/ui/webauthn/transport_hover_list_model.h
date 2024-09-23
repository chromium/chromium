// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_TRANSPORT_HOVER_LIST_MODEL_H_
#define CHROME_BROWSER_UI_WEBAUTHN_TRANSPORT_HOVER_LIST_MODEL_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webauthn/hover_list_model.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "ui/base/models/image_model.h"

class TransportHoverListModel
    : public HoverListModel,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  explicit TransportHoverListModel(
      AuthenticatorRequestDialogModel* dialog_model);

  TransportHoverListModel(AuthenticatorRequestDialogModel* dialog_model,
                          std::vector<int> mechanism_indices_to_display);

  TransportHoverListModel(const TransportHoverListModel&) = delete;
  TransportHoverListModel& operator=(const TransportHoverListModel&) = delete;

  ~TransportHoverListModel() override;

  // HoverListModel:
  std::vector<int> GetButtonTags() const override;
  std::u16string GetItemText(int item_tag) const override;
  std::u16string GetDescriptionText(int item_tag) const override;
  ui::ImageModel GetItemIcon(int item_tag) const override;
  bool IsButtonEnabled(int item_tag) const override;
  void OnListItemSelected(int item_tag) override;
  size_t GetPreferredItemCount() const override;

  // AuthenticatorRequestDialogModel:
  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override;

 private:
  base::ScopedObservation<AuthenticatorRequestDialogModel,
                          AuthenticatorRequestDialogModel::Observer>
      dialog_model_observation_{this};
  const std::vector<int> mechanism_indices_to_display_;
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_TRANSPORT_HOVER_LIST_MODEL_H_
