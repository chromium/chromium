// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/welcome_handler.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/webui/intro/welcome.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::NiceMock;

class WelcomeHandlerTest : public testing::Test {
 public:
  WelcomeHandlerTest() = default;
  ~WelcomeHandlerTest() override = default;

  void SetUp() override {
    shell_integration::DefaultBrowserWorker::DisableSetAsDefaultForTesting();
  }

  void CreateHandler() {
    remote_.reset();
    handler_ = std::make_unique<WelcomeHandler>(
        mock_continue_callback_.Get(), remote_.BindNewPipeAndPassReceiver(),
        mock_set_as_default_callback_.Get(), mock_metrics_callback_.Get());
  }

  mojo::Remote<intro::mojom::WelcomePageHandler>& remote() { return remote_; }
  base::MockCallback<base::OnceClosure>& mock_continue_callback() {
    return mock_continue_callback_;
  }
  base::MockCallback<base::OnceCallback<void(bool)>>& mock_metrics_callback() {
    return mock_metrics_callback_;
  }
  base::MockCallback<base::OnceClosure>& mock_set_as_default_callback() {
    return mock_set_as_default_callback_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  NiceMock<base::MockCallback<base::OnceClosure>> mock_continue_callback_;
  NiceMock<base::MockCallback<base::OnceCallback<void(bool)>>>
      mock_metrics_callback_;
  NiceMock<base::MockCallback<base::OnceClosure>>
      mock_set_as_default_callback_;
  mojo::Remote<intro::mojom::WelcomePageHandler> remote_;
  std::unique_ptr<WelcomeHandler> handler_;
};

TEST_F(WelcomeHandlerTest, ContinueWithDefaultBrowserTrue) {
  CreateHandler();

  base::test::TestFuture<void> future;
  EXPECT_CALL(mock_set_as_default_callback(), Run())
      .WillOnce(base::test::InvokeFuture(future));

  remote()->Continue(/*is_uma_opt_in=*/std::nullopt,
                     /*is_default_browser=*/true);
  remote().FlushForTesting();
  EXPECT_TRUE(future.Wait());
}

TEST_F(WelcomeHandlerTest, ContinueWithDefaultBrowserFalse) {
  CreateHandler();

  EXPECT_CALL(mock_set_as_default_callback(), Run()).Times(0);

  remote()->Continue(/*is_uma_opt_in=*/std::nullopt,
                     /*is_default_browser=*/false);
  remote().FlushForTesting();
}

TEST_F(WelcomeHandlerTest, ContinueWithDefaultBrowserNullopt) {
  CreateHandler();

  EXPECT_CALL(mock_set_as_default_callback(), Run()).Times(0);

  remote()->Continue(/*is_uma_opt_in=*/std::nullopt,
                     /*is_default_browser=*/std::nullopt);
  remote().FlushForTesting();
}

TEST_F(WelcomeHandlerTest, ContinueWithUmaOptInTrue) {
  CreateHandler();

  EXPECT_CALL(mock_metrics_callback(), Run(true));

  remote()->Continue(/*is_uma_opt_in=*/true,
                     /*is_default_browser=*/std::nullopt);
  remote().FlushForTesting();
}

TEST_F(WelcomeHandlerTest, ContinueWithUmaOptInFalse) {
  CreateHandler();

  EXPECT_CALL(mock_metrics_callback(), Run(false));

  remote()->Continue(/*is_uma_opt_in=*/false,
                     /*is_default_browser=*/std::nullopt);
  remote().FlushForTesting();
}

TEST_F(WelcomeHandlerTest, ContinueWithUmaOptInNullopt) {
  CreateHandler();

  EXPECT_CALL(mock_metrics_callback(), Run).Times(0);

  remote()->Continue(/*is_uma_opt_in=*/std::nullopt,
                     /*is_default_browser=*/std::nullopt);
  remote().FlushForTesting();
}

TEST_F(WelcomeHandlerTest, ContinueCalledMultipleTimes) {
  CreateHandler();

  EXPECT_CALL(mock_continue_callback(), Run()).Times(1);

  remote()->Continue(/*is_uma_opt_in=*/std::nullopt,
                     /*is_default_browser=*/std::nullopt);
  remote()->Continue(/*is_uma_opt_in=*/std::nullopt,
                     /*is_default_browser=*/std::nullopt);
  remote().FlushForTesting();
}

}  // namespace
