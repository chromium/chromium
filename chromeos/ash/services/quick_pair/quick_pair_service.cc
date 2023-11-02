// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/quick_pair_service.h"

#include <memory>

#include "chromeos/ash/services/quick_pair/fast_pair_data_parser.h"
#include "chromeos/ash/services/quick_pair/public/mojom/fast_pair_data_parser.mojom-forward.h"
#include "chromeos/ash/services/quick_pair/public/mojom/quick_pair_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {
namespace quick_pair {

QuickPairService::QuickPairService(
    mojo::PendingReceiver<mojom::QuickPairService> receiver)
    : receiver_(this, std::move(receiver)) {}

QuickPairService::~QuickPairService() = default;

void QuickPairService::Connect(
    mojo::PendingReceiver<mojom::FastPairDataParser> fast_pair_data_parser) {
  DCHECK(!fast_pair_data_parser_);
  fast_pair_data_parser_ =
      std::make_unique<FastPairDataParser>(std::move(fast_pair_data_parser));
}

}  // namespace quick_pair
}  // namespace ash
