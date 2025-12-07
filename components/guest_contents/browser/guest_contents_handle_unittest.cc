// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_contents/browser/guest_contents_handle.h"

#include "base/location.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"

namespace guest_contents {

class GuestContentsHandleTest : public content::RenderViewHostTestHarness {
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
  }

  void TearDown() override {
    outer_delegate_rfh_ = nullptr;
    guest_web_contents_.reset();
    outer_web_contents_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<content::WebContents>& guest_web_contents() {
    return guest_web_contents_;
  }
  std::unique_ptr<content::WebContents>& outer_web_contents() {
    return outer_web_contents_;
  }
  content::RenderFrameHost& outer_delegate_rfh() {
    return *outer_delegate_rfh_;
  }

 protected:
  std::unique_ptr<content::WebContents> guest_web_contents_;
  std::unique_ptr<content::WebContents> outer_web_contents_;
  raw_ptr<content::RenderFrameHost> outer_delegate_rfh_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
    features::kAttachUnownedInnerWebContents};
};

TEST_F(GuestContentsHandleTest, CreateAndGetById) {
  GuestContentsHandle* handle =
      GuestContentsHandle::CreateForWebContents(guest_web_contents().get());
  ASSERT_TRUE(handle);
  EXPECT_EQ(handle,
            GuestContentsHandle::FromWebContents(guest_web_contents().get()));

  GuestId id = handle->id();
  EXPECT_EQ(handle, GuestContentsHandle::FromID(id));

  // Test invalid ID
  EXPECT_EQ(nullptr, GuestContentsHandle::FromID(id + 1));
  EXPECT_EQ(nullptr, GuestContentsHandle::FromID(-1));

  // Destroy the WebContents, which should destroy the handle
  guest_web_contents().reset();

  // Handle should no longer be findable by ID
  EXPECT_EQ(nullptr, GuestContentsHandle::FromID(id));
}

TEST_F(GuestContentsHandleTest, UniqueIds) {
  GuestContentsHandle* handle1 =
      GuestContentsHandle::CreateForWebContents(guest_web_contents().get());
  ASSERT_TRUE(handle1);

  // Create a second guest WebContents and handle
  std::unique_ptr<content::WebContents> guest_web_contents2 =
      CreateTestWebContents();
  GuestContentsHandle* handle2 =
      GuestContentsHandle::CreateForWebContents(guest_web_contents2.get());
  ASSERT_TRUE(handle2);

  EXPECT_NE(handle1->id(), handle2->id());

  // Clean up second guest
  guest_web_contents2.reset();
  // First handle should still exist
  EXPECT_EQ(handle1, GuestContentsHandle::FromID(handle1->id()));
}

TEST_F(GuestContentsHandleTest, AttachAndDetach) {
  GuestContentsHandle* handle =
      GuestContentsHandle::CreateForWebContents(guest_web_contents().get());
  ASSERT_TRUE(handle);

  handle->AttachToOuterWebContents(&outer_delegate_rfh());
  EXPECT_EQ(guest_web_contents()->GetOuterWebContents(),
            outer_web_contents().get());
  EXPECT_TRUE(base::Contains(outer_web_contents()->GetInnerWebContents(),
                             guest_web_contents().get()));

  handle->DetachFromOuterWebContents();
  EXPECT_EQ(nullptr, guest_web_contents()->GetOuterWebContents());
  EXPECT_TRUE(outer_web_contents()->GetInnerWebContents().empty());
}

TEST_F(GuestContentsHandleTest, DestroyGuestContents) {
  GuestContentsHandle* handle =
      GuestContentsHandle::CreateForWebContents(guest_web_contents().get());
  ASSERT_TRUE(handle);

  handle->AttachToOuterWebContents(&outer_delegate_rfh());
  EXPECT_EQ(guest_web_contents()->GetOuterWebContents(),
            outer_web_contents().get());
  EXPECT_TRUE(base::Contains(outer_web_contents()->GetInnerWebContents(),
            guest_web_contents().get()));

  // Destroy the guest WebContents while it is still attached to the outer
  // WebContents.
  guest_web_contents().reset();
  EXPECT_TRUE(outer_web_contents()->GetInnerWebContents().empty());
}

TEST_F(GuestContentsHandleTest, DestroyOuterWebContents) {
  GuestContentsHandle* handle =
      GuestContentsHandle::CreateForWebContents(guest_web_contents().get());
  ASSERT_TRUE(handle);

  handle->AttachToOuterWebContents(&outer_delegate_rfh());
  EXPECT_EQ(guest_web_contents()->GetOuterWebContents(),
            outer_web_contents().get());

  // Destroy the outer WebContents while it still has a guest WebContents.
  outer_delegate_rfh_ = nullptr;
  outer_web_contents().reset();
  EXPECT_EQ(nullptr, guest_web_contents()->GetOuterWebContents());
}

}  // namespace guest_contents
