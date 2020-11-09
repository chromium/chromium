// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/index.h"

#include "base/optional.h"

namespace chromeos {
namespace local_search_service {

Index::Index(IndexId index_id, Backend backend) {}

Index::~Index() = default;

void Index::BindReceiver(mojo::PendingReceiver<mojom::Index> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace local_search_service
}  // namespace chromeos
