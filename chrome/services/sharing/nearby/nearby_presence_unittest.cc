// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_presence.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::nearby::presence {

class NearbyPresenceTest : public testing::Test {
 public:
  NearbyPresenceTest() {
    nearby_presence_ = std::make_unique<NearbyPresence>(
        remote_.BindNewPipeAndPassReceiver(),
        base::BindOnce(&NearbyPresenceTest::OnDisconnect,
                       base::Unretained(this)));
  }
  ~NearbyPresenceTest() override = default;

  void OnDisconnect() {}

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<NearbyPresence> nearby_presence_;
  mojo::Remote<mojom::NearbyPresence> remote_;
};

TEST_F(NearbyPresenceTest, CreatePresenceMojom) {
  EXPECT_TRUE(nearby_presence_);
}

}  // namespace ash::nearby::presence
