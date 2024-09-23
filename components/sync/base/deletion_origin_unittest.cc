// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/deletion_origin.h"

#include <string>
#include <string_view>

#include "base/hash/hash.h"
#include "base/location.h"
#include "components/sync/protocol/deletion_origin.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

TEST(DeletionOriginTest, ShouldDistinguishSpecifiedFromUnspecified) {
  EXPECT_FALSE(DeletionOrigin::Unspecified().is_specified());
  EXPECT_TRUE(DeletionOrigin::FromLocation(FROM_HERE).is_specified());
}

TEST(DeletionOriginTest, ShouldConvertToProto) {
  const base::Location kLocation = FROM_HERE;
  const std::string kTestVersion = "1234.0.5.0";

  const sync_pb::DeletionOrigin proto =
      DeletionOrigin::FromLocation(kLocation).ToProto(kTestVersion);

  EXPECT_EQ(proto.chromium_version(), kTestVersion);
  EXPECT_EQ(proto.file_name_hash(),
            base::PersistentHash(kLocation.file_name()));
  EXPECT_EQ(proto.file_line_number(), kLocation.line_number());
  EXPECT_TRUE(std::string_view(proto.file_name_possibly_truncated())
                  .ends_with("deletion_origin_unittest.cc"));
}

TEST(DeletionOriginTest, ShouldTruncateFileName) {
  // See constant with the same name in deletion_origin.cc.
  const size_t kMaxFileNameBeforeTruncation = 30;

  const std::string kShortFileName("foo.cc");
  const std::string kLongFileName("components/xxxxxxxxxxxxxxxxxxxx/foo.cc");
  const std::string kExactTruncationLengthFileName(
      "components/xxxxxxxxxxxx/foo.cc");

  ASSERT_LT(kShortFileName.size(), kMaxFileNameBeforeTruncation);
  ASSERT_GT(kLongFileName.size(), kMaxFileNameBeforeTruncation);
  ASSERT_EQ(kExactTruncationLengthFileName.size(),
            kMaxFileNameBeforeTruncation);

  const base::Location kShortFileNameLocation =
      base::Location::CreateForTesting("function_name", kShortFileName.c_str(),
                                       1, FROM_HERE.program_counter());
  const base::Location kLongFileNameLocation = base::Location::CreateForTesting(
      "function_name", kLongFileName.c_str(), 1, FROM_HERE.program_counter());
  const base::Location kExactTruncationLengthFileNameLocation =
      base::Location::CreateForTesting("function_name",
                                       kExactTruncationLengthFileName.c_str(),
                                       1, FROM_HERE.program_counter());

  EXPECT_EQ(kShortFileName, DeletionOrigin::FromLocation(kShortFileNameLocation)
                                .ToProto("")
                                .file_name_possibly_truncated());
  EXPECT_EQ(kExactTruncationLengthFileName,
            DeletionOrigin::FromLocation(kExactTruncationLengthFileNameLocation)
                .ToProto("")
                .file_name_possibly_truncated());

  // The overly long filename should be truncated.
  const std::string kExpectedTruncatedFileName(
      "...xxxxxxxxxxxxxxxxxxxx/foo.cc");
  ASSERT_EQ(kExpectedTruncatedFileName.size(), kMaxFileNameBeforeTruncation);
  EXPECT_EQ(kExpectedTruncatedFileName,
            DeletionOrigin::FromLocation(kLongFileNameLocation)
                .ToProto("")
                .file_name_possibly_truncated());
}

}  // namespace

}  // namespace syncer
