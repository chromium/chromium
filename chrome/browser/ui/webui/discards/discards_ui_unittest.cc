// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/discards/discards_ui.h"

#include "base/test/run_until.h"
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/graph_impl.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DiscardsUITest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    pm_harness_.SetUp();
    SetContents(CreateTestWebContents());
    web_ui_.set_web_contents(web_contents());
  }

  void TearDown() override {
    DeleteContents();
    pm_harness_.TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  size_t GraphOwnedCount() {
    return performance_manager::GraphImpl::FromGraph(
               performance_manager::PerformanceManager::GetGraph())
        ->GraphOwnedCountForTesting();
  }

  content::TestWebUI web_ui_;

 private:
  performance_manager::PerformanceManagerTestHarnessHelper pm_harness_;
};

// The DetailsProvider implementation is owned by the graph. It must be removed
// from the graph when the WebUI disconnects, rather than living until the graph
// is torn down.
TEST_F(DiscardsUITest, DetailsProviderDeletedOnDisconnect) {
  DiscardsUI discards_ui(&web_ui_);
  const size_t initial_count = GraphOwnedCount();

  mojo::Remote<discards::mojom::DetailsProvider> remote;
  discards_ui.BindInterface(remote.BindNewPipeAndPassReceiver());
  EXPECT_EQ(GraphOwnedCount(), initial_count + 1);

  remote.reset();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GraphOwnedCount() == initial_count; }));
}

}  // namespace
