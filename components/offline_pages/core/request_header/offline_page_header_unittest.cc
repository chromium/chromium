// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/request_header/offline_page_header.h"

#include "base/base64.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_features.h"

namespace offline_pages {

namespace {
std::string Base64EncodeString(const std::string value) {
  return base::Base64Encode(value);
  ;
}
}  // namespace

class OfflinePageHeaderTest : public testing::Test {
 public:
  OfflinePageHeaderTest() = default;
  ~OfflinePageHeaderTest() override = default;

  bool ParseFromHeaderValue(const std::string& header_value,
                            bool* need_to_persist,
                            OfflinePageHeader::Reason* reason,
                            std::string* id,
                            GURL* intent_url) {
    OfflinePageHeader header(header_value);
    *need_to_persist = header.need_to_persist;
    *reason = header.reason;
    *id = header.id;
    *intent_url = header.intent_url;
    return !header.did_fail_parsing_for_test;
  }
};

// Non-special URLs behavior is affected by the
// StandardCompliantNonSpecialSchemeURLParsing feature.
// See https://crbug.com/40063064 for details.
class OfflinePageHeaderParamTest : public OfflinePageHeaderTest,
                                   public ::testing::WithParamInterface<bool> {
 public:
  OfflinePageHeaderParamTest()
      : use_standard_compliant_non_special_scheme_url_parsing_(GetParam()) {
    if (use_standard_compliant_non_special_scheme_url_parsing_) {
      scoped_feature_list_.InitAndEnableFeature(
          url::kStandardCompliantNonSpecialSchemeURLParsing);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          url::kStandardCompliantNonSpecialSchemeURLParsing);
    }
  }

 protected:
  bool use_standard_compliant_non_special_scheme_url_parsing_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(OfflinePageHeaderParamTest, Parse) {
  bool need_to_persist;
  OfflinePageHeader::Reason reason;
  std::string id;
  GURL intent_url;

  // Unstructured data.
  EXPECT_FALSE(
      ParseFromHeaderValue("", &need_to_persist, &reason, &id, &intent_url));
  EXPECT_FALSE(
      ParseFromHeaderValue("   ", &need_to_persist, &reason, &id, &intent_url));
  EXPECT_FALSE(
      ParseFromHeaderValue(" , ", &need_to_persist, &reason, &id, &intent_url));
  EXPECT_FALSE(ParseFromHeaderValue("reason", &need_to_persist, &reason, &id,
                                    &intent_url));
  EXPECT_FALSE(ParseFromHeaderValue("a b c", &need_to_persist, &reason, &id,
                                    &intent_url));
  EXPECT_FALSE(
      ParseFromHeaderValue("a=b", &need_to_persist, &reason, &id, &intent_url));

  // Parse persist field.
  EXPECT_FALSE(ParseFromHeaderValue("persist=aa", &need_to_persist, &reason,
                                    &id, &intent_url));

  EXPECT_TRUE(ParseFromHeaderValue("persist=1", &need_to_persist, &reason, &id,
                                   &intent_url));
  EXPECT_TRUE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::NONE, reason);
  EXPECT_EQ("", id);
  EXPECT_TRUE(intent_url.is_empty());

