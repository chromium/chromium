// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/welcome_handler.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/webui/intro/welcome.mojom.h"
#include "content/public/test/browser_task_environment.h"
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
    shell_integration::DefaultBrowserWorker::DisableSetAsDefaultForTesting();
  }

  void CreateHandler(base::OnceClosure on_set_as_default_completed_callback =
                         base::NullCallback()) {
    remote_.reset();
    handler_ = std::make_unique<WelcomeHandler>(
        mock_callback_.Get(), remote_.BindNewPipeAndPassReceiver(),
        std::move(on_set_as_default_completed_callback));
  }

  mojo::Remote<intro::mojom::WelcomePageHandler>& remote() { return remote_; }
  base::MockCallback<base::OnceClosure>& mock_callback() {
    return mock_callback_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  StrictMock<base::MockCallback<base::OnceClosure>> mock_callback_;
  mojo::Remote<intro::mojom::WelcomePageHandler> remote_;
  std::unique_ptr<WelcomeHandler> handler_;
};

TEST_F(WelcomeHandlerTest, ContinueWithDefaultBrowserTrue) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  CreateHandler(run_loop.QuitClosure());

  EXPECT_CALL(mock_callback(), Run());

  remote()->Continue(/*is_uma_opt_in=*/true, /*is_default_browser=*/true);
  remote().FlushForTesting();
  run_loop.Run();

  histogram_tester.ExpectTotalCount("DefaultBrowser.SetDefaultResult2", 1);
}

TEST_F(WelcomeHandlerTest, ContinueWithDefaultBrowserFalse) {
  base::HistogramTester histogram_tester;
  CreateHandler();

  EXPECT_CALL(mock_callback(), Run());

  remote()->Continue(/*is_uma_opt_in=*/true, /*is_default_browser=*/false);
  remote().FlushForTesting();

  histogram_tester.ExpectTotalCount("DefaultBrowser.SetDefaultResult2", 0);
}

TEST_F(WelcomeHandlerTest, ContinueWithDefaultBrowserNullopt) {
  base::HistogramTester histogram_tester;
  CreateHandler();

  EXPECT_CALL(mock_callback(), Run());

  remote()->Continue(/*is_uma_opt_in=*/true,
                     /*is_default_browser=*/std::nullopt);
  remote().FlushForTesting();

  histogram_tester.ExpectTotalCount("DefaultBrowser.SetDefaultResult2", 0);
}

TEST_F(WelcomeHandlerTest, ContinueCalledMultipleTimes) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  CreateHandler(run_loop.QuitClosure());

  EXPECT_CALL(mock_callback(), Run());

  remote()->Continue(/*is_uma_opt_in=*/true, /*is_default_browser=*/true);
  remote()->Continue(/*is_uma_opt_in=*/true, /*is_default_browser=*/false);
  remote().FlushForTesting();
  run_loop.Run();

  histogram_tester.ExpectTotalCount("DefaultBrowser.SetDefaultResult2", 1);
}

}  // namespace
