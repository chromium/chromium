// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals_ui.h"

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals.mojom.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/multistep_filter/core/features.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter_internals {
namespace {

class MultistepFilterInternalsUITest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override { ChromeRenderViewHostTestHarness::SetUp(); }
};

TEST_F(MultistepFilterInternalsUITest, IsWebUIEnabled_FeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(multistep_filter::kMultistepFilter);

  MultistepFilterInternalsUIConfig config;
  EXPECT_TRUE(config.IsWebUIEnabled(profile()));
}

TEST_F(MultistepFilterInternalsUITest, IsWebUIEnabled_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(multistep_filter::kMultistepFilter);

  MultistepFilterInternalsUIConfig config;
  EXPECT_FALSE(config.IsWebUIEnabled(profile()));
}

TEST_F(MultistepFilterInternalsUITest, IsWebUIEnabled_OffTheRecord) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(multistep_filter::kMultistepFilter);

  Profile* incognito_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  MultistepFilterInternalsUIConfig config;
  EXPECT_FALSE(config.IsWebUIEnabled(incognito_profile));
}

TEST_F(MultistepFilterInternalsUITest, CreatePageHandler) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(multistep_filter::kMultistepFilter);

  content::TestWebUI web_ui;
  web_ui.set_web_contents(web_contents());

  const auto controller = std::make_unique<MultistepFilterInternalsUI>(&web_ui);

  mojo::Remote<mojom::PageHandler> page_handler_remote;
  mojo::PendingRemote<mojom::Page> page_remote;
  mojo::PendingReceiver<mojom::Page> page_receiver =
      page_remote.InitWithNewPipeAndPassReceiver();

  controller->CreatePageHandler(
      std::move(page_remote), page_handler_remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<std::vector<mojom::LogEntryPtr>> future;
  page_handler_remote->GetBufferedLogs(future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

}  // namespace
}  // namespace multistep_filter_internals