  EXPECT_TRUE(ParseFromHeaderValue("persist=0", &need_to_persist, &reason, &id,
                                   &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::NONE, reason);
  EXPECT_EQ("", id);
  EXPECT_TRUE(intent_url.is_empty());

  // Parse reason field.
  EXPECT_FALSE(ParseFromHeaderValue("reason=foo", &need_to_persist, &reason,
                                    &id, &intent_url));

  EXPECT_TRUE(ParseFromHeaderValue("reason=error", &need_to_persist, &reason,
                                   &id, &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::NET_ERROR, reason);
  EXPECT_EQ("", id);
  EXPECT_TRUE(intent_url.is_empty());

  EXPECT_TRUE(ParseFromHeaderValue("reason=download", &need_to_persist, &reason,
                                   &id, &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::DOWNLOAD, reason);
  EXPECT_EQ("", id);
  EXPECT_TRUE(intent_url.is_empty());

  EXPECT_TRUE(ParseFromHeaderValue("reason=reload", &need_to_persist, &reason,
                                   &id, &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::RELOAD, reason);
  EXPECT_EQ("", id);
  EXPECT_TRUE(intent_url.is_empty());

  EXPECT_TRUE(ParseFromHeaderValue("reason=notification", &need_to_persist,
                                   &reason, &id, &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::NOTIFICATION, reason);
  EXPECT_EQ("", id);
  EXPECT_TRUE(intent_url.is_empty());

  EXPECT_TRUE(ParseFromHeaderValue("reason=file_url_intent", &need_to_persist,
                                   &reason, &id, &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::FILE_URL_INTENT, reason);
  EXPECT_EQ("", id);
  EXPECT_TRUE(intent_url.is_empty());

  EXPECT_TRUE(ParseFromHeaderValue("reason=content_url_intent",
                                   &need_to_persist, &reason, &id,
                                   &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::CONTENT_URL_INTENT, reason);
  EXPECT_EQ("", id);
  EXPECT_TRUE(intent_url.is_empty());

  EXPECT_TRUE(ParseFromHeaderValue("reason=net_error_suggestion",
                                   &need_to_persist, &reason, &id,
                                   &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::NET_ERROR_SUGGESTION, reason);
  EXPECT_EQ("", id);
  EXPECT_TRUE(intent_url.is_empty());

  // Parse id field.
  EXPECT_TRUE(ParseFromHeaderValue("id=a1b2", &need_to_persist, &reason, &id,
                                   &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::NONE, reason);
  EXPECT_EQ("a1b2", id);
  EXPECT_TRUE(intent_url.is_empty());

  // Parse intent_url field.

  // Failed to decode base64.
  EXPECT_FALSE(ParseFromHeaderValue("intent_url=a@#", &need_to_persist, &reason,
                                    &id, &intent_url));

  // Invalid URL.
  EXPECT_FALSE(
      ParseFromHeaderValue("intent_url=" + Base64EncodeString("://foo/bar"),
                           &need_to_persist, &reason, &id, &intent_url));

  // Unsafe characters in file:// URL are escaped.
  EXPECT_TRUE(ParseFromHeaderValue(
      "intent_url=" + Base64EncodeString("file://foo/Bar%20Test"),
      &need_to_persist, &reason, &id, &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::NONE, reason);
  EXPECT_EQ("", id);
  EXPECT_EQ(GURL("file://foo/Bar%20Test"), intent_url);

  // Unsafe characters in content:// URL are escaped.
  EXPECT_TRUE(ParseFromHeaderValue(
      "intent_url=" + Base64EncodeString("content://foo/Bar%20%22\'\\Test"),
      &need_to_persist, &reason, &id, &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::NONE, reason);
  EXPECT_EQ("", id);
  EXPECT_EQ(GURL("content://foo/Bar%20%22\'\\Test"), intent_url);

  // Unsafe characters in content:// URL are NOT escaped.
  EXPECT_TRUE(ParseFromHeaderValue(
      "intent_url=" + Base64EncodeString("content://foo/Bar \"\'\\Test"),
      &need_to_persist, &reason, &id, &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::NONE, reason);
  EXPECT_EQ("", id);
  EXPECT_EQ(GURL("content://foo/Bar \"\'\\Test"), intent_url);

  // Parse multiple fields.
  EXPECT_TRUE(
      ParseFromHeaderValue("persist=1 reason=download id=a1b2 intent_url=" +
                               Base64EncodeString("content://foo/Bar Test"),
                           &need_to_persist, &reason, &id, &intent_url));
  EXPECT_TRUE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::DOWNLOAD, reason);
  EXPECT_EQ("a1b2", id);
  EXPECT_EQ(GURL("content://foo/Bar Test"), intent_url);
}

TEST_F(OfflinePageHeaderTest, ToEmptyString) {
  OfflinePageHeader header;
  EXPECT_EQ("", header.GetCompleteHeaderString());
  EXPECT_EQ("", header.GetHeaderKeyString());
  EXPECT_EQ("", header.GetHeaderValueString());
}

TEST_P(OfflinePageHeaderParamTest, ToString) {
  OfflinePageHeader header;
  header.need_to_persist = true;
  header.reason = OfflinePageHeader::Reason::DOWNLOAD;
  header.id = "a1b2";

  const char* url = "content://foo/Bar \"\'\\Test";
  header.intent_url = GURL(url);

  // With the StandardCompliantNonSpecialSchemeURLParsing feature enabled, any
  // space (' ') or double quote ('"') characters within the path of
  // non-special URLs are percent-encoded.
  const char* url_spec = use_standard_compliant_non_special_scheme_url_parsing_
                             ? "content://foo/Bar%20%22\'\\Test"
                             : "content://foo/Bar \"\'\\Test";
  EXPECT_EQ(GURL(url).spec(), url_spec);

  EXPECT_EQ(
      "X-Chrome-offline: persist=1 reason=download id=a1b2 "
      "intent_url=" +
          Base64EncodeString(url_spec),
      header.GetCompleteHeaderString());
  EXPECT_EQ("X-Chrome-offline", header.GetHeaderKeyString());
  EXPECT_EQ("persist=1 reason=download id=a1b2 intent_url=" +
                Base64EncodeString(url_spec),
            header.GetHeaderValueString());
}

INSTANTIATE_TEST_SUITE_P(All, OfflinePageHeaderParamTest, ::testing::Bool());

}  // namespace offline_pages
