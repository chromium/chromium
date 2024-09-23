// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/data_controls_dialog.h"

#include "base/check_op.h"
#include "base/functional/callback.h"

namespace data_controls {

DataControlsDialog::~DataControlsDialog() = default;

DataControlsDialog::DataControlsDialog(
    Type type,
    base::OnceCallback<void(bool bypassed)> callback)
    : type_(type) {
  if (callback) {
    callbacks_.push_back(std::move(callback));
  }
}

DataControlsDialog::Type DataControlsDialog::type() const {
  return type_;
}

void DataControlsDialog::AddCallback(
    base::OnceCallback<void(bool bypassed)> callback) {
  callbacks_.push_back(std::move(callback));
}

void DataControlsDialog::ClearCallbacks() {
  callbacks_.clear();
}

void DataControlsDialog::OnDialogButtonClicked(bool bypassed) {
  for (auto& callback : callbacks_) {
    if (callback) {
      std::move(callback).Run(bypassed);
    }
  }
  callbacks_.clear();
}

}  // namespace data_controls
