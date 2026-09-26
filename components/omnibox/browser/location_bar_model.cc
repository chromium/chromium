// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/location_bar_model.h"

DEFINE_USER_DATA(LocationBarModel);

LocationBarModel::LocationBarModel() = default;

LocationBarModel::LocationBarModel(ui::UnownedUserDataHost& host)
    : host_(&host), previous_model_(Get(host)) {
  if (previous_model_) {
    previous_model_->next_model_ = this;
    previous_model_->scoped_unowned_user_data_.reset();
  }
  scoped_unowned_user_data_.emplace(host, *this);
}

LocationBarModel::~LocationBarModel() {
  scoped_unowned_user_data_.reset();
  if (next_model_) {
    next_model_->previous_model_ = previous_model_;
  }
  if (previous_model_) {
    previous_model_->next_model_ = next_model_;
    if (!next_model_ && host_) {
      previous_model_->scoped_unowned_user_data_.emplace(*host_,
                                                         *previous_model_);
    }
  }
}
