// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/quick_pair_service.h"

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/quick_pair/public/mojom/fast_pair_data_parser.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

class QuickPairServiceTest : public testing::Test {
 public:
  void SetUp() override {
    receiver_ = service_.BindNewPipeAndPassReceiver();

    mojo::PendingRemote<mojom::FastPairDataParser> fast_pair_data_parser;
    mojo::PendingReceiver<mojom::FastPairDataParser>
        fast_pair_data_parser_receiver =
            fast_pair_data_parser.InitWithNewPipeAndPassReceiver();
    fast_pair_data_parser_.Bind(std::move(fast_pair_data_parser),
                                /*bind_task_runner=*/nullptr);

    quick_pair_service_ =
        std::make_unique<QuickPairService>(std::move(receiver_));
    quick_pair_service_->Connect(std::move(fast_pair_data_parser_receiver));
  }

  void TearDown() override { quick_pair_service_.reset(); }

 protected:
  mojo::PendingReceiver<mojom::QuickPairService> receiver_;
  mojo::SharedRemote<mojom::FastPairDataParser> data_parser_remote_;
  mojo::SharedRemote<mojom::FastPairDataParser> fast_pair_data_parser_;
  mojo::Remote<mojom::QuickPairService> service_;
  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<QuickPairService> quick_pair_service_;
};

TEST_F(QuickPairServiceTest, ConnectSuccess) {
  EXPECT_TRUE(quick_pair_service_->fast_pair_data_parser());
}

}  // namespace quick_pair
}  // namespace ash
