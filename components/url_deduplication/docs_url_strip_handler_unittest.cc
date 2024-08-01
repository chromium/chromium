// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_deduplication/docs_url_strip_handler.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace url_deduplication {

class DocsURLStripHandlerTest : public ::testing::Test {
 public:
  DocsURLStripHandlerTest() = default;

  void SetUp() override { handler_ = std::make_unique<DocsURLStripHandler>(); }

  DocsURLStripHandler* Handler() { return handler_.get(); }

 private:
  std::unique_ptr<DocsURLStripHandler> handler_;
};

TEST_F(DocsURLStripHandlerTest, StripURL) {
  GURL full_url = GURL(
      "https://docs.google.com/document/d/document1#heading=h.xaresuk9ir9a");
  GURL stripped_url = Handler()->StripExtraParams(full_url);
  ASSERT_EQ("https://drive.google.com/open?id=document1", stripped_url.spec());
}

TEST_F(DocsURLStripHandlerTest, StripURLNonDocsURL) {
  GURL full_url = GURL(
      "https://nondocsurl.com/document/d/document1#heading=h.xaresuk9ir9a");
  GURL stripped_url = Handler()->StripExtraParams(full_url);
  ASSERT_EQ("", stripped_url.spec());
}

}  // namespace url_deduplication
