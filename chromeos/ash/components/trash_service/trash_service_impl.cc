// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/trash_service/trash_service_impl.h"

#include <utility>

namespace chromeos::trash_service {

TrashImpl::TrashServiceImpl(
    mojo::PendingReceiver<mojom::TrashService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

TrashImpl::~TrashServiceImpl() = default;

}  // namespace chromeos::trash_service
