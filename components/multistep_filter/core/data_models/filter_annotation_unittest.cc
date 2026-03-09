// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/data_models/filter_annotation.h"

#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

constexpr char kTaskType[] = "task_type1";
constexpr char kSourceDomain[] = "domain1.com";
constexpr char kNormalizedKey1[] = "normalized_key1";
constexpr char kNormalizedValue1[] = "normalized_value1";
constexpr char kSourceUrl[] = "https://domain1.com";
constexpr base::Time kCreationTimestamp = base::Time::UnixEpoch();

base::Uuid TestUuid() {
  return base::Uuid::ParseLowercase("00000000-0000-0000-0000-000000000000");
}

TEST(FilterAnnotationTest, FilterAttribute_Constructor) {
  FilterAttribute attr(kNormalizedKey1, kNormalizedValue1);
  EXPECT_EQ(attr.normalized_key, kNormalizedKey1);
  EXPECT_EQ(attr.normalized_value, kNormalizedValue1);
}

TEST(FilterAnnotationTest, FilterAttribute_Constructor_InvalidAttributes) {
  EXPECT_DCHECK_DEATH(FilterAttribute("", kNormalizedValue1));
  EXPECT_DCHECK_DEATH(FilterAttribute(kNormalizedKey1, ""));
}

TEST(FilterAnnotationTest, FilterAnnotation_Constructor) {
  GURL source_url(kSourceUrl);
  FilterAttribute attr(kNormalizedKey1, kNormalizedValue1);
  FilterAnnotation annotation(
      /*id=*/TestUuid(),
      /*task_type=*/kTaskType,
      /*source_domain=*/kSourceDomain,
      /*source_url=*/source_url,
      /*creation_timestamp=*/kCreationTimestamp,
      /*attributes=*/{attr});
  EXPECT_EQ(annotation.task_type, kTaskType);
  EXPECT_EQ(annotation.source_domain, kSourceDomain);
  EXPECT_EQ(annotation.source_url, source_url);
  EXPECT_EQ(annotation.creation_timestamp, kCreationTimestamp);
  EXPECT_EQ(annotation.attributes.size(), 1u);
  EXPECT_EQ(annotation.attributes[0], attr);
}

TEST(FilterAnnotationTest, FilterAnnotation_Constructor_InvalidAttributes) {
  EXPECT_DCHECK_DEATH(FilterAnnotation(
      TestUuid(), "", kSourceDomain, GURL(kSourceUrl), kCreationTimestamp, {}));
  EXPECT_DCHECK_DEATH(FilterAnnotation(
      TestUuid(), kTaskType, "", GURL(kSourceUrl), kCreationTimestamp, {}));
}

}  // namespace

}  // namespace multistep_filter
