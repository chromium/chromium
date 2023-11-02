// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_QUICK_PAIR_QUICK_PAIR_SERVICE_H_
#define CHROMEOS_ASH_SERVICES_QUICK_PAIR_QUICK_PAIR_SERVICE_H_

#include <memory>

#include "chromeos/ash/services/quick_pair/public/mojom/fast_pair_data_parser.mojom-forward.h"
#include "chromeos/ash/services/quick_pair/public/mojom/quick_pair_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {
namespace quick_pair {

class FastPairDataParser;

// Class which implements the QuickPairService mojo interface to provide
// functionality to the Quick Pair system which needs to run in a utility
// process, e.g. parsing untrusted bytes.
class QuickPairService : public mojom::QuickPairService {
 public:
  explicit QuickPairService(
      mojo::PendingReceiver<mojom::QuickPairService> receiver);
  QuickPairService(const QuickPairService&) = delete;
  QuickPairService& operator=(QuickPairService&) = delete;
  ~QuickPairService() override;

  // mojom::QuickPairService:
  void Connect(mojo::PendingReceiver<mojom::FastPairDataParser>
                   fast_pair_data_parser) override;

  FastPairDataParser* fast_pair_data_parser() {
    return fast_pair_data_parser_.get();
  }

 private:
  mojo::Receiver<mojom::QuickPairService> receiver_;
  std::unique_ptr<FastPairDataParser> fast_pair_data_parser_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_QUICK_PAIR_QUICK_PAIR_SERVICE_H_
