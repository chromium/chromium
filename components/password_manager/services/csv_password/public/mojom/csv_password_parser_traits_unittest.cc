// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/services/csv_password/public/mojom/csv_password_parser_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

TEST(CsvPasswordParserTraitsTest, SerializeAndDeserializeMalformedURL) {
  password_manager::CSVPassword input = password_manager::CSVPassword(
      "ww1.google.com", "username", "password", "note",
      password_manager::CSVPassword::Status::kOK);

  password_manager::CSVPassword output;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<password_manager::mojom::CSVPassword>(
          input, output));
  EXPECT_EQ(output.GetParseStatus(),
            password_manager::CSVPassword::Status::kOK);
  EXPECT_EQ(output, input);
}

TEST(CsvPasswordParserTraitsTest, SerializeAndDeserializeGoodURL) {
  GURL url("https://www.google.com");
  password_manager::CSVPassword input =
      password_manager::CSVPassword(url, "username", "password", "note",
                                    password_manager::CSVPassword::Status::kOK);

  password_manager::CSVPassword output;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<password_manager::mojom::CSVPassword>(
          input, output));
  EXPECT_EQ(output.GetParseStatus(),
            password_manager::CSVPassword::Status::kOK);
  EXPECT_EQ(output, input);
}

TEST(CsvPasswordParserTraitsTest, SerializeAndDeserializeSyntaxError) {
  password_manager::CSVPassword input = password_manager::CSVPassword(
      /*invalid_url=*/"", /*username=*/"", /*password=*/"", /*note=*/"",
      password_manager::CSVPassword::Status::kSyntaxError);

  password_manager::CSVPassword output;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<password_manager::mojom::CSVPassword>(
          input, output));
  EXPECT_EQ(output, input);
}

TEST(CsvPasswordParserTraitsTest, SerializeAndDeserializeSemanticError) {
  password_manager::CSVPassword input = password_manager::CSVPassword(
      /*invalid_url=*/"", /*username=*/"", /*password=*/"", /*note=*/"",
      password_manager::CSVPassword::Status::kSemanticError);

  password_manager::CSVPassword output;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<password_manager::mojom::CSVPassword>(
          input, output));
  EXPECT_EQ(output, input);
}

}  // namespace mojo
