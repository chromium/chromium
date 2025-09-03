// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_context_eligibility.h"

#include <vector>

#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content_metadata.mojom.h"

TEST(PageContextEligibilityTest, GetFrameMetadataFromPageContent) {
  optimization_guide::AIPageContentResult result;
  blink::mojom::PageMetadataPtr page_metadata =
      blink::mojom::PageMetadata::New();
  std::vector<blink::mojom::FrameMetadataPtr> frame_metadata_list;

  blink::mojom::FrameMetadataPtr frame_metadata =
      blink::mojom::FrameMetadata::New();
  frame_metadata->url = GURL("https://www.google.com/search?q=text#someref");

  std::vector<blink::mojom::MetaTagPtr> meta_tags;
  blink::mojom::MetaTagPtr meta_tag = blink::mojom::MetaTag::New();
  meta_tag->name = "meta-tag-name";
  meta_tag->content = "meta-tag-content";
  meta_tags.push_back(std::move(meta_tag));

  frame_metadata->meta_tags = std::move(meta_tags);
  frame_metadata_list.push_back(std::move(frame_metadata));

  page_metadata->frame_metadata = std::move(frame_metadata_list);
  result.metadata = std::move(page_metadata);

  const auto frame_metadata_structs = GetFrameMetadataFromPageContent(result);
  ASSERT_EQ(1ul, frame_metadata_structs.size());

  const auto frame_metadata_struct = frame_metadata_structs[0];
  EXPECT_EQ("www.google.com", frame_metadata_struct.host);
  EXPECT_EQ("/search", frame_metadata_struct.path);
  ASSERT_EQ(1ul, frame_metadata_struct.meta_tags.size());

  const auto meta_tag_struct = frame_metadata_struct.meta_tags[0];
  EXPECT_EQ("meta-tag-name", meta_tag_struct.name);
  EXPECT_EQ("meta-tag-content", meta_tag_struct.content);
}
