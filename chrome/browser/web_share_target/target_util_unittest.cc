// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_share_target/target_util.h"

#include <string>

#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_share_target {

std::string convertDataElementToString(const network::DataElement& element) {
  if (element.type() == network::DataElement::Tag::kBytes) {
    return std::string(element.As<network::DataElementBytes>().AsStringPiece());
  }
  if (element.type() == network::DataElement::Tag::kFile) {
    return std::string(
        element.As<network::DataElementFile>().path().AsUTF8Unsafe());
  }
  return "";
}

void CheckDataElements(
    const scoped_refptr<network::ResourceRequestBody>& body,
    const std::vector<network::DataElement::Tag>& expected_element_types,
    const std::vector<std::string>& expected_element_values) {
  EXPECT_NE(nullptr, body->elements());
  const std::vector<network::DataElement>& data_elements = *body->elements();

  EXPECT_EQ(expected_element_types.size(), data_elements.size());
  EXPECT_EQ(expected_element_types.size(), expected_element_values.size());

  for (size_t i = 0; i < expected_element_types.size(); ++i) {
    EXPECT_EQ(expected_element_types[i], data_elements[i].type())
        << "unexpected difference at i = " << i;
    EXPECT_EQ(expected_element_values[i],
              convertDataElementToString(data_elements[i]))
        << "unexpected difference at i = " << i;
  }
}

// Test that multipart/form-data body is empty if inputs are of different sizes.
TEST(TargetUtilTest, InvalidMultipartBody) {
  std::vector<std::string> names = {"name"};
  std::vector<std::string> values;
  std::vector<bool> is_value_file_uris;
  std::vector<std::string> filenames;
  std::vector<std::string> types;
  std::string boundary = "boundary";
  scoped_refptr<network::ResourceRequestBody> multipart_body =
      ComputeMultipartBody(names, values, is_value_file_uris, filenames, types,
                           boundary);
  EXPECT_EQ(nullptr, multipart_body.get());
}

// Test that multipart/form-data body is correctly computed for accepted
// file inputs.
TEST(TargetUtilTest, ValidMultipartBodyForFile) {
  std::vector<std::string> names = {"share-file\"", "share-file\""};
  std::vector<std::string> values = {"mock-file-path", "mock-shared-text"};
  std::vector<bool> is_value_file_uris = {true, false};

  std::vector<std::string> filenames = {"filename\r\n", "shared.txt"};
  std::vector<std::string> types = {"type", "type"};
  std::string boundary = "boundary";
  scoped_refptr<network::ResourceRequestBody> multipart_body =
      ComputeMultipartBody(names, values, is_value_file_uris, filenames, types,
                           boundary);

  std::vector<network::DataElement::Tag> expected_types = {
      network::DataElement::Tag::kBytes, network::DataElement::Tag::kFile,
      network::DataElement::Tag::kBytes, network::DataElement::Tag::kBytes,
      network::DataElement::Tag::kBytes};
  std::vector<std::string> expected = {
      "--boundary\r\nContent-Disposition: form-data; name=\"share-file%22\"; "
      "filename=\"filename%0D%0A\"\r\nContent-Type: type\r\n\r\n",
      "mock-file-path", "\r\n",
      "--boundary\r\nContent-Disposition: form-data; name=\"share-file%22\"; "
      "filename=\"shared.txt\"\r\nContent-Type: "
      "type\r\n\r\nmock-shared-text\r\n",
      "--boundary--\r\n"};

  CheckDataElements(multipart_body, expected_types, expected);
}

// Test that multipart/form-data body is correctly computed for non-file inputs.
TEST(TargetUtilTest, ValidMultipartBodyForText) {
  std::vector<std::string> names = {"name\""};
  std::vector<std::string> values = {"value"};
  std::vector<bool> is_value_file_uris = {false};
  std::vector<std::string> filenames = {""};
  std::vector<std::string> types = {"type"};
  std::string boundary = "boundary";
  scoped_refptr<network::ResourceRequestBody> multipart_body =
      ComputeMultipartBody(names, values, is_value_file_uris, filenames, types,
                           boundary);

  std::vector<network::DataElement::Tag> expected_types = {
      network::DataElement::Tag::kBytes, network::DataElement::Tag::kBytes};
  std::vector<std::string> expected = {
      "--boundary\r\nContent-Disposition: form-data; "
      "name=\"name%22\"\r\nContent-Type: type\r\n\r\nvalue\r\n",
      "--boundary--\r\n"};

  CheckDataElements(multipart_body, expected_types, expected);
}

