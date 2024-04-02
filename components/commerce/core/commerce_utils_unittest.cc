// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_utils.h"

#include "base/json/json_reader.h"
#include "components/commerce/core/test_utils.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace commerce {
namespace {

class CommerceUtilsTest : public testing::Test {};

TEST_F(CommerceUtilsTest, GetProductSpecsTabUrl) {
  std::vector<GURL> test_urls{GURL("http://example.com/1"),
                              GURL("http://example.com/2")};
  const auto output = commerce::GetProductSpecsTabUrl(test_urls);

  // The value should be non-empty.
  std::string urls_param;
  ASSERT_TRUE(net::GetValueForKeyInQuery(output, "urls", &urls_param));

  // The value should be a valid JSON list.
  auto json = base::JSONReader::Read(urls_param);

  ASSERT_TRUE(json != std::nullopt);
  ASSERT_TRUE(json.value().is_list());

  base::Value::List* list = json.value().GetIfList();
  ASSERT_TRUE(list != nullptr);
  ASSERT_EQ(2U, list->size());

  for (auto& url : test_urls) {
    auto item = std::find(list->begin(), list->end(), url.spec());
    ASSERT_TRUE(item != list->end());
  }
}
}  // namespace
}  // namespace commerce
