// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/media_transcript_provider.h"

#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

class TestMediaTranscriptProvider : public MediaTranscriptProvider {
 public:
  TestMediaTranscriptProvider() = default;
  ~TestMediaTranscriptProvider() override = default;

  // MediaTranscriptProvider:
  std::vector<proto::MediaTranscript> GetTranscriptsForFrame(
      content::RenderFrameHost* rfh) override {
    return {};
  }
};

}  // namespace

class MediaTranscriptProviderTest : public content::RenderViewHostTestHarness {
};

TEST_F(MediaTranscriptProviderTest, GetForNullWebContents) {
  EXPECT_EQ(nullptr, MediaTranscriptProvider::GetFor(nullptr));
}

TEST_F(MediaTranscriptProviderTest, GetForNoProviderSet) {
  EXPECT_EQ(nullptr, MediaTranscriptProvider::GetFor(web_contents()));
}

TEST_F(MediaTranscriptProviderTest, SetAndGet) {
  auto provider = std::make_unique<TestMediaTranscriptProvider>();
  MediaTranscriptProvider* provider_ptr = provider.get();
  MediaTranscriptProvider::SetFor(web_contents(), std::move(provider));
  EXPECT_EQ(provider_ptr, MediaTranscriptProvider::GetFor(web_contents()));
}

TEST_F(MediaTranscriptProviderTest, SetAndClear) {
  auto provider = std::make_unique<TestMediaTranscriptProvider>();
  MediaTranscriptProvider::SetFor(web_contents(), std::move(provider));
  ASSERT_NE(nullptr, MediaTranscriptProvider::GetFor(web_contents()));

  MediaTranscriptProvider::SetFor(web_contents(), nullptr);
  EXPECT_EQ(nullptr, MediaTranscriptProvider::GetFor(web_contents()));
}

TEST_F(MediaTranscriptProviderTest, MultipleWebContents) {
  std::unique_ptr<content::WebContents> web_contents2 = CreateTestWebContents();

  auto provider1 = std::make_unique<TestMediaTranscriptProvider>();
  MediaTranscriptProvider* provider1_ptr = provider1.get();
  MediaTranscriptProvider::SetFor(web_contents(), std::move(provider1));

  auto provider2 = std::make_unique<TestMediaTranscriptProvider>();
  MediaTranscriptProvider* provider2_ptr = provider2.get();
  MediaTranscriptProvider::SetFor(web_contents2.get(), std::move(provider2));

  EXPECT_EQ(provider1_ptr, MediaTranscriptProvider::GetFor(web_contents()));
  EXPECT_EQ(provider2_ptr,
            MediaTranscriptProvider::GetFor(web_contents2.get()));
}

}  // namespace optimization_guide
