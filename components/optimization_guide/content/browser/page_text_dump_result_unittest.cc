// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_text_dump_result.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

TEST(FrameTextDumpResultTest, Preliminary) {
  content::GlobalRenderFrameHostId id(1, 2);
  FrameTextDumpResult frame_result = FrameTextDumpResult::Initialize(
      mojom::TextDumpEvent::kFirstLayout, id,
      /*amp_frame=*/false, /*unique_navigation_id=*/1);

  EXPECT_FALSE(frame_result.IsCompleted());
  EXPECT_EQ(mojom::TextDumpEvent::kFirstLayout, frame_result.event());
  EXPECT_EQ(id, frame_result.rfh_id());
  EXPECT_EQ(1, frame_result.unique_navigation_id());
  EXPECT_FALSE(frame_result.amp_frame());
}

TEST(FrameTextDumpResultTest, Equality) {
  FrameTextDumpResult starting_frame_result =
      FrameTextDumpResult::Initialize(mojom::TextDumpEvent::kFirstLayout,
                                      content::GlobalRenderFrameHostId(1, 2),
                                      /*amp_frame=*/false,
                                      /*unique_navigation_id=*/0)
          .CompleteWithContents(u"abc");

  FrameTextDumpResult different_event =
      FrameTextDumpResult::Initialize(mojom::TextDumpEvent::kFinishedLoad,
                                      content::GlobalRenderFrameHostId(1, 2),
                                      /*amp_frame=*/false,
                                      /*unique_navigation_id=*/0)
          .CompleteWithContents(u"abc");

  FrameTextDumpResult different_id =
      FrameTextDumpResult::Initialize(mojom::TextDumpEvent::kFirstLayout,
                                      content::GlobalRenderFrameHostId(2, 1),
                                      /*amp_frame=*/false,
                                      /*unique_navigation_id=*/0)
          .CompleteWithContents(u"abc");

  FrameTextDumpResult different_amp_frame =
      FrameTextDumpResult::Initialize(mojom::TextDumpEvent::kFirstLayout,
                                      content::GlobalRenderFrameHostId(1, 2),
                                      /*amp_frame=*/true,
                                      /*unique_navigation_id=*/0)
          .CompleteWithContents(u"abc");

  FrameTextDumpResult different_contents =
      FrameTextDumpResult::Initialize(mojom::TextDumpEvent::kFirstLayout,
                                      content::GlobalRenderFrameHostId(1, 2),
                                      /*amp_frame=*/false,
                                      /*unique_navigation_id=*/0)
          .CompleteWithContents(u"abcd");

  FrameTextDumpResult different_nav_id =
      FrameTextDumpResult::Initialize(mojom::TextDumpEvent::kFirstLayout,
                                      content::GlobalRenderFrameHostId(1, 2),
                                      /*amp_frame=*/false,
                                      /*unique_navigation_id=*/1)
          .CompleteWithContents(u"abc");

  std::vector<FrameTextDumpResult> all_frame_results = {
      starting_frame_result, different_event,    different_id,
      different_amp_frame,   different_contents, different_nav_id,
  };
  for (size_t lhs = 0; lhs < all_frame_results.size() - 1; lhs++) {
    for (size_t rhs = lhs + 1; rhs < all_frame_results.size(); rhs++) {
      SCOPED_TRACE(base::StringPrintf("lhs: %zu, rhs: %zu", lhs, rhs));
      EXPECT_FALSE(all_frame_results[lhs] == all_frame_results[rhs]);
    }
  }
}

TEST(FrameTextDumpResultTest, Ordering) {
  FrameTextDumpResult starting_frame_result =
      FrameTextDumpResult::Initialize(mojom::TextDumpEvent::kFirstLayout,
                                      content::GlobalRenderFrameHostId(1, 2),
                                      /*amp_frame=*/false,
                                      /*unique_navigation_id=*/0)
          .CompleteWithContents(u"abc");

  FrameTextDumpResult later_event_frame_result =
      FrameTextDumpResult::Initialize(mojom::TextDumpEvent::kFinishedLoad,
                                      content::GlobalRenderFrameHostId(1, 2),
                                      /*amp_frame=*/false,
                                      /*unique_navigation_id=*/0)
          .CompleteWithContents(u"abc");

  FrameTextDumpResult longer_frame_result =
      FrameTextDumpResult::Initialize(mojom::TextDumpEvent::kFirstLayout,
                                      content::GlobalRenderFrameHostId(1, 2),
                                      /*amp_frame=*/false,
                                      /*unique_navigation_id=*/0)
          .CompleteWithContents(u"abcd");

  FrameTextDumpResult amp_frame_result =
      FrameTextDumpResult::Initialize(mojom::TextDumpEvent::kFirstLayout,
                                      content::GlobalRenderFrameHostId(1, 2),
                                      /*amp_frame=*/true,
                                      /*unique_navigation_id=*/0)
          .CompleteWithContents(u"abc");

  FrameTextDumpResult longer_amp_frame_result =
      FrameTextDumpResult::Initialize(mojom::TextDumpEvent::kFirstLayout,
                                      content::GlobalRenderFrameHostId(1, 2),
                                      /*amp_frame=*/true,
                                      /*unique_navigation_id=*/0)
          .CompleteWithContents(u"abcd");

  std::set<FrameTextDumpResult> ordered_set{
      starting_frame_result, later_event_frame_result, longer_frame_result,
      amp_frame_result, longer_amp_frame_result};
  EXPECT_THAT(ordered_set, ::testing::ElementsAreArray({
                               longer_amp_frame_result,
                               amp_frame_result,
                               longer_frame_result,
                               later_event_frame_result,
                               starting_frame_result,
                           }));
}

