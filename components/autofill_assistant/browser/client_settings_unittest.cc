// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/mock_client.h"

namespace autofill_assistant {

namespace {

class ClientSettingsTest : public testing::Test {
 protected:
  ClientSettingsTest() {}
  ~ClientSettingsTest() override {}
};

TEST_F(ClientSettingsTest, CheckLegacyOverlayImage) {
  ClientSettingsProto proto;
  proto.mutable_overlay_image()->set_image_url(
      "https://www.example.com/favicon.ico");
  proto.mutable_overlay_image()->mutable_image_size()->set_dp(32);

  ClientSettings settings;
  settings.UpdateFromProto(proto);

  ASSERT_TRUE(settings.overlay_image.has_value());
  EXPECT_EQ(settings.overlay_image->image_drawable().bitmap().url(),
            "https://www.example.com/favicon.ico");
}

}  // namespace
}  // namespace autofill_assistant