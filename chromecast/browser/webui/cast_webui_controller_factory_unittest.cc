// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webui/cast_webui_controller_factory.h"

#include <memory>
#include <string>
#include <vector>

#include "chromecast/browser/webui/constants.h"
#include "base/test/task_environment.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromecast {

class CastWebUiControllerFactoryTest : public testing::Test {
 protected:
  CastWebUiControllerFactoryTest() {
    auto receiver = client_.InitWithNewPipeAndPassReceiver();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  mojo::PendingRemote<mojom::WebUiClient> client_;
};

TEST_F(CastWebUiControllerFactoryTest, AllowsKnownHostAndCorrectScheme) {
  CastWebUiControllerFactory factory(std::move(client_), {kCastWebUIHomeHost});
  EXPECT_NE(factory.GetWebUIType(nullptr, GURL("chrome://home")),
            content::WebUI::kNoWebUI);
}

TEST_F(CastWebUiControllerFactoryTest, IgnoresUnknownHost) {
  CastWebUiControllerFactory factory(std::move(client_), {"pwn"});
  EXPECT_EQ(factory.GetWebUIType(nullptr, GURL("chrome://pwn")),
            content::WebUI::kNoWebUI);
}

TEST_F(CastWebUiControllerFactoryTest, IgnoresWrongScheme) {
  CastWebUiControllerFactory factory(std::move(client_), {kCastWebUIHomeHost});
  EXPECT_EQ(factory.GetWebUIType(nullptr, GURL("http://home")),
            content::WebUI::kNoWebUI);
}

}  // namespace chromecast
