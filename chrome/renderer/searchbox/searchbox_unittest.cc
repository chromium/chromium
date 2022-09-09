// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  int GetMainFrameID() const override;
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

int MockIconURLHelper::GetMainFrameID() const {
  return 137;
}

std::string MockIconURLHelper::GetURLStringFromRestrictedID(
    InstantRestrictedID rid) const {
  auto it = rid_to_url_string_.find(rid);
  return it == rid_to_url_string_.end() ? std::string() : it->second;
}

}  // namespace

namespace internal {

// Defined in searchbox.cc
bool ParseFrameIdAndRestrictedId(const std::string& id_part,
                                 int* frame_id_out,
                                 InstantRestrictedID* rid_out);

// Defined in searchbox.cc
bool ParseIconRestrictedUrl(const GURL& url,
                            std::string* param_part,
                            int* frame_id,
                            InstantRestrictedID* rid);

// Defined in searchbox.cc
void TranslateIconRestrictedUrl(const GURL& transient_url,
                                const SearchBox::IconURLHelper& helper,
                                GURL* url);

TEST(SearchBoxUtilTest, ParseFrameIdAndRestrictedIdSuccess) {
  int frame_id = -1;
  InstantRestrictedID rid = -1;

  EXPECT_TRUE(ParseFrameIdAndRestrictedId("2/3", &frame_id, &rid));
  EXPECT_EQ(2, frame_id);
  EXPECT_EQ(3, rid);

  EXPECT_TRUE(ParseFrameIdAndRestrictedId("0/0", &frame_id, &rid));
  EXPECT_EQ(0, frame_id);
  EXPECT_EQ(0, rid);

  EXPECT_TRUE(ParseFrameIdAndRestrictedId("1048576/314", &frame_id, &rid));
  EXPECT_EQ(1048576, frame_id);
  EXPECT_EQ(314, rid);

  // Odd but not fatal.
  EXPECT_TRUE(ParseFrameIdAndRestrictedId("00/09", &frame_id, &rid));
  EXPECT_EQ(0, frame_id);
  EXPECT_EQ(9, rid);

  // Tolerates multiple, leading, and trailing "/".
  EXPECT_TRUE(ParseFrameIdAndRestrictedId("2////3", &frame_id, &rid));
  EXPECT_EQ(2, frame_id);
  EXPECT_EQ(3, rid);

  EXPECT_TRUE(ParseFrameIdAndRestrictedId("5/6/", &frame_id, &rid));
  EXPECT_EQ(5, frame_id);
  EXPECT_EQ(6, rid);

  EXPECT_TRUE(ParseFrameIdAndRestrictedId("/7/8", &frame_id, &rid));
  EXPECT_EQ(7, frame_id);
  EXPECT_EQ(8, rid);
}

TEST(SearchBoxUtilTest, ParseFrameIdAndRestrictedIdFailure) {
  const char* test_cases[] = {
    "",
    "    ",
    "/",
    "2/",
    "/3",
    "2a/3",
    "2/3a",
    " 2/3",
    "2/ 3",
    "2 /3 ",
    "23",
    "2,3",
    "-2/3",
    "2/-3",
    "2/3/1",
    "blahblah",
    "0xA/0x10",
  };
  for (size_t i = 0; i < std::size(test_cases); ++i) {
    int frame_id = -1;
    InstantRestrictedID rid = -1;
    EXPECT_FALSE(ParseFrameIdAndRestrictedId(test_cases[i], &frame_id, &rid))
        << " for test_cases[" << i << "]";
    EXPECT_EQ(-1, frame_id);
    EXPECT_EQ(-1, rid);
  }
}

TEST(SearchBoxUtilTest, ParseIconRestrictedUrlFaviconSuccess) {
  struct {
    const char* transient_url_str;
    const char* expected_param_part;
    int expected_frame_id;
    InstantRestrictedID expected_rid;
  } test_cases[] = {
      {"chrome-search://favicon/1/2", "", 1, 2},
      {"chrome-search://favicon/size/16@2x/3/4", "size/16@2x/", 3, 4},
      {"chrome-search://favicon/iconurl/9/10", "iconurl/", 9, 10},
  };
  for (size_t i = 0; i < std::size(test_cases); ++i) {
    std::string param_part = "(unwritten)";
    int frame_id = -1;
    InstantRestrictedID rid = -1;
    EXPECT_TRUE(ParseIconRestrictedUrl(GURL(test_cases[i].transient_url_str),
                                       &param_part, &frame_id, &rid))
        << " for test_cases[" << i << "]";
    EXPECT_EQ(test_cases[i].expected_param_part, param_part)
        << " for test_cases[" << i << "]";
    EXPECT_EQ(test_cases[i].expected_frame_id, frame_id)
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
    int frame_id = -1;
    InstantRestrictedID rid = -1;
    EXPECT_FALSE(ParseIconRestrictedUrl(GURL(test_cases[i].transient_url_str),
                                        &param_part, &frame_id, &rid))
        << " for test_cases[" << i << "]";
    EXPECT_EQ("(unwritten)", param_part);
    EXPECT_EQ(-1, frame_id);
    EXPECT_EQ(-1, rid);
  }
}

TEST(SearchBoxUtilTest, TranslateIconRestrictedUrlSuccess) {
  struct {
    const char* transient_url_str;
    std::string expected_url_str;
  } test_cases[] = {
      {"chrome-search://favicon/137/1",
       std::string("chrome-search://favicon/") + kUrlString1},
      {"chrome-search://favicon/", "chrome-search://favicon/"},
      {"chrome-search://favicon/314", "chrome-search://favicon/"},
      {"chrome-search://favicon/314/1", "chrome-search://favicon/"},
      {"chrome-search://favicon/137/255", "chrome-search://favicon/"},
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
