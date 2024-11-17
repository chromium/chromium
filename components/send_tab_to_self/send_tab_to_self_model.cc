// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "base/observer_list.h"

namespace send_tab_to_self {

SendTabToSelfModel::SendTabToSelfModel() = default;

SendTabToSelfModel::~SendTabToSelfModel() = default;

// Observer methods.
void SendTabToSelfModel::AddObserver(SendTabToSelfModelObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void SendTabToSelfModel::RemoveObserver(SendTabToSelfModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace send_tab_to_self
