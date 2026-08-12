// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/welcome_handler.h"

#include <memory>
#include <optional>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/webui/intro/welcome.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::StrictMock;

class WelcomeHandlerTest : public testing::Test {
 public:
  WelcomeHandlerTest() = default;
  ~WelcomeHandlerTest() override = default;

  void SetUp() override {
    handler_ = std::make_unique<WelcomeHandler>(
        mock_callback_.Get(), remote_.BindNewPipeAndPassReceiver());
  }

  mojo::Remote<intro::mojom::WelcomePageHandler>& remote() { return remote_; }
  base::MockCallback<base::OnceClosure>& mock_callback() {
    return mock_callback_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  StrictMock<base::MockCallback<base::OnceClosure>> mock_callback_;
  mojo::Remote<intro::mojom::WelcomePageHandler> remote_;
  std::unique_ptr<WelcomeHandler> handler_;
};

TEST_F(WelcomeHandlerTest, Continue) {
  EXPECT_CALL(mock_callback(), Run());

  remote()->Continue(/*is_uma_opt_in=*/true, /*is_default_browser=*/false);
  remote().FlushForTesting();
}

TEST_F(WelcomeHandlerTest, ContinueCalledMultipleTimes) {
  EXPECT_CALL(mock_callback(), Run());

  remote()->Continue(/*is_uma_opt_in=*/true, /*is_default_browser=*/true);
  remote()->Continue(/*is_uma_opt_in=*/true, /*is_default_browser=*/false);
  remote().FlushForTesting();
}

}  // namespace
