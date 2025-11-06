// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"

#include "components/omnibox/browser/searchbox.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

class SearchboxContextDataTest : public testing::Test {};

TEST_F(SearchboxContextDataTest, TakePendingContextReturnsNullPtrWhenNotSet) {
  SearchboxContextData data;
  EXPECT_EQ(nullptr, data.TakePendingContext());
}

TEST_F(SearchboxContextDataTest, SetAndTakePendingContext) {
  SearchboxContextData data;
  auto context = std::make_unique<SearchboxContextData::Context>();
  context->text = "hello";
  data.SetPendingContext(std::move(context));

  std::unique_ptr<SearchboxContextData::Context> taken_context =
      data.TakePendingContext();
  EXPECT_NE(nullptr, taken_context);
  EXPECT_EQ("hello", taken_context->text);
  EXPECT_EQ(nullptr, data.TakePendingContext());
}

TEST_F(SearchboxContextDataTest, SetAndTakePendingContextWithFileInfo) {
  SearchboxContextData data;
  auto context = std::make_unique<SearchboxContextData::Context>();
  context->text = "world";
  auto file_info = searchbox::mojom::SelectedFileInfo::New();
  file_info->file_name = "test.pdf";
  file_info->mime_type = "application/pdf";
  context->file_infos.push_back(std::move(file_info));
  data.SetPendingContext(std::move(context));

  std::unique_ptr<SearchboxContextData::Context> taken_context =
      data.TakePendingContext();
  EXPECT_NE(nullptr, taken_context);
  EXPECT_EQ("world", taken_context->text);
  ASSERT_EQ(1u, taken_context->file_infos.size());
  EXPECT_EQ("test.pdf", taken_context->file_infos[0]->file_name);
  EXPECT_EQ("application/pdf", taken_context->file_infos[0]->mime_type);
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