TEST(PageTextDumpResultTest, WhitespaceTrimmed) {
  FrameTextDumpResult frame_result =
      FrameTextDumpResult::Initialize(mojom::TextDumpEvent::kFirstLayout,
                                      content::GlobalRenderFrameHostId(1, 2),
                                      /*amp_frame=*/false,
                                      /*unique_navigation_id=*/0)
          .CompleteWithContents(u"  abc\n\n");

  ASSERT_TRUE(frame_result.contents());
  EXPECT_EQ(*frame_result.contents(), u"abc");
}

TEST(PageTextDumpResultTest, Empty) {
  PageTextDumpResult page_result;
  EXPECT_TRUE(page_result.empty());
  EXPECT_EQ(std::nullopt, page_result.GetAMPTextContent());
  EXPECT_EQ(std::nullopt, page_result.GetMainFrameTextContent());
  EXPECT_EQ(std::nullopt, page_result.GetAllFramesTextContent());
}

TEST(PageTextDumpResultTest, OneAMP) {
  PageTextDumpResult page_result;
  page_result.AddFrameTextDumpResult(
      FrameTextDumpResult::Initialize(mojom::TextDumpEvent::kFinishedLoad,
                                      content::GlobalRenderFrameHostId(2, 1),
                                      /*amp_frame=*/true,
                                      /*unique_navigation_id=*/0)
          .CompleteWithContents(u"amp frame"));

  ASSERT_TRUE(page_result.GetAMPTextContent());
  EXPECT_EQ("amp frame", *page_result.GetAMPTextContent());

  EXPECT_FALSE(page_result.GetMainFrameTextContent());

  ASSERT_TRUE(page_result.GetAllFramesTextContent());
  EXPECT_EQ("amp frame", *page_result.GetAllFramesTextContent());
}

TEST(PageTextDumpResultTest, OneMainframe) {
  PageTextDumpResult page_result;
  page_result.AddFrameTextDumpResult(
      FrameTextDumpResult::Initialize(mojom::TextDumpEvent::kFirstLayout,
                                      content::GlobalRenderFrameHostId(1, 2),
                                      /*amp_frame=*/false,
                                      /*unique_navigation_id=*/0)
          .CompleteWithContents(u"mainframe"));

  EXPECT_FALSE(page_result.GetAMPTextContent());

  ASSERT_TRUE(page_result.GetMainFrameTextContent());
  EXPECT_EQ("mainframe", *page_result.GetMainFrameTextContent());

  ASSERT_TRUE(page_result.GetAllFramesTextContent());
  EXPECT_EQ("mainframe", *page_result.GetAllFramesTextContent());
}

TEST(PageTextDumpResultTest, OneAMPOneMF) {
  PageTextDumpResult page_result;
  page_result.AddFrameTextDumpResult(
      FrameTextDumpResult::Initialize(mojom::TextDumpEvent::kFirstLayout,
                                      content::GlobalRenderFrameHostId(1, 2),
                                      /*amp_frame=*/false,
                                      /*unique_navigation_id=*/0)
          .CompleteWithContents(u"mainframe"));

  page_result.AddFrameTextDumpResult(
      FrameTextDumpResult::Initialize(mojom::TextDumpEvent::kFinishedLoad,
                                      content::GlobalRenderFrameHostId(2, 1),
                                      /*amp_frame=*/true,
                                      /*unique_navigation_id=*/0)
          .CompleteWithContents(u"amp frame"));

  ASSERT_TRUE(page_result.GetAMPTextContent());
  EXPECT_EQ("amp frame", *page_result.GetAMPTextContent());

  ASSERT_TRUE(page_result.GetMainFrameTextContent());
  EXPECT_EQ("mainframe", *page_result.GetMainFrameTextContent());

  ASSERT_TRUE(page_result.GetAllFramesTextContent());
  EXPECT_EQ("amp frame mainframe", *page_result.GetAllFramesTextContent());
}

}  // namespace optimization_guide
