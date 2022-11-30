// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/request_metadata.h"

#include "base/memory/ref_counted.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace image_fetcher {

TEST(RequestMetadataTest, Equality) {
  RequestMetadata rhs;
  RequestMetadata lhs;
  rhs.mime_type = "testMimeType";
  lhs.mime_type = "testMimeType";
  rhs.http_response_code = 1;
  lhs.http_response_code = 1;
  lhs.content_location_header = "http://test-location.com/image.png";
  rhs.content_location_header = "http://test-location.com/image.png";

  EXPECT_EQ(rhs, lhs);
}

TEST(RequestMetadataTest, NoEquality) {
  RequestMetadata rhs;
  RequestMetadata lhs;
  rhs.mime_type = "testMimeType";
  lhs.mime_type = "testMimeType";
  rhs.http_response_code = 1;
  lhs.http_response_code = 1;
  lhs.content_location_header = "http://test-location.com/image.png";
  rhs.content_location_header = "http://test-location.com/image.png";

  lhs.mime_type = "testOtherMimeType";
  EXPECT_NE(rhs, lhs);
  lhs.mime_type = "testMimeType";

  lhs.http_response_code = 2;
  EXPECT_NE(rhs, lhs);
  lhs.http_response_code = 1;

  lhs.content_location_header = "http://other.test-location.com/image.png";
  EXPECT_NE(rhs, lhs);
  lhs.content_location_header = "http://test-location.com/image.png";
}

}  // namespace image_fetcher
