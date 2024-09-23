// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/renderer/searchbox/searchbox.h"

#include <stddef.h>

#include <map>
#include <string>

#include "chrome/common/search/instant_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char* kUrlString1 = "http://www.google.com";
const char* kUrlString2 = "http://www.chromium.org/path/q=3#r=4";
const char* kUrlString3 = "http://www.youtube.com:8080/hosps";

// Mock helper to test internal::TranslateIconRestrictedUrl().
class MockIconURLHelper: public SearchBox::IconURLHelper {
 public:
  MockIconURLHelper();
  ~MockIconURLHelper() override;
  std::string GetMainFrameToken() const override;
  std::string GetURLStringFromRestrictedID(InstantRestrictedID rid) const
      override;

 private:
  std::map<InstantRestrictedID, std::string> rid_to_url_string_;
};

MockIconURLHelper::MockIconURLHelper() {
  rid_to_url_string_[1] = kUrlString1;
  rid_to_url_string_[2] = kUrlString2;
  rid_to_url_string_[3] = kUrlString3;
}

MockIconURLHelper::~MockIconURLHelper() {
}

std::string MockIconURLHelper::GetMainFrameToken() const {
  return "0123456789ABCDEF0123456789ABCDEF";
}

std::string MockIconURLHelper::GetURLStringFromRestrictedID(
    InstantRestrictedID rid) const {
  auto it = rid_to_url_string_.find(rid);
  return it == rid_to_url_string_.end() ? std::string() : it->second;
}

}  // namespace

