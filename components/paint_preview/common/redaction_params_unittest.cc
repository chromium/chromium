// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/redaction_params.h"

#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace paint_preview {

const std::string_view kExampleRoot = "https://example.test";
const std::string_view kExampleSub = "https://www.example.test";
const std::string_view kFooRoot = "https://foo.test";
const std::string_view kFooSub = "https://www.foo.test";

TEST(RedactionParamsTest, DefaultCtor) {
  RedactionParams params;

  EXPECT_FALSE(
      params.ShouldRedactSubframe(url::Origin::Create(GURL(kExampleRoot))));
  EXPECT_FALSE(
      params.ShouldRedactSubframe(url::Origin::Create(GURL(kFooRoot))));
}

TEST(RedactionParamsTest, OriginsFilter) {
  const auto example_origin = url::Origin::Create(GURL(kExampleRoot));
  const auto example_sub_origin = url::Origin::Create(GURL(kExampleSub));
  const auto foo_origin = url::Origin::Create(GURL(kFooRoot));
  const auto foo_sub_origin = url::Origin::Create(GURL(kFooSub));

  RedactionParams params(/*allowed_origins=*/{example_origin},
                         /*allowed_sites=*/{});

  EXPECT_FALSE(params.ShouldRedactSubframe(example_origin));
  EXPECT_TRUE(params.ShouldRedactSubframe(example_sub_origin));
  EXPECT_TRUE(params.ShouldRedactSubframe(foo_origin));
  EXPECT_TRUE(params.ShouldRedactSubframe(foo_sub_origin));
}

TEST(RedactionParamsTest, SitesFilter) {
  const auto example_origin = url::Origin::Create(GURL(kExampleRoot));
  const auto example_sub_origin = url::Origin::Create(GURL(kExampleSub));
  const auto foo_origin = url::Origin::Create(GURL(kFooRoot));
  const auto foo_sub_origin = url::Origin::Create(GURL(kFooSub));

  RedactionParams params(/*allowed_origins=*/{}, /*allowed_sites=*/{
                             net::SchemefulSite(example_sub_origin)});

  EXPECT_FALSE(params.ShouldRedactSubframe(example_origin));
  EXPECT_FALSE(params.ShouldRedactSubframe(example_sub_origin));
  EXPECT_TRUE(params.ShouldRedactSubframe(foo_origin));
  EXPECT_TRUE(params.ShouldRedactSubframe(foo_sub_origin));
}

}  // namespace paint_preview
