// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/searchbox.h"

#include <stddef.h>

#include <map>
#include <string>

#include "base/macros.h"
#include "chrome/common/search/instant_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const auto FAVICON = SearchBox::FAVICON;
const auto THUMB = SearchBox::THUMB;

const char* kUrlString1 = "http://www.google.com";
const char* kUrlString2 = "http://www.chromium.org/path/q=3#r=4";
const char* kUrlString3 = "http://www.youtube.com:8080/hosps";

// Mock helper to test internal::TranslateIconRestrictedUrl().
class MockIconURLHelper: public SearchBox::IconURLHelper {
 public:
  MockIconURLHelper();
  ~MockIconURLHelper() override;
  int GetViewID() const override;
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

int MockIconURLHelper::GetViewID() const {
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
bool ParseViewIdAndRestrictedId(const std::string& id_part,
                                int* view_id_out,
                                InstantRestrictedID* rid_out);

// Defined in searchbox.cc
bool ParseIconRestrictedUrl(const GURL& url,
                            SearchBox::ImageSourceType type,
                            std::string* param_part,
                            int* view_id,
                            InstantRestrictedID* rid);

// Defined in searchbox.cc
bool TranslateIconRestrictedUrl(const GURL& transient_url,
                                SearchBox::ImageSourceType type,
                                const SearchBox::IconURLHelper& helper,
                                GURL* url);

// Defined in searchbox.cc
std::pair<GURL, bool> FixupAndValidateUrl(const std::string& url);

TEST(SearchBoxUtilTest, ParseViewIdAndRestrictedIdSuccess) {
  int view_id = -1;
  InstantRestrictedID rid = -1;

  EXPECT_TRUE(ParseViewIdAndRestrictedId("2/3", &view_id, &rid));
  EXPECT_EQ(2, view_id);
  EXPECT_EQ(3, rid);

  EXPECT_TRUE(ParseViewIdAndRestrictedId("0/0", &view_id, &rid));
  EXPECT_EQ(0, view_id);
  EXPECT_EQ(0, rid);

  EXPECT_TRUE(ParseViewIdAndRestrictedId("1048576/314", &view_id, &rid));
  EXPECT_EQ(1048576, view_id);
  EXPECT_EQ(314, rid);

  // Odd but not fatal.
  EXPECT_TRUE(ParseViewIdAndRestrictedId("00/09", &view_id, &rid));
  EXPECT_EQ(0, view_id);
  EXPECT_EQ(9, rid);

  // Tolerates multiple, leading, and trailing "/".
  EXPECT_TRUE(ParseViewIdAndRestrictedId("2////3", &view_id, &rid));
  EXPECT_EQ(2, view_id);
  EXPECT_EQ(3, rid);

  EXPECT_TRUE(ParseViewIdAndRestrictedId("5/6/", &view_id, &rid));
  EXPECT_EQ(5, view_id);
  EXPECT_EQ(6, rid);

  EXPECT_TRUE(ParseViewIdAndRestrictedId("/7/8", &view_id, &rid));
  EXPECT_EQ(7, view_id);
  EXPECT_EQ(8, rid);
}

TEST(SearchBoxUtilTest, ParseViewIdAndRestrictedIdFailure) {
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
  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    int view_id = -1;
    InstantRestrictedID rid = -1;
    EXPECT_FALSE(ParseViewIdAndRestrictedId(test_cases[i], &view_id, &rid))
      << " for test_cases[" << i << "]";
    EXPECT_EQ(-1, view_id);
    EXPECT_EQ(-1, rid);
  }
}

TEST(SearchBoxUtilTest, ParseIconRestrictedUrlFaviconSuccess) {
  struct {
    SearchBox::ImageSourceType type;
    const char* transient_url_str;
    const char* expected_param_part;
    int expected_view_id;
    InstantRestrictedID expected_rid;
  } test_cases[] = {
    {FAVICON, "chrome-search://favicon/1/2", "", 1, 2},
    {FAVICON, "chrome-search://favicon/size/16@2x/3/4", "size/16@2x/", 3, 4},
    {FAVICON, "chrome-search://favicon/iconurl/9/10", "iconurl/", 9, 10},
    {THUMB, "chrome-search://thumb/1/2", "", 1, 2},
  };
  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    std::string param_part = "(unwritten)";
    int view_id = -1;
    InstantRestrictedID rid = -1;
    EXPECT_TRUE(ParseIconRestrictedUrl(GURL(test_cases[i].transient_url_str),
        test_cases[i].type, &param_part, &view_id, &rid))
            << " for test_cases[" << i << "]";
    EXPECT_EQ(test_cases[i].expected_param_part, param_part)
        << " for test_cases[" << i << "]";
    EXPECT_EQ(test_cases[i].expected_view_id, view_id)
        << " for test_cases[" << i << "]";
    EXPECT_EQ(test_cases[i].expected_rid, rid)
        << " for test_cases[" << i << "]";
  }
}