namespace internal {

// Defined in searchbox.cc
bool ParseFrameTokenAndRestrictedId(const std::string& id_part,
                                    std::string* frame_token_out,
                                    InstantRestrictedID* rid_out);

// Defined in searchbox.cc
bool ParseIconRestrictedUrl(const GURL& url,
                            std::string* param_part,
                            std::string* frame_token,
                            InstantRestrictedID* rid);

// Defined in searchbox.cc
void TranslateIconRestrictedUrl(const GURL& transient_url,
                                const SearchBox::IconURLHelper& helper,
                                GURL* url);

TEST(SearchBoxUtilTest, ParseFrameTokenAndRestrictedIdSuccess) {
  std::string frame_token;
  InstantRestrictedID rid = -1;

  EXPECT_TRUE(ParseFrameTokenAndRestrictedId(
      "FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE/3", &frame_token, &rid));
  EXPECT_EQ("FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE", frame_token);
  EXPECT_EQ(3, rid);

  EXPECT_TRUE(ParseFrameTokenAndRestrictedId(
      "00000000000000000000000000000000/0", &frame_token, &rid));
  EXPECT_EQ("00000000000000000000000000000000", frame_token);
  EXPECT_EQ(0, rid);

  EXPECT_TRUE(ParseFrameTokenAndRestrictedId(
      "1FFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE/314", &frame_token, &rid));
  EXPECT_EQ("1FFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE", frame_token);
  EXPECT_EQ(314, rid);

  // Odd but not fatal.
  EXPECT_TRUE(ParseFrameTokenAndRestrictedId(
      "FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE/09", &frame_token, &rid));
  EXPECT_EQ("FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE", frame_token);
  EXPECT_EQ(9, rid);

  // Tolerates multiple, leading, and trailing "/".
  EXPECT_TRUE(ParseFrameTokenAndRestrictedId(
      "FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE////3", &frame_token, &rid));
  EXPECT_EQ("FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE", frame_token);
  EXPECT_EQ(3, rid);

  EXPECT_TRUE(ParseFrameTokenAndRestrictedId(
      "FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE/6/", &frame_token, &rid));
  EXPECT_EQ("FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE", frame_token);
  EXPECT_EQ(6, rid);

  EXPECT_TRUE(ParseFrameTokenAndRestrictedId(
      "/FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE/8", &frame_token, &rid));
  EXPECT_EQ("FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE", frame_token);
  EXPECT_EQ(8, rid);
}

TEST(SearchBoxUtilTest, ParseFrameIdAndRestrictedIdFailure) {
  const char* test_cases[] = {
      "",
      "    ",
      "/",
      "FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE/",
      "/3",
      "2a/3",
      "FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE/3a",
      " 2/3",
      "FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE/ 3",
      "FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE /3 ",
      "23",
      "2,3",
      "-FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE/3",
      "FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE/-3",
      "FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE/3/1",
      "blahblah",
      "0xA/0x10",
  };
  for (size_t i = 0; i < std::size(test_cases); ++i) {
    std::string frame_token;
    InstantRestrictedID rid = -1;
    EXPECT_FALSE(
        ParseFrameTokenAndRestrictedId(test_cases[i], &frame_token, &rid))
        << " for test_cases[" << i << "]";
    EXPECT_EQ("", frame_token);
    EXPECT_EQ(-1, rid);
  }
}

TEST(SearchBoxUtilTest, ParseIconRestrictedUrlFaviconSuccess) {
  struct {
    const char* transient_url_str;
    const char* expected_param_part;
    const char* expected_frame_token;
    InstantRestrictedID expected_rid;
  } test_cases[] = {
      {"chrome-search://favicon/FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE/2", "",
       "FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE", 2},
      {"chrome-search://favicon/size/16@2x/1FFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE/4",
       "size/16@2x/", "1FFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE", 4},
      {"chrome-search://favicon/iconurl/FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFA/10",
       "iconurl/", "FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFA", 10},
  };
  for (size_t i = 0; i < std::size(test_cases); ++i) {
    std::string param_part = "(unwritten)";
    std::string frame_token;
    InstantRestrictedID rid = -1;
    EXPECT_TRUE(ParseIconRestrictedUrl(GURL(test_cases[i].transient_url_str),
                                       &param_part, &frame_token, &rid))
        << " for test_cases[" << i << "]";
    EXPECT_EQ(test_cases[i].expected_param_part, param_part)
        << " for test_cases[" << i << "]";
    EXPECT_EQ(test_cases[i].expected_frame_token, frame_token)
        << " for test_cases[" << i << "]";
    EXPECT_EQ(test_cases[i].expected_rid, rid)
        << " for test_cases[" << i << "]";
  }
}

TEST(SearchBoxUtilTest, ParseIconRestrictedUrlFailure) {
  struct {
    const char* transient_url_str;
  } test_cases[] = {
      {"chrome-search://favicon/"},
      {"chrome-search://favicon/3/"},
      {"chrome-search://favicon/size/3/4"},
      {"chrome-search://favicon/largest/http://www.google.com"},
      {"chrome-search://favicon/size/16@2x/-1/10"},
  };
  for (size_t i = 0; i < std::size(test_cases); ++i) {
    std::string param_part = "(unwritten)";
    std::string frame_token;
    InstantRestrictedID rid = -1;
    EXPECT_FALSE(ParseIconRestrictedUrl(GURL(test_cases[i].transient_url_str),
                                        &param_part, &frame_token, &rid))
        << " for test_cases[" << i << "]";
    EXPECT_EQ("(unwritten)", param_part);
    EXPECT_EQ("", frame_token);
    EXPECT_EQ(-1, rid);
  }
}

TEST(SearchBoxUtilTest, TranslateIconRestrictedUrlSuccess) {
  struct {
    const char* transient_url_str;
    std::string expected_url_str;
  } test_cases[] = {
      {"chrome-search://favicon/0123456789ABCDEF0123456789ABCDEF/1",
       std::string("chrome-search://favicon/") + kUrlString1},
      {"chrome-search://favicon/", "chrome-search://favicon/"},
      {"chrome-search://favicon/314", "chrome-search://favicon/"},
      {"chrome-search://favicon/314/1", "chrome-search://favicon/"},
      {"chrome-search://favicon/0123456789ABCDEF0123456789ABCDEF/255",
       "chrome-search://favicon/"},
      {"chrome-search://favicon/-3/-1", "chrome-search://favicon/"},
      {"chrome-search://favicon/invalidstuff", "chrome-search://favicon/"},
      {"chrome-search://favicon/size/16@2x/http://www.google.com",
       "chrome-search://favicon/"},
  };

  MockIconURLHelper helper;
  for (size_t i = 0; i < std::size(test_cases); ++i) {
    GURL url;
    GURL transient_url(test_cases[i].transient_url_str);
    TranslateIconRestrictedUrl(transient_url, helper, &url);
    EXPECT_EQ(GURL(test_cases[i].expected_url_str), url)
        << " for test_cases[" << i << "]";
  }
}

}  // namespace internal