// Test that multipart/form-data body is correctly computed for a mixture
// of file and non-file inputs.
TEST(TargetUtilTest, ValidMultipartBodyForTextAndFile) {
  std::vector<std::string> names = {"name1\"", "name2", "name3",
                                    "name4",   "name5", "name6"};
  std::vector<std::string> values = {"value1", "file_uri2", "file_uri3",
                                     "value4", "file_uri5", "value6"};
  std::vector<bool> is_value_file_uris = {false, true, true,
                                          false, true, false};
  std::vector<std::string> filenames = {"", "filename2\r\n", "filename3", "",
                                        "", "shared.txt"};
  std::vector<std::string> types = {"type1", "type2", "type3",
                                    "type4", "type5", "type6"};
  std::string boundary = "boundary";

  scoped_refptr<network::ResourceRequestBody> body = ComputeMultipartBody(
      names, values, is_value_file_uris, filenames, types, boundary);

  std::vector<network::DataElement::Tag> expected_types = {
      // item 1
      network::DataElement::Tag::kBytes,
      // item 2
      network::DataElement::Tag::kBytes, network::DataElement::Tag::kFile,
      network::DataElement::Tag::kBytes,
      // item 3
      network::DataElement::Tag::kBytes, network::DataElement::Tag::kFile,
      network::DataElement::Tag::kBytes,
      // item 4
      network::DataElement::Tag::kBytes,
      // item 5
      network::DataElement::Tag::kBytes, network::DataElement::Tag::kFile,
      network::DataElement::Tag::kBytes,
      // item 6
      network::DataElement::Tag::kBytes,
      // ending
      network::DataElement::Tag::kBytes};
  std::vector<std::string> expected = {
      // item 1
      "--boundary\r\nContent-Disposition: form-data; "
      "name=\"name1%22\"\r\nContent-Type: type1\r\n\r\nvalue1\r\n",
      // item 2
      "--boundary\r\nContent-Disposition: form-data; name=\"name2\"; "
      "filename=\"filename2%0D%0A\"\r\nContent-Type: type2\r\n\r\n",
      "file_uri2", "\r\n",
      // item 3
      "--boundary\r\nContent-Disposition: form-data; name=\"name3\"; "
      "filename=\"filename3\"\r\nContent-Type: type3\r\n\r\n",
      "file_uri3", "\r\n",
      // item 4
      "--boundary\r\nContent-Disposition: form-data; "
      "name=\"name4\"\r\nContent-Type: type4\r\n\r\nvalue4\r\n",
      // item 5
      "--boundary\r\nContent-Disposition: form-data; "
      "name=\"name5\"\r\nContent-Type: type5\r\n\r\n",
      "file_uri5", "\r\n",
      // item 6
      "--boundary\r\nContent-Disposition: form-data; name=\"name6\"; "
      "filename=\"shared.txt\"\r\nContent-Type: type6\r\n\r\nvalue6\r\n",
      "--boundary--\r\n"};
  CheckDataElements(body, expected_types, expected);
}

// Test that multipart/form-data body is properly percent-escaped.
TEST(TargetUtilTest, MultipartBodyWithPercentEncoding) {
  std::vector<std::string> names = {"name"};
  std::vector<std::string> values = {"value"};
  std::vector<bool> is_value_file_uris = {false};
  std::vector<std::string> filenames = {"filename"};
  std::vector<std::string> types = {"type"};
  std::string boundary = "boundary";
  scoped_refptr<network::ResourceRequestBody> body = ComputeMultipartBody(
      names, values, is_value_file_uris, filenames, types, boundary);
  EXPECT_NE(nullptr, body->elements());

  std::vector<network::DataElement::Tag> expected_types = {
      network::DataElement::Tag::kBytes, network::DataElement::Tag::kBytes};
  std::vector<std::string> expected = {
      "--boundary\r\nContent-Disposition: form-data;"
      " name=\"name\"; filename=\"filename\"\r\nContent-Type: type"
      "\r\n\r\nvalue\r\n",
      "--boundary--\r\n"};

  CheckDataElements(body, expected_types, expected);
}

// Test that application/x-www-form-urlencoded body is empty if inputs are of
// different sizes.
TEST(TargetUtilTest, InvalidApplicationBody) {
  std::vector<std::string> names = {"name1", "name2"};
  std::vector<std::string> values = {"value1"};
  std::string application_body = ComputeUrlEncodedBody(names, values);
  EXPECT_EQ("", application_body);
}

// Test that application/x-www-form-urlencoded body is correctly computed for
// accepted inputs.
TEST(TargetUtilTest, ValidApplicationBody) {
  std::vector<std::string> names = {"name1", "name2"};
  std::vector<std::string> values = {"value1", "value2"};
  std::string application_body = ComputeUrlEncodedBody(names, values);
  EXPECT_EQ("name1=value1&name2=value2", application_body);
}

// Test that PercentEscapeString correctly escapes quotes to %22.
TEST(TargetUtilTest, NeedsPercentEscapeQuote) {
  EXPECT_EQ("hello%22", PercentEscapeString("hello\""));
}

// Test that PercentEscapeString correctly escapes newline to %0A.
TEST(TargetUtilTest, NeedsPercentEscape0A) {
  EXPECT_EQ("%0A", PercentEscapeString("\n"));
}

// Test that PercentEscapeString correctly escapes \r to %0D.
TEST(TargetUtilTest, NeedsPercentEscape0D) {
  EXPECT_EQ("%0D", PercentEscapeString("\r"));
}

// Test that Percent Escape is not performed on strings that don't need to be
// escaped.
TEST(TargetUtilTest, NoPercentEscape) {
  EXPECT_EQ("helloworld", PercentEscapeString("helloworld"));
}

}  // namespace web_share_target
