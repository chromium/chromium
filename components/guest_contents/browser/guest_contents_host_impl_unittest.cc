// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/guest_contents/browser/guest_contents_host_impl.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/guest_contents/browser/guest_contents_handle.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace guest_contents {

class GuestContentsHostImplTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    guest_web_contents_ = CreateTestWebContents();
    content::RenderFrameHostTester::For(
        guest_web_contents_->GetPrimaryMainFrame())
        ->InitializeRenderFrameIfNeeded();
    outer_web_contents_ = CreateTestWebContents();
    content::RenderFrameHostTester* outer_rfh_tester =
        content::RenderFrameHostTester::For(
            outer_web_contents_->GetPrimaryMainFrame());
    outer_rfh_tester->InitializeRenderFrameIfNeeded();
    outer_delegate_rfh_ = outer_rfh_tester->AppendChild("outer_delegate");
    ASSERT_TRUE(outer_delegate_rfh_);

    GuestContentsHostImpl::Create(
        outer_web_contents_.get(),
        guest_contents_host_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    // Reset the remote so that the mojo pipe is destroyed, which should
    // asynchronously destroy the GuestContentsHostImpl. In production, this is
    // done by the RenderFrameHost.
    guest_contents_host_remote_.reset();
    task_environment()->RunUntilIdle();

    outer_delegate_rfh_ = nullptr;
    guest_web_contents_.reset();
    outer_web_contents_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  content::WebContents* guest_web_contents() {
    return guest_web_contents_.get();
  }

  content::WebContents* outer_web_contents() {
    return outer_web_contents_.get();
  }

  content::RenderFrameHost* outer_delegate_rfh() { return outer_delegate_rfh_; }

  mojom::GuestContentsHost* guest_contents_host() {
    return guest_contents_host_remote_.get();
  }

 private:
  std::unique_ptr<content::WebContents> guest_web_contents_;
  std::unique_ptr<content::WebContents> outer_web_contents_;
  raw_ptr<content::RenderFrameHost> outer_delegate_rfh_;
  mojo::Remote<mojom::GuestContentsHost> guest_contents_host_remote_;
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAttachUnownedInnerWebContents};
};

TEST_F(GuestContentsHostImplTest, Attach) {
  GuestContentsHandle* handle =
      GuestContentsHandle::CreateForWebContents(guest_web_contents());
  ASSERT_TRUE(handle);

  base::RunLoop run_loop;
  guest_contents_host()->Attach(
      outer_delegate_rfh()->GetFrameToken(), handle->id(),
      base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_EQ(guest_web_contents()->GetOuterWebContents(), outer_web_contents());
  EXPECT_TRUE(base::Contains(outer_web_contents()->GetInnerWebContents(),
                             guest_web_contents()));
}

}  // namespace guest_contents
