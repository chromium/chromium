// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/avatar_icon_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

TEST(AvatarIconUtilTest, GetAvatarImageURLWithOptionsNoInitialSize_LegacyURL) {
  GURL in("http://example.com/-A/AAAAAAAAAAI/AAAAAAAAACQ/B/photo.jpg");
  GURL result =
      signin::GetAvatarImageURLWithOptions(in, 128, false /* no_silhouette */);
  GURL expected(
      "http://example.com/-A/AAAAAAAAAAI/AAAAAAAAACQ/B/s128-c/photo.jpg");
  EXPECT_EQ(result, expected);
}

TEST(AvatarIconUtilTest,
     GetAvatarImageURLWithOptionsSizeAlreadySpecified_LegacyURL) {
  // If there's already a size specified in the URL, it should be changed to the
  // specified size in the resulting URL.
  GURL in("http://example.com/-A/AAAAAAAAAAI/AAAAAAAAACQ/B/s64-c/photo.jpg");
  GURL result =
      signin::GetAvatarImageURLWithOptions(in, 128, false /* no_silhouette */);
  GURL expected(
      "http://example.com/-A/AAAAAAAAAAI/AAAAAAAAACQ/B/s128-c/photo.jpg");
  EXPECT_EQ(result, expected);
}

TEST(AvatarIconUtilTest,
     GetAvatarImageURLWithOptionsOtherSizeSpecified_LegacyURL) {
  // If there's already a size specified in the URL, it should be changed to the
  // specified size in the resulting URL.
  GURL in("http://example.com/-A/AAAAAAAAAAI/AAAAAAAAACQ/B/s128-c/photo.jpg");
  GURL result =
      signin::GetAvatarImageURLWithOptions(in, 64, false /* no_silhouette */);
  GURL expected(
      "http://example.com/-A/AAAAAAAAAAI/AAAAAAAAACQ/B/s64-c/photo.jpg");
  EXPECT_EQ(result, expected);
}

TEST(AvatarIconUtilTest, GetAvatarImageURLWithOptionsSameSize_LegacyURL) {
  // If there's already a size specified in the URL, and it's already the
  // requested size, true should be returned and the URL should remain
  // unchanged.
  GURL in("http://example.com/-A/AAAAAAAAAAI/AAAAAAAAACQ/B/s64-c/photo.jpg");
  GURL result =
      signin::GetAvatarImageURLWithOptions(in, 64, false /* no_silhouette */);
  GURL expected(
      "http://example.com/-A/AAAAAAAAAAI/AAAAAAAAACQ/B/s64-c/photo.jpg");
  EXPECT_EQ(result, expected);
}

TEST(AvatarIconUtilTest,
     GetAvatarImageURLWithOptionsNoFileNameInPath_LegacyURL) {
  // If there is no file path component in the URL path, we should return
  // the input URL.
  GURL in("http://example.com/-A/AAAAAAAAAAI/AAAAAAAAACQ/B/");
  GURL result =
      signin::GetAvatarImageURLWithOptions(in, 128, false /* no_silhouette */);
  EXPECT_EQ(result, in);
}

TEST(AvatarIconUtilTest, GetAvatarImageURLWithOptionsNoSilhouette_LegacyURL) {
  // If there's already a size specified in the URL, it should be changed to the
  // specified size in the resulting URL.
  GURL in("http://example.com/-A/AAAAAAAAAAI/AAAAAAAAACQ/B/photo.jpg");
  GURL result =
      signin::GetAvatarImageURLWithOptions(in, 64, true /* no_silhouette */);
  GURL expected(
      "http://example.com/-A/AAAAAAAAAAI/AAAAAAAAACQ/B/s64-c-ns/photo.jpg");
  EXPECT_EQ(result, expected);
}

TEST(AvatarIconUtilTest,
     GetAvatarImageURLWithOptionsSizeReplaceNoSilhouette_LegacyURL) {
  // If there's already a no_silhouette option specified in the URL, it should
  // be removed if necessary.
  GURL in("http://example.com/-A/AAAAAAAAAAI/AAAAAAAAACQ/B/s64-c-ns/photo.jpg");
  GURL result =
      signin::GetAvatarImageURLWithOptions(in, 128, false /* no_silhouette */);
  GURL expected(
      "http://example.com/-A/AAAAAAAAAAI/AAAAAAAAACQ/B/s128-c/photo.jpg");
  EXPECT_EQ(result, expected);
}

TEST(AvatarIconUtilTest,
     GetAvatarImageURLWithOptionsUnknownShouldBePreserved_LegacyURL) {
  // If there are any unknown options encoded in URL,
  // GetAvatarImageURLWithOptions should preserve them.
  GURL in("http://example.com/-A/AAAAAAAAAAI/AAAAAAAAACQ/B/s64-mo-k/photo.jpg");
  GURL result =
      signin::GetAvatarImageURLWithOptions(in, 128, false /* no_silhouette */);
  GURL expected(
      "http://example.com/-A/AAAAAAAAAAI/AAAAAAAAACQ/B/mo-k-s128-c/photo.jpg");
  EXPECT_EQ(result, expected);
}

TEST(AvatarIconUtilTest, GetAvatarImageURLWithExistingOptions_ContentURL) {
  GURL in("http://example.com/-A/ABcdefghijk1l2mN3=s256");
  GURL result =
      signin::GetAvatarImageURLWithOptions(in, 128, false /* no_silhouette */);
  GURL expected("http://example.com/-A/ABcdefghijk1l2mN3=s128-c");
  EXPECT_EQ(result, expected);
}

TEST(AvatarIconUtilTest, GetAvatarImageURLNoExistingOptions_ContentURL) {
  GURL in("http://example.com/-A/ABcdefghijk1l2mN3");
  GURL result =
      signin::GetAvatarImageURLWithOptions(in, 128, true /* no_silhouette */);
  GURL expected("http://example.com/-A/ABcdefghijk1l2mN3=s128-c-ns");
  EXPECT_EQ(result, expected);
}

}  // namespace
