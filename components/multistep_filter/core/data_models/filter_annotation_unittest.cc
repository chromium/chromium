// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/data_models/filter_annotation.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {

namespace {

constexpr char kTaskType[] = "task_type1";
constexpr char kSourceDomain[] = "domain1.com";
constexpr char kKey1[] = "key1";
constexpr char kValue1[] = "value1";
constexpr base::Time kCreationTimestamp = base::Time::UnixEpoch();

base::Uuid TestUuid() {
  return base::Uuid::ParseLowercase("00000000-0000-0000-0000-000000000000");
}

TEST(FilterAnnotationTest, FilterAttribute_Constructor) {
  FilterAttribute attr(kKey1, kValue1);
  EXPECT_EQ(attr.key, kKey1);
  EXPECT_EQ(attr.value, kValue1);
}

TEST(FilterAnnotationTest, FilterAttribute_Constructor_InvalidAttributes) {
  EXPECT_DCHECK_DEATH(FilterAttribute("", kValue1));
  EXPECT_DCHECK_DEATH(FilterAttribute(kKey1, ""));
}

TEST(FilterAnnotationTest, FilterAnnotation_Constructor) {
  FilterAttribute attr(kKey1, kValue1);
  FilterAnnotation annotation(
      /*id=*/TestUuid(),
      /*task_type=*/kTaskType,
      /*source_domain=*/kSourceDomain,
      /*creation_timestamp=*/kCreationTimestamp,
      /*attributes=*/{attr});
  EXPECT_EQ(annotation.task_type, kTaskType);
  EXPECT_EQ(annotation.source_domain, kSourceDomain);
  EXPECT_EQ(annotation.creation_timestamp, kCreationTimestamp);
  EXPECT_EQ(annotation.attributes.size(), 1u);
  EXPECT_EQ(annotation.attributes[0], attr);
}

TEST(FilterAnnotationTest, FilterAnnotation_Constructor_InvalidAttributes) {
  EXPECT_DCHECK_DEATH(
      FilterAnnotation(TestUuid(), "", kSourceDomain, kCreationTimestamp, {}));
  EXPECT_DCHECK_DEATH(
      FilterAnnotation(TestUuid(), kTaskType, "", kCreationTimestamp, {}));
}

TEST(FilterAnnotationTest, FilterAttribute_ToString) {
  FilterAttribute attr(kKey1, kValue1);
  EXPECT_EQ(attr.ToString(), "FilterAttribute(key=key1, value=value1)");
}

TEST(FilterAnnotationTest, FilterAnnotation_ToString) {
  FilterAttribute attr(kKey1, kValue1);
  FilterAnnotation annotation(TestUuid(), kTaskType, kSourceDomain,
                              kCreationTimestamp, {attr});

  std::string expected =
      "FilterAnnotation(id=00000000-0000-0000-0000-000000000000, "
      "task_type=task_type1, source_domain=domain1.com, "
      "creation_timestamp=" +
      base::NumberToString(
          kCreationTimestamp.ToDeltaSinceWindowsEpoch().InMicroseconds()) +
      ", attributes=[FilterAttribute(key=key1, value=value1)])";
  EXPECT_EQ(annotation.ToString(), expected);
}

}  // namespace

}  // namespace multistep_filter
