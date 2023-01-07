// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/chooser_title_util.h"

#include "components/strings/grit/components_strings.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace permissions {
namespace {

constexpr int kTitleResourceId = IDS_USB_DEVICE_CHOOSER_PROMPT_ORIGIN;

using ChooserTitleTest = content::RenderViewHostTestHarness;

TEST_F(ChooserTitleTest, NoFrame) {
  EXPECT_EQ(u"", CreateChooserTitle(nullptr, kTitleResourceId));
}

TEST_F(ChooserTitleTest, FrameTree) {
  NavigateAndCommit(GURL("https://main-frame.com"));
  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://sub-frame.com"),
          content::RenderFrameHostTester::For(main_rfh())
              ->AppendChild("subframe"));

  EXPECT_EQ("main-frame.com", main_rfh()->GetLastCommittedOrigin().host());
  EXPECT_EQ(u"main-frame.com wants to connect",
            CreateChooserTitle(main_rfh(), kTitleResourceId));
  EXPECT_EQ("sub-frame.com", subframe->GetLastCommittedOrigin().host());
  EXPECT_EQ(u"main-frame.com wants to connect",
            CreateChooserTitle(subframe, kTitleResourceId));
}

}  // namespace
}  // namespace permissions
