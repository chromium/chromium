// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"

#include "base/unguessable_token.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kHelloText[] = "hello";
const char kWorldText[] = "world";
const char kTestPdf[] = "test.pdf";
const char kApplicationPdf[] = "application/pdf";
const char kImageUrl[] = "image url";
const char kTabName[] = "My Tab";
const int kTabId = 123;
const char kExampleUrl[] = "https://example.com";

}  // namespace

class SearchboxContextDataTest : public testing::Test {};

TEST_F(SearchboxContextDataTest, TakePendingContextReturnsNullPtrWhenNotSet) {
  SearchboxContextData data;
  EXPECT_EQ(nullptr, data.TakePendingContext());
}

TEST_F(SearchboxContextDataTest, SetAndTakePendingContext) {
  SearchboxContextData data;
  auto context = std::make_unique<SearchboxContextData::Context>();
  context->text = kHelloText;
  data.SetPendingContext(std::move(context));

  std::unique_ptr<SearchboxContextData::Context> taken_context =
      data.TakePendingContext();
  EXPECT_NE(nullptr, taken_context);
  EXPECT_EQ(kHelloText, taken_context->text);
  EXPECT_EQ(nullptr, data.TakePendingContext());
}

TEST_F(SearchboxContextDataTest, SetAndTakePendingContextWithToolMode) {
  SearchboxContextData data;
  auto context = std::make_unique<SearchboxContextData::Context>();
  context->mode = searchbox::mojom::ToolMode::kCreateImage;
  data.SetPendingContext(std::move(context));

  std::unique_ptr<SearchboxContextData::Context> taken_context =
      data.TakePendingContext();
  EXPECT_NE(nullptr, taken_context);
  EXPECT_EQ(searchbox::mojom::ToolMode::kCreateImage, taken_context->mode);
  EXPECT_EQ(nullptr, data.TakePendingContext());
}

TEST_F(SearchboxContextDataTest, SetAndTakePendingContextWithFileAttachment) {
  SearchboxContextData data;
  auto context = std::make_unique<SearchboxContextData::Context>();
  context->text = kWorldText;
  context->file_infos.push_back(
      searchbox::mojom::SearchContextAttachment::NewFileAttachment(
          searchbox::mojom::FileAttachment::New(
              base::UnguessableToken::Create(), kTestPdf, kApplicationPdf,
              kImageUrl)));
  data.SetPendingContext(std::move(context));

  std::unique_ptr<SearchboxContextData::Context> taken_context =
      data.TakePendingContext();
  EXPECT_NE(nullptr, taken_context);
  EXPECT_EQ(kWorldText, taken_context->text);
  ASSERT_EQ(1u, taken_context->file_infos.size());
  EXPECT_TRUE(taken_context->file_infos[0]->is_file_attachment());
  EXPECT_FALSE(taken_context->file_infos[0]->is_tab_attachment());
  EXPECT_EQ(kTestPdf,
            taken_context->file_infos[0]->get_file_attachment()->name);
  EXPECT_EQ(kApplicationPdf,
            taken_context->file_infos[0]->get_file_attachment()->mime_type);
  EXPECT_EQ(
      kImageUrl,
      taken_context->file_infos[0]->get_file_attachment()->image_data_url);
  EXPECT_EQ(nullptr, data.TakePendingContext());
}

TEST_F(SearchboxContextDataTest, SetAndTakePendingContextWithTabAttachment) {
  SearchboxContextData data;
  auto context = std::make_unique<SearchboxContextData::Context>();
  context->text = kWorldText;
  context->file_infos.push_back(
      searchbox::mojom::SearchContextAttachment::NewTabAttachment(
          searchbox::mojom::TabAttachment::New(kTabId, kTabName,
                                                   GURL(kExampleUrl))));
  data.SetPendingContext(std::move(context));

  std::unique_ptr<SearchboxContextData::Context> taken_context =
      data.TakePendingContext();
  EXPECT_NE(nullptr, taken_context);
  EXPECT_EQ(kWorldText, taken_context->text);
  ASSERT_EQ(1u, taken_context->file_infos.size());
  EXPECT_FALSE(taken_context->file_infos[0]->is_file_attachment());
  EXPECT_TRUE(taken_context->file_infos[0]->is_tab_attachment());
  EXPECT_EQ(kTabId, taken_context->file_infos[0]->get_tab_attachment()->tab_id);
  EXPECT_EQ(kTabName,
            taken_context->file_infos[0]->get_tab_attachment()->title);
  EXPECT_EQ(GURL(kExampleUrl),
            taken_context->file_infos[0]->get_tab_attachment()->url);
  EXPECT_EQ(nullptr, data.TakePendingContext());
}