TEST(SearchBoxUtilTest, ParseIconRestrictedUrlFailure) {
  struct {
    SearchBox::ImageSourceType type;
    const char* transient_url_str;
  } test_cases[] = {
    {FAVICON, "chrome-search://favicon/"},
    {FAVICON, "chrome-search://favicon/3/"},
    {FAVICON, "chrome-search://favicon/size/3/4"},
    {FAVICON, "chrome-search://favicon/largest/http://www.google.com"},
    {FAVICON, "chrome-search://favicon/size/16@2x/-1/10"},
    {THUMB, "chrome-search://thumb"},
    {THUMB, "chrome-search://thumb/"},
    {THUMB, "chrome-search://thumb/123"},
    {THUMB, "chrome-search://thumb/xyz"},
    {THUMB, "chrome-search://thumb/123/"},
    {THUMB, "chrome-search://thumb/123/xyz"},
    {THUMB, "chrome-search://thumb/http://www.google.com"},
  };
  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    std::string param_part = "(unwritten)";
    int view_id = -1;
    InstantRestrictedID rid = -1;
    EXPECT_FALSE(ParseIconRestrictedUrl(GURL(test_cases[i].transient_url_str),
        test_cases[i].type, &param_part, &view_id, &rid))
            << " for test_cases[" << i << "]";
    EXPECT_EQ("(unwritten)", param_part);
    EXPECT_EQ(-1, view_id);
    EXPECT_EQ(-1, rid);
  }
}

TEST(SearchBoxUtilTest, TranslateIconRestrictedUrlSuccess) {
  struct {
    SearchBox::ImageSourceType type;
    const char* transient_url_str;
    std::string expected_url_str;
  } test_cases[] = {
    {FAVICON, "chrome-search://favicon/137/1",
        std::string("chrome-search://favicon/") + kUrlString1},
    // FAVICON is permission: invalid input just yields default endpoint.
    {FAVICON, "chrome-search://favicon/", "chrome-search://favicon/"},
    {FAVICON, "chrome-search://favicon/314", "chrome-search://favicon/"},
    {FAVICON, "chrome-search://favicon/314/1", "chrome-search://favicon/"},
    {FAVICON, "chrome-search://favicon/137/255", "chrome-search://favicon/"},
    {FAVICON, "chrome-search://favicon/-3/-1", "chrome-search://favicon/"},
    {FAVICON, "chrome-search://favicon/invalidstuff",
        "chrome-search://favicon/"},
    {FAVICON, "chrome-search://favicon/size/16@2x/http://www.google.com",
        "chrome-search://favicon/"},
    // Other types of icons.
    {THUMB, "chrome-search://thumb/137/3",
        std::string("chrome-search://thumb/") + kUrlString3},
  };

  MockIconURLHelper helper;
  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    GURL url;
    GURL transient_url(test_cases[i].transient_url_str);
    EXPECT_TRUE(TranslateIconRestrictedUrl(transient_url, test_cases[i].type,
                    helper, &url))
        << " for test_cases[" << i << "]";
    EXPECT_EQ(GURL(test_cases[i].expected_url_str), url)
        << " for test_cases[" << i << "]";
  }
}

// For Non-FAVICON only.
TEST(SearchBoxUtilTest, TranslateIconRestrictedUrlFailure) {
  struct {
    SearchBox::ImageSourceType type;
    const char* transient_url_str;
  } test_cases[] = {
    // Empty.
    {THUMB, "chrome-search://thumb/"},
    // Bad view_id.
    {THUMB, "chrome-search://thumb/314/1"},
    // Missing rid.
    {THUMB, "chrome-search://thumb/314/"},
    // Use Page URL.
    {THUMB, "chrome-search://thumb/http://www.google.com"},
  };

  MockIconURLHelper helper;
  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    GURL url;
    GURL transient_url(test_cases[i].transient_url_str);
    EXPECT_FALSE(TranslateIconRestrictedUrl(transient_url, test_cases[i].type,
                     helper, &url))
        << " for test_cases[" << i << "]";
    EXPECT_TRUE(url.is_empty()) << " for test_cases[" << i << "]";
  }
}

TEST(SearchBoxUtilTest, FixupAndValidateUrlReturnsEmptyIfInvalid) {
  struct TestCase {
    const char* url;
    bool is_valid;
  } test_cases[] = {
      {"   ", false},
      {"^&*@)^)", false},
      {"foo", true},
      {"http://foo", true},
      {"\thttp://foo", true},
      {"    http://foo", true},
      {"https://foo", true},
      {"foo.com", true},
      {"http://foo.com", true},
      {"https://foo.com", true},
      {"blob://foo", true},

  };

  for (const TestCase& test_case : test_cases) {
    std::pair<GURL, bool> fixed_url = FixupAndValidateUrl(test_case.url);
    const std::string& url = fixed_url.first.spec();
    EXPECT_EQ(!url.empty(), test_case.is_valid)
        << " for test_case '" << test_case.url << "'";
  }
}

TEST(SearchBoxUtilTest, FixupAndValidateUrlDefaultsToHttps) {
  struct TestCase {
    const char* url;
    bool default_https;
    const char* expected_scheme;
  } test_cases[] = {
      // No scheme.
      {"foo.com", true, url::kHttpsScheme},
      // With "http".
      {"http://foo.com", false, url::kHttpScheme},
      // With "http" and whitespace.
      {"\thttp://foo", false, url::kHttpScheme},
      {"    http://foo", false, url::kHttpScheme},
      // With "https".
      {"https://foo.com", false, url::kHttpsScheme},
      // Non "http"/"https".
      {"blob://foo", false, url::kBlobScheme},
  };

  for (const TestCase& test_case : test_cases) {
    std::pair<GURL, bool> fixed_url = FixupAndValidateUrl(test_case.url);
    EXPECT_TRUE(fixed_url.first.SchemeIs(test_case.expected_scheme))
        << " for test case '" << test_case.url << "'";
    EXPECT_EQ(fixed_url.second, test_case.default_https)
        << " for test case '" << test_case.url << "'";
  }
}

}  // namespace internal
