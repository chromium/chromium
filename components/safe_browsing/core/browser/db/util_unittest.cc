// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/safe_browsing/core/browser/db/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

TEST(ThreatMetadataTest, Equality) {
  ThreatMetadata t1;
  t1.subresource_filter_match = {
      {SubresourceFilterType::ABUSIVE, SubresourceFilterLevel::ENFORCE}};
  ThreatMetadata t2;
  t2.subresource_filter_match = {
      {SubresourceFilterType::ABUSIVE, SubresourceFilterLevel::ENFORCE}};
  EXPECT_TRUE(t1 == t2);
}

TEST(ThreatMetadataTest, Inequality) {
  ThreatMetadata t1;
  t1.subresource_filter_match = {
      {SubresourceFilterType::ABUSIVE, SubresourceFilterLevel::ENFORCE}};
  ThreatMetadata t2;
  t2.subresource_filter_match = {
      {SubresourceFilterType::ABUSIVE, SubresourceFilterLevel::WARN}};
  EXPECT_TRUE(t1 != t2);
}

TEST(ThreatMetadataTest, ToTracedValue) {
  ThreatMetadata t1;
  t1.subresource_filter_match = {
      {SubresourceFilterType::ABUSIVE, SubresourceFilterLevel::ENFORCE}};
  std::unique_ptr<base::trace_event::TracedValue> v1 = t1.ToTracedValue();
  std::string json;
  v1->AppendAsTraceFormat(&json);
  EXPECT_EQ(
      "{"
      "\"subresource_filter_match\":{\"match_metadata\":[0,1]}"
      "}",
      json);
}

}  // namespace
}  // namespace safe_browsing
