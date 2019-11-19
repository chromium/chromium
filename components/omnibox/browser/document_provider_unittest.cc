// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/document_provider.h"

#include "base/json/json_reader.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time_to_iso8601.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::string SAMPLE_ORIGINAL_URL =
    "https://www.google.com/url?url=https://drive.google.com/a/domain.tld/"
    "open?id%3D_0123_ID_4567_&_placeholder_";

const std::string SAMPLE_STRIPPED_URL =
    "https://drive.google.com/open?id=_0123_ID_4567_";

using testing::Return;

class FakeAutocompleteProviderClient : public MockAutocompleteProviderClient {
 public:
  FakeAutocompleteProviderClient()
      : template_url_service_(new TemplateURLService(nullptr, 0)) {
    pref_service_.registry()->RegisterBooleanPref(
        omnibox::kDocumentSuggestEnabled, true);
  }

  bool SearchSuggestEnabled() const override { return true; }

  TemplateURLService* GetTemplateURLService() override {
    return template_url_service_.get();
  }

  TemplateURLService* GetTemplateURLService() const override {
    return template_url_service_.get();
  }

  PrefService* GetPrefs() override { return &pref_service_; }

 private:
  std::unique_ptr<TemplateURLService> template_url_service_;
  TestingPrefServiceSimple pref_service_;

  DISALLOW_COPY_AND_ASSIGN(FakeAutocompleteProviderClient);
};

}  // namespace

class DocumentProviderTest : public testing::Test,
                             public AutocompleteProviderListener {
 public:
  DocumentProviderTest();

  void SetUp() override;

 protected:
  // AutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches) override;

  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<DocumentProvider> provider_;
  TemplateURL* default_template_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DocumentProviderTest);
};

DocumentProviderTest::DocumentProviderTest() {}

void DocumentProviderTest::SetUp() {
  client_.reset(new FakeAutocompleteProviderClient());

  TemplateURLService* turl_model = client_->GetTemplateURLService();
  turl_model->Load();

  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16("t"));
  data.SetURL("https://www.google.com/?q={searchTerms}");
  data.suggestions_url = "https://www.google.com/complete/?q={searchTerms}";
  default_template_url_ = turl_model->Add(std::make_unique<TemplateURL>(data));
  turl_model->SetUserSelectedDefaultSearchProvider(default_template_url_);

  // Add a keyword provider.
  data.SetShortName(base::ASCIIToUTF16("wiki"));
  data.SetKeyword(base::ASCIIToUTF16("wikipedia.org"));
  data.SetURL("https://en.wikipedia.org/w/index.php?search={searchTerms}");
  data.suggestions_url =
      "https://en.wikipedia.org/w/index.php?search={searchTerms}";
  turl_model->Add(std::make_unique<TemplateURL>(data));

  // Add another.
  data.SetShortName(base::ASCIIToUTF16("drive"));
  data.SetKeyword(base::ASCIIToUTF16("drive.google.com"));
  data.SetURL("https://drive.google.com/drive/search?q={searchTerms}");
  data.suggestions_url =
      "https://drive.google.com/drive/search?q={searchTerms}";
  turl_model->Add(std::make_unique<TemplateURL>(data));

  provider_ = DocumentProvider::Create(client_.get(), this, 4);
}

void DocumentProviderTest::OnProviderUpdate(bool updated_matches) {
  // No action required.
}

TEST_F(DocumentProviderTest, CheckFeatureBehindFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(omnibox::kDocumentProvider);
  EXPECT_FALSE(
      provider_->IsDocumentProviderAllowed(client_.get(), AutocompleteInput()));
}

TEST_F(DocumentProviderTest, CheckFeaturePrerequisiteNoIncognito) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kDocumentProvider);
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsAuthenticated()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsSyncActive()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));

  // Feature starts enabled.
  EXPECT_TRUE(
      provider_->IsDocumentProviderAllowed(client_.get(), AutocompleteInput()));

  // Feature should be disabled in incognito.
  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(true));
  EXPECT_FALSE(
      provider_->IsDocumentProviderAllowed(client_.get(), AutocompleteInput()));
}

TEST_F(DocumentProviderTest, CheckFeaturePrerequisiteNoSync) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kDocumentProvider);
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsAuthenticated()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsSyncActive()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));

  // Feature starts enabled.
  EXPECT_TRUE(
      provider_->IsDocumentProviderAllowed(client_.get(), AutocompleteInput()));

  // Feature should be disabled without active sync.
  EXPECT_CALL(*client_.get(), IsSyncActive()).WillOnce(Return(false));
  EXPECT_FALSE(
      provider_->IsDocumentProviderAllowed(client_.get(), AutocompleteInput()));
}

TEST_F(DocumentProviderTest, CheckFeaturePrerequisiteClientSettingOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kDocumentProvider);
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsAuthenticated()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsSyncActive()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));

  // Feature starts enabled.
  EXPECT_TRUE(
      provider_->IsDocumentProviderAllowed(client_.get(), AutocompleteInput()));

  // Disabling toggle in chrome://settings should be respected.
  PrefService* fake_prefs = client_->GetPrefs();
  fake_prefs->SetBoolean(omnibox::kDocumentSuggestEnabled, false);
  EXPECT_FALSE(
      provider_->IsDocumentProviderAllowed(client_.get(), AutocompleteInput()));
  fake_prefs->SetBoolean(omnibox::kDocumentSuggestEnabled, true);
}

TEST_F(DocumentProviderTest, CheckFeaturePrerequisiteDefaultSearch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kDocumentProvider);
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsAuthenticated()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsSyncActive()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));

  // Feature starts enabled.
  EXPECT_TRUE(
      provider_->IsDocumentProviderAllowed(client_.get(), AutocompleteInput()));

  // Switching default search disables it.
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16("t"));
  data.SetURL("https://www.notgoogle.com/?q={searchTerms}");
  data.suggestions_url = "https://www.notgoogle.com/complete/?q={searchTerms}";
  TemplateURL* new_default_provider =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(
      new_default_provider);
  EXPECT_FALSE(
      provider_->IsDocumentProviderAllowed(client_.get(), AutocompleteInput()));
  template_url_service->SetUserSelectedDefaultSearchProvider(
      default_template_url_);
  template_url_service->Remove(new_default_provider);
}

TEST_F(DocumentProviderTest, CheckFeatureNotInExplicitKeywordMode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {omnibox::kDocumentProvider, omnibox::kExperimentalKeywordMode}, {});
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsAuthenticated()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsSyncActive()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));

  // Prevent document search results in explicit keyword mode.
  {
    AutocompleteInput input(base::ASCIIToUTF16("wikipedia.org soup"),
                            metrics::OmniboxEventProto::NTP,
                            TestSchemeClassifier());
    input.set_prefer_keyword(true);

    EXPECT_FALSE(provider_->IsDocumentProviderAllowed(client_.get(), input));
  }
  {
    AutocompleteInput input(base::ASCIIToUTF16("amazon.com soup"),
                            metrics::OmniboxEventProto::NTP,
                            TestSchemeClassifier());
    input.set_prefer_keyword(true);

    EXPECT_TRUE(provider_->IsDocumentProviderAllowed(client_.get(), input));
  }
  {
    AutocompleteInput input(base::ASCIIToUTF16("drive.google.com soup"),
                            metrics::OmniboxEventProto::NTP,
                            TestSchemeClassifier());
    input.set_prefer_keyword(true);

    EXPECT_TRUE(provider_->IsDocumentProviderAllowed(client_.get(), input));
  }
}

TEST_F(DocumentProviderTest, CheckFeaturePrerequisiteServerBackoff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kDocumentProvider);
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsAuthenticated()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsSyncActive()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));

  // Feature starts enabled.
  EXPECT_TRUE(
      provider_->IsDocumentProviderAllowed(client_.get(), AutocompleteInput()));

  // Server setting backoff flag disables it.
  provider_->backoff_for_session_ = true;
  EXPECT_FALSE(
      provider_->IsDocumentProviderAllowed(client_.get(), AutocompleteInput()));
  provider_->backoff_for_session_ = false;
}

TEST_F(DocumentProviderTest, IsInputLikelyURL) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kDocumentProvider);

  auto IsInputLikelyURL_Wrapper = [](const std::string& input_ascii) {
    const AutocompleteInput autocomplete_input(
        base::ASCIIToUTF16(input_ascii), metrics::OmniboxEventProto::OTHER,
        TestSchemeClassifier());
    return DocumentProvider::IsInputLikelyURL(autocomplete_input);
  };

  EXPECT_TRUE(IsInputLikelyURL_Wrapper("htt"));
  EXPECT_TRUE(IsInputLikelyURL_Wrapper("http"));
  EXPECT_TRUE(IsInputLikelyURL_Wrapper("https"));
  EXPECT_TRUE(IsInputLikelyURL_Wrapper("https://"));
  EXPECT_TRUE(IsInputLikelyURL_Wrapper("http://web.site"));
  EXPECT_TRUE(IsInputLikelyURL_Wrapper("https://web.site"));
  EXPECT_TRUE(IsInputLikelyURL_Wrapper("https://web.site"));
  EXPECT_TRUE(IsInputLikelyURL_Wrapper("w"));
  EXPECT_TRUE(IsInputLikelyURL_Wrapper("www."));
  EXPECT_TRUE(IsInputLikelyURL_Wrapper("www.web.site"));
  EXPECT_TRUE(IsInputLikelyURL_Wrapper("chrome://extensions"));
  EXPECT_FALSE(IsInputLikelyURL_Wrapper("https certificate"));
  EXPECT_FALSE(IsInputLikelyURL_Wrapper("www website hosting"));
  EXPECT_FALSE(IsInputLikelyURL_Wrapper("text query"));
}

TEST_F(DocumentProviderTest, ParseDocumentSearchResults) {
  const std::string kGoodJSONResponse = base::StringPrintf(
      R"({
      "results": [
        {
          "title": "Document 1",
          "url": "https://documentprovider.tld/doc?id=1",
          "score": 1234,
          "originalUrl": "%s"
        },
        {
          "title": "Document 2",
          "url": "https://documentprovider.tld/doc?id=2"
        }
      ]
     })",
      SAMPLE_ORIGINAL_URL.c_str());

  base::Optional<base::Value> response =
      base::JSONReader::Read(kGoodJSONResponse);
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->is_dict());

  provider_->input_.UpdateText(base::UTF8ToUTF16("input"), 0, {});
  ACMatches matches = provider_->ParseDocumentSearchResults(*response);
  EXPECT_EQ(matches.size(), 2u);

  EXPECT_EQ(matches[0].contents, base::ASCIIToUTF16("Document 1"));
  EXPECT_EQ(matches[0].destination_url,
            GURL("https://documentprovider.tld/doc?id=1"));
  EXPECT_EQ(matches[0].relevance, 1234);  // Server-specified.
  EXPECT_EQ(matches[0].stripped_destination_url, GURL(SAMPLE_STRIPPED_URL));

  EXPECT_EQ(matches[1].contents, base::ASCIIToUTF16("Document 2"));
  EXPECT_EQ(matches[1].destination_url,
            GURL("https://documentprovider.tld/doc?id=2"));
  EXPECT_EQ(matches[1].relevance, 0);
  EXPECT_TRUE(matches[1].stripped_destination_url.is_empty());

  ASSERT_FALSE(provider_->backoff_for_session_);
}

TEST_F(DocumentProviderTest, ProductDescriptionStringsAndAccessibleLabels) {
  // Dates are kept > 1 year in the past since
  // See comments for GenerateLastModifiedString in this file for references.
  const std::string kGoodJSONResponseWithMimeTypes = base::StringPrintf(
      R"({
      "results": [
        {
          "title": "My Google Doc",
          "url": "https://documentprovider.tld/doc?id=1",
          "score": 999,
          "originalUrl": "%s",
          "metadata": {
            "mimeType": "application/vnd.google-apps.document",
            "updateTime": "Mon, 15 Oct 2007 19:45:00 GMT"
          }
        },
        {
          "title": "My File in Drive",
          "score": 998,
          "url": "https://documentprovider.tld/doc?id=2",
          "metadata": {
            "mimeType": "application/vnd.foocorp.file",
            "updateTime": "10 Oct 2010 19:45:00 GMT"
          }
        },
        {
          "title": "Shared Spreadsheet",
          "score": 997,
          "url": "https://documentprovider.tld/doc?id=3",
          "metadata": {
            "mimeType": "application/vnd.google-apps.spreadsheet"
          }
        }
      ]
     })",
      SAMPLE_ORIGINAL_URL.c_str());

  base::Optional<base::Value> response =
      base::JSONReader::Read(kGoodJSONResponseWithMimeTypes);
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->is_dict());

  provider_->input_.UpdateText(base::UTF8ToUTF16("input"), 0, {});
  ACMatches matches = provider_->ParseDocumentSearchResults(*response);
  EXPECT_EQ(matches.size(), 3u);

  // match.destination_url is used as the match's temporary text in the Omnibox.
  EXPECT_EQ(
      AutocompleteMatchType::ToAccessibilityLabel(
          matches[0], base::ASCIIToUTF16(matches[0].destination_url.spec()), 1,
          4, false),
      base::ASCIIToUTF16("My Google Doc, 10/15/07 - Google Docs, "
                         "https://documentprovider.tld/doc?id=1, 2 of 4"));
  // Unhandled MIME Type falls back to "Google Drive" where the file was stored.
  EXPECT_EQ(
      AutocompleteMatchType::ToAccessibilityLabel(
          matches[1], base::ASCIIToUTF16(matches[1].destination_url.spec()), 2,
          4, false),
      base::ASCIIToUTF16("My File in Drive, 10/10/10 - Google Drive, "
                         "https://documentprovider.tld/doc?id=2, 3 of 4"));
  // No modified time was specified for the last file.
  EXPECT_EQ(
      AutocompleteMatchType::ToAccessibilityLabel(
          matches[2], base::ASCIIToUTF16(matches[2].destination_url.spec()), 3,
          4, false),
      base::ASCIIToUTF16("Shared Spreadsheet, Google Sheets, "
                         "https://documentprovider.tld/doc?id=3, 4 of 4"));
}

TEST_F(DocumentProviderTest, ParseDocumentSearchResultsBreakTies) {
  const std::string kGoodJSONResponseWithTies = base::StringPrintf(
      R"({
      "results": [
        {
          "title": "Document 1",
          "url": "https://documentprovider.tld/doc?id=1",
          "score": 1234,
          "originalUrl": "%s"
        },
        {
          "title": "Document 2",
          "score": 1234,
          "url": "https://documentprovider.tld/doc?id=2"
        },
        {
          "title": "Document 3",
          "score": 1234,
          "url": "https://documentprovider.tld/doc?id=3"
        }
      ]
     })",
      SAMPLE_ORIGINAL_URL.c_str());

  base::Optional<base::Value> response =
      base::JSONReader::Read(kGoodJSONResponseWithTies);
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->is_dict());

  provider_->input_.UpdateText(base::UTF8ToUTF16("input"), 0, {});
  ACMatches matches = provider_->ParseDocumentSearchResults(*response);
  EXPECT_EQ(matches.size(), 3u);

  // Server is suggesting relevances of [1234, 1234, 1234]
  // We should break ties to [1234, 1233, 1232]
  EXPECT_EQ(matches[0].contents, base::ASCIIToUTF16("Document 1"));
  EXPECT_EQ(matches[0].destination_url,
            GURL("https://documentprovider.tld/doc?id=1"));
  EXPECT_EQ(matches[0].relevance, 1234);  // As the server specified.
  EXPECT_EQ(matches[0].stripped_destination_url, GURL(SAMPLE_STRIPPED_URL));

  EXPECT_EQ(matches[1].contents, base::ASCIIToUTF16("Document 2"));
  EXPECT_EQ(matches[1].destination_url,
            GURL("https://documentprovider.tld/doc?id=2"));
  EXPECT_EQ(matches[1].relevance, 1233);  // Tie demoted
  EXPECT_TRUE(matches[1].stripped_destination_url.is_empty());

  EXPECT_EQ(matches[2].contents, base::ASCIIToUTF16("Document 3"));
  EXPECT_EQ(matches[2].destination_url,
            GURL("https://documentprovider.tld/doc?id=3"));
  EXPECT_EQ(matches[2].relevance, 1232);  // Tie demoted, twice.
  EXPECT_TRUE(matches[2].stripped_destination_url.is_empty());

  ASSERT_FALSE(provider_->backoff_for_session_);
}

TEST_F(DocumentProviderTest, ParseDocumentSearchResultsBreakTiesCascade) {
  const std::string kGoodJSONResponseWithTies = base::StringPrintf(
      R"({
      "results": [
        {
          "title": "Document 1",
          "url": "https://documentprovider.tld/doc?id=1",
          "score": 1234,
          "originalUrl": "%s"
        },
        {
          "title": "Document 2",
          "score": 1234,
          "url": "https://documentprovider.tld/doc?id=2"
        },
        {
          "title": "Document 3",
          "score": 1233,
          "url": "https://documentprovider.tld/doc?id=3"
        }
      ]
     })",
      SAMPLE_ORIGINAL_URL.c_str());

  base::Optional<base::Value> response =
      base::JSONReader::Read(kGoodJSONResponseWithTies);
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->is_dict());

  provider_->input_.UpdateText(base::UTF8ToUTF16("input"), 0, {});
  ACMatches matches = provider_->ParseDocumentSearchResults(*response);
  EXPECT_EQ(matches.size(), 3u);

  // Server is suggesting relevances of [1233, 1234, 1233, 1000, 1000]
  // We should break ties to [1234, 1233, 1232, 1000, 999]
  EXPECT_EQ(matches[0].contents, base::ASCIIToUTF16("Document 1"));
  EXPECT_EQ(matches[0].destination_url,
            GURL("https://documentprovider.tld/doc?id=1"));
  EXPECT_EQ(matches[0].relevance, 1234);  // As the server specified.
  EXPECT_EQ(matches[0].stripped_destination_url, GURL(SAMPLE_STRIPPED_URL));

  EXPECT_EQ(matches[1].contents, base::ASCIIToUTF16("Document 2"));
  EXPECT_EQ(matches[1].destination_url,
            GURL("https://documentprovider.tld/doc?id=2"));
  EXPECT_EQ(matches[1].relevance, 1233);  // Tie demoted
  EXPECT_TRUE(matches[1].stripped_destination_url.is_empty());

  EXPECT_EQ(matches[2].contents, base::ASCIIToUTF16("Document 3"));
  EXPECT_EQ(matches[2].destination_url,
            GURL("https://documentprovider.tld/doc?id=3"));
  // Document 2's demotion caused an implicit tie.
  // Ensure we demote this one as well.
  EXPECT_EQ(matches[2].relevance, 1232);
  EXPECT_TRUE(matches[2].stripped_destination_url.is_empty());

  ASSERT_FALSE(provider_->backoff_for_session_);
}

TEST_F(DocumentProviderTest, ParseDocumentSearchResultsBreakTiesZeroLimit) {
  const std::string kGoodJSONResponseWithTies = base::StringPrintf(
      R"({
      "results": [
        {
          "title": "Document 1",
          "url": "https://documentprovider.tld/doc?id=1",
          "score": 1,
          "originalUrl": "%s"
        },
        {
          "title": "Document 2",
          "score": 1,
          "url": "https://documentprovider.tld/doc?id=2"
        },
        {
          "title": "Document 3",
          "score": 1,
          "url": "https://documentprovider.tld/doc?id=3"
        }
      ]
     })",
      SAMPLE_ORIGINAL_URL.c_str());

  base::Optional<base::Value> response =
      base::JSONReader::Read(kGoodJSONResponseWithTies);
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->is_dict());

  provider_->input_.UpdateText(base::UTF8ToUTF16("input"), 0, {});
  ACMatches matches = provider_->ParseDocumentSearchResults(*response);
  EXPECT_EQ(matches.size(), 3u);

  // Server is suggesting relevances of [1, 1, 1]
  // We should break ties, but not below zero, to [1, 0, 0]
  EXPECT_EQ(matches[0].contents, base::ASCIIToUTF16("Document 1"));
  EXPECT_EQ(matches[0].destination_url,
            GURL("https://documentprovider.tld/doc?id=1"));
  EXPECT_EQ(matches[0].relevance, 1);  // As the server specified.
  EXPECT_EQ(matches[0].stripped_destination_url, GURL(SAMPLE_STRIPPED_URL));

  EXPECT_EQ(matches[1].contents, base::ASCIIToUTF16("Document 2"));
  EXPECT_EQ(matches[1].destination_url,
            GURL("https://documentprovider.tld/doc?id=2"));
  EXPECT_EQ(matches[1].relevance, 0);  // Tie demoted
  EXPECT_TRUE(matches[1].stripped_destination_url.is_empty());

  EXPECT_EQ(matches[2].contents, base::ASCIIToUTF16("Document 3"));
  EXPECT_EQ(matches[2].destination_url,
            GURL("https://documentprovider.tld/doc?id=3"));
  // Tie is demoted further.
  EXPECT_EQ(matches[2].relevance, 0);
  EXPECT_TRUE(matches[2].stripped_destination_url.is_empty());

  ASSERT_FALSE(provider_->backoff_for_session_);
}

TEST_F(DocumentProviderTest, ParseDocumentSearchResultsWithBackoff) {
  // Response where the server wishes to trigger backoff.
  const char kBackoffJSONResponse[] = R"({
      "error": {
        "code": 503,
        "message": "Not eligible to query, see retry info.",
        "status": "UNAVAILABLE",
        "details": [
          {
            "@type": "type.googleapis.com/google.rpc.RetryInfo",
            "retryDelay": "100000s"
          },
        ]
      }
    })";

  ASSERT_FALSE(provider_->backoff_for_session_);
  base::Optional<base::Value> backoff_response = base::JSONReader::Read(
      kBackoffJSONResponse, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(backoff_response);
  ASSERT_TRUE(backoff_response->is_dict());

  ACMatches matches = provider_->ParseDocumentSearchResults(*backoff_response);
  ASSERT_TRUE(provider_->backoff_for_session_);
}

TEST_F(DocumentProviderTest, ParseDocumentSearchResultsWithIneligibleFlag) {
  // Response where the server wishes to trigger backoff.
  const char kIneligibleJSONResponse[] = R"({
      "error": {
        "code": 403,
        "message": "Not eligible to query due to admin disabled Chrome search settings.",
        "status": "PERMISSION_DENIED",
      }
    })";

  // Same as above, but the message doesn't match. We should accept this
  // response, but it isn't expected to trigger backoff.
  const char kMismatchedMessageJSON[] = R"({
      "error": {
        "code": 403,
        "message": "Some other thing went wrong.",
        "status": "PERMISSION_DENIED",
      }
    })";

  ACMatches matches;
  ASSERT_FALSE(provider_->backoff_for_session_);

  // First, parse an invalid response - shouldn't prohibit future requests
  // from working but also shouldn't trigger backoff.
  base::Optional<base::Value> bad_response = base::JSONReader::Read(
      kMismatchedMessageJSON, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(bad_response);
  ASSERT_TRUE(bad_response->is_dict());
  matches = provider_->ParseDocumentSearchResults(*bad_response);
  ASSERT_FALSE(provider_->backoff_for_session_);

  // Now parse a response that does trigger backoff.
  base::Optional<base::Value> backoff_response = base::JSONReader::Read(
      kIneligibleJSONResponse, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(backoff_response);
  ASSERT_TRUE(backoff_response->is_dict());
  matches = provider_->ParseDocumentSearchResults(*backoff_response);
  ASSERT_TRUE(provider_->backoff_for_session_);
}

// This test is affected by an iOS 10 simulator bug: https://crbug.com/782033
// and may get wrong timezone on Win7: https://crbug.com/856119
#if !defined(OS_IOS) && !defined(OS_WIN)
TEST_F(DocumentProviderTest, GenerateLastModifiedString) {
  base::Time::Exploded local_exploded = {0};
  local_exploded.year = 2018;
  local_exploded.month = 8;
  local_exploded.day_of_month = 27;
  local_exploded.hour = 3;
  local_exploded.minute = 18;
  local_exploded.second = 54;
  base::Time local_now;
  EXPECT_TRUE(base::Time::FromLocalExploded(local_exploded, &local_now));

  base::Time modified_today = local_now + base::TimeDelta::FromHours(-1);
  base::Time modified_this_year = local_now + base::TimeDelta::FromDays(-8);
  base::Time modified_last_year = local_now + base::TimeDelta::FromDays(-365);

  // GenerateLastModifiedString should accept any parseable timestamp, but use
  // ISO8601 UTC timestamp strings since the service returns them in practice.
  EXPECT_EQ(DocumentProvider::GenerateLastModifiedString(
                base::TimeToISO8601(modified_today), local_now),
            base::ASCIIToUTF16("2:18 AM"));
  EXPECT_EQ(DocumentProvider::GenerateLastModifiedString(
                base::TimeToISO8601(modified_this_year), local_now),
            base::ASCIIToUTF16("Aug 19"));
  EXPECT_EQ(DocumentProvider::GenerateLastModifiedString(
                base::TimeToISO8601(modified_last_year), local_now),
            base::ASCIIToUTF16("8/27/17"));
}
#endif  // !defined(OS_IOS)

TEST_F(DocumentProviderTest, GetURLForDeduping) {
  // Checks that |url_string| is a URL for opening |expected_id|. An empty ID
  // signifies |url_string| is not a Drive document.
  auto CheckDeduper = [](const std::string& url_string,
                         const std::string& expected_id) {
    const GURL url(url_string);
    const GURL got_output = DocumentProvider::GetURLForDeduping(url);

    const GURL expected_output;
    if (!expected_id.empty()) {
      EXPECT_EQ(got_output,
                GURL("https://drive.google.com/open?id=" + expected_id))
          << url_string;
    } else {
      EXPECT_EQ(got_output, GURL()) << url_string;
    }
  };

  // Turning clang-format off to avoid wrapping the URLs which makes them harder
  // to search, copy/navigate, and edit.
  // clang-format off

  // Various hosts (e.g. docs).
  CheckDeduper("https://docs.google.com/a/google.com/document/d/tH3_d0C-1d/edit", "tH3_d0C-1d");
  CheckDeduper("https://drive.google.com/a/google.com/document/d/tH3_d0C-1d/edit", "tH3_d0C-1d");
  CheckDeduper("https://spreadsheets.google.com/a/google.com/document/d/tH3_d0C-1d/edit", "tH3_d0C-1d");
  CheckDeduper("https://script.google.com/a/google.com/document/d/tH3_d0C-1d/edit", "tH3_d0C-1d");
  CheckDeduper("https://sites.google.com/a/google.com/document/d/tH3_d0C-1d/edit", "tH3_d0C-1d");
  // Without domain in path (e.g. a/google.com/).
  CheckDeduper("https://docs.google.com/document/d/tH3_d0C-1d/edit", "tH3_d0C-1d");
  CheckDeduper("https://drive.google.com/document/d/tH3_d0C-1d/edit", "tH3_d0C-1d");
  // Non-document paths (e.g. presentation).
  CheckDeduper("https://docs.google.com/a/google.com/presentation/d/tH3_d0C-1d", "tH3_d0C-1d");
  CheckDeduper("https://drive.google.com/a/google.com/spreadsheets/d/tH3_d0C-1d", "tH3_d0C-1d");
  CheckDeduper("https://docs.google.com/098/d/tH3_d0C-1d", "tH3_d0C-1d");
  // With various action suffixes (e.g. view).
  CheckDeduper("https://docs.google.com/a/google.com/forms/d/tH3_d0C-1d/view", "tH3_d0C-1d");
  CheckDeduper("https://spreadsheets.google.com/spreadsheets/d/tH3_d0C-1d/comment", "tH3_d0C-1d");
  CheckDeduper("https://docs.google.com/spreadsheets/d/tH3_d0C-1d/view", "tH3_d0C-1d");
  CheckDeduper("https://drive.google.com/spreadsheets/d/tH3_d0C-1d/089", "tH3_d0C-1d");
  CheckDeduper("https://docs.google.com/file/d/tH3_d0C-1d", "tH3_d0C-1d");
  // With query params.
  CheckDeduper("https://docs.google.com/a/google.com/forms/d/tH3_d0C-1d?usp=drive_web", "tH3_d0C-1d");
  CheckDeduper("https://drive.google.com/a/google.com/file/d/tH3_d0C-1d/comment?usp=drive_web", "tH3_d0C-1d");
  CheckDeduper("https://drive.google.com/presentation/d/tH3_d0C-1d/edit?usp=drive_web", "tH3_d0C-1d");
  CheckDeduper("https://docs.google.com/presentation/d/tH3_d0C-1d/edit#slide=id.abc_0_789", "tH3_d0C-1d");
  CheckDeduper("https://drive.google.com/file/d/tH3_d0C-1d/789", "tH3_d0C-1d");
  CheckDeduper("https://docs.google.com/spreadsheets/d/tH3_d0C-1d/preview?x=1#y=2", "tH3_d0C-1d");
  // With non-google domains.
  CheckDeduper("https://docs.google.com/a/rand.com/forms/d/tH3_d0C-1d/edit", "tH3_d0C-1d");
  CheckDeduper("https://sites.google.com/a/rand.om.org/file/d/tH3_d0C-1d/view", "tH3_d0C-1d");
  CheckDeduper("https://docs.google.com/spreadsheets/d/tH3_d0C-1d/edit", "tH3_d0C-1d");
  CheckDeduper("https://drive.google.com/presentation/d/tH3_d0C-1d/comment", "tH3_d0C-1d");
  CheckDeduper("https://script.google.com/a/domain/spreadsheets/d/tH3_d0C-1d/preview?x=1#y=2", "tH3_d0C-1d");
  // Open.
  CheckDeduper("https://drive.google.com/open?id=tH3_d0C-1d", "tH3_d0C-1d");
  CheckDeduper("https://docs.google.com/a/google.com/open?x=prefix&id=tH3_d0C-1d&y=suffix", "tH3_d0C-1d");
  CheckDeduper("https://drive.google.com/a/domain.com/open?id=tH3_d0C-1d&y=suffix/edit", "tH3_d0C-1d");
  CheckDeduper("https://docs.google.com/open?x=prefix&id=tH3_d0C-1d", "tH3_d0C-1d");
  CheckDeduper("https://script.google.com/open?id=tH3_d0C-1d", "tH3_d0C-1d");
  // Viewform examples.
  CheckDeduper("https://drive.google.com/a/google.com/forms/d/e/tH3_d0C-1d/viewform", "tH3_d0C-1d");
  CheckDeduper("https://drive.google.com/a/google.com/forms/d/e/tH3_d0C-1d/viewform", "tH3_d0C-1d");
  CheckDeduper("https://docs.google.com/forms/d/e/tH3_d0C-1d/viewform", "tH3_d0C-1d");
  CheckDeduper("https://drive.google.com/forms/d/e/tH3_d0C-1d/viewform", "tH3_d0C-1d");
  // File and folder.
  CheckDeduper("https://docs.google.com/a/google.com/drive/folders/tH3_d0C-1d", "tH3_d0C-1d");
  CheckDeduper("https://drive.google.com/drive/folders/tH3_d0C-1d", "tH3_d0C-1d");
  CheckDeduper("https://docs.google.com/file/d/tH3_d0C-1d/view?usp=sharing", "tH3_d0C-1d");
  CheckDeduper("https://drive.google.com/a/google.com/file/d/tH3_d0C-1d/view?usp=sharing", "tH3_d0C-1d");
  // Redirects.
  CheckDeduper("https://www.google.com/url?q=https://docs.google.com/a/google.com/document/d/tH3_d0C-1d/edit", "tH3_d0C-1d");
  CheckDeduper("https://www.google.com/url?sa=t&url=https://docs.google.com/a/google.com/document/d/tH3_d0C-1d/edit", "tH3_d0C-1d");
  CheckDeduper("https://www.google.com/url?sa=t&q&url=https://docs.google.com/a/google.com/document/d/tH3_d0C-1d/edit", "tH3_d0C-1d");
  CheckDeduper("https://docs.google.com/accounts?continueUrl=https://docs.google.com/a/google.com/document/d/tH3_d0C-1d/edit", "tH3_d0C-1d");
  CheckDeduper("https://docs.google.com/a/google.com/accounts?continueUrl=https://docs.google.com/a/google.com/document/d/tH3_d0C-1d/edit", "tH3_d0C-1d");
  CheckDeduper("https://drive.google.com/a/google.com/accounts?continueUrl=https://docs.google.com/a/google.com/document/d/tH3_d0C-1d/edit", "tH3_d0C-1d");
  CheckDeduper("https://drive.google.com/accounts?continueUrl=https://docs.google.com/a/google.com/document/d/tH3_d0C-1d/edit", "tH3_d0C-1d");
  // Redirects encoded.
  CheckDeduper("https://www.google.com/url?q=https%3A%2F%2Fdocs.google.com%2Fa%2Fgoogle.com%2Fdocument%2Fd%2FtH3_d0C-1d%2Fedit", "tH3_d0C-1d");
  CheckDeduper("https://www.google.com/url?sa=t&url=https%3A%2F%2Fdocs.google.com%2Fa%2Fgoogle.com%2Fdocument%2Fd%2FtH3_d0C-1d%2Fedit", "tH3_d0C-1d");
  CheckDeduper("https://docs.google.com/accounts?continueUrl=https%3A%2F%2Fdocs.google.com%2Fa%2Fgoogle.com%2Fdocument%2Fd%2FtH3_d0C-1d%2Fedit", "tH3_d0C-1d");
  CheckDeduper("https://docs.google.com/a/google.com/accounts?continueUrl=https%3A%2F%2Fdocs.google.com%2Fa%2Fgoogle.com%2Fdocument%2Fd%2FtH3_d0C-1d%2Fedit", "tH3_d0C-1d");
  CheckDeduper("https://drive.google.com/a/google.com/accounts?continueUrl=https%3A%2F%2Fdocs.google.com%2Fa%2Fgoogle.com%2Fdocument%2Fd%2FtH3_d0C-1d%2Fedit", "tH3_d0C-1d");
  CheckDeduper("https://drive.google.com/accounts?continueUrl=https%3A%2F%2Fdocs.google.com%2Fa%2Fgoogle.com%2Fdocument%2Fd%2FtH3_d0C-1d%2Fedit", "tH3_d0C-1d");

  // URLs that do not represent docs and shouldn't be deduped with doc URLs:
  CheckDeduper("https://support.google.com/a/users/answer/1?id=2", "");
  CheckDeduper("https://www.google.com", "");
  CheckDeduper("https://www.google.com/url?url=https://drive.google.com/homepage", "");
  CheckDeduper("https://www.google.com/url?url=https://www.youtube.com/view", "");
  CheckDeduper("https://notdrive.google.com/?x=https%3A%2F%2Fdocs.google.com%2Fa%2Fgoogle.com%2Fdocument%2Fd%2FtH3_d0C-1d%2Fedit", "");

  // clang-format on
}

TEST_F(DocumentProviderTest, Scoring) {
  auto CheckScoring = [this](
                          const std::map<std::string, std::string> parameters,
                          const std::string& response_str,
                          const std::string& input_text,
                          const std::vector<int> expected_scores) {
    static int invocation = -1;
    invocation++;

    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(omnibox::kDocumentProvider,
                                                    parameters);
    base::Optional<base::Value> response = base::JSONReader::Read(response_str);
    provider_->input_.UpdateText(base::UTF8ToUTF16(input_text), 0, {});
    ACMatches matches = provider_->ParseDocumentSearchResults(*response);

    EXPECT_EQ(matches.size(), expected_scores.size())
        << "invocation " << invocation;
    for (size_t i = 0; i < matches.size(); i++) {
      EXPECT_EQ(matches[i].relevance, expected_scores[i])
          << "Match " << i << " of invocation " << invocation;
    }
  };

  // Server scoring should use server scores with possible demotion of ties.
  CheckScoring(
      {
          {"DocumentUseServerScore", "true"},
          {"DocumentUseClientScore", "false"},
          {"DocumentCapScorePerRank", "false"},
          {"DocumentBoostOwned", "false"},
      },
      R"({"results": [
          {"title": "Document 1", "score": 1000, "url": "url"},
          {"title": "Document 2", "score": 900, "url": "url"},
          {"title": "Document 3", "score": 900, "url": "url"}
        ]})",
      "input", {1000, 900, 899});

  // Server scoring with rank caps.
  CheckScoring(
      {
          {"DocumentUseServerScore", "true"},
          {"DocumentUseClientScore", "false"},
          {"DocumentCapScorePerRank", "true"},
          {"DocumentBoostOwned", "false"},
      },
      R"({"results": [
          {"title": "Document 1", "score": 1150, "url": "url"},
          {"title": "Document 2", "score": 1150, "url": "url"},
          {"title": "Document 3", "score": 1150, "url": "url"}
        ]})",
      "input", {1150, 1100, 900});

  // Server scoring with owner boosting.
  CheckScoring(
      {
          {"DocumentUseServerScore", "true"},
          {"DocumentUseClientScore", "false"},
          {"DocumentCapScorePerRank", "false"},
          {"DocumentBoostOwned", "true"},
      },
      R"({"results": [
          {"title": "Document 1", "score": 1150, "url": "url",
            "metadata": {"owner": {"emailAddresses": [{"emailAddress": ""}]}}},
          {"title": "Document 2", "score": 1150, "url": "url"},
          {"title": "Document 3", "score": 1150, "url": "url"}
        ]})",
      "input", {1150, 950, 949});

  // Client scoring should match each input word at most once.
  CheckScoring(
      {
          {"DocumentUseServerScore", "false"},
          {"DocumentUseClientScore", "true"},
          {"DocumentCapScorePerRank", "false"},
          {"DocumentBoostOwned", "false"},
      },
      R"({"results": [
          {"title": "rainbows", "score": 1000, "url": "url"},
          {"title": "rain bows", "score": 900, "url": "url"},
          {"title": "rain bowss bows bows", "score": 900, "url": "bows",
            "snippet": {"snippet": "bows bows"}}
        ]})",
      "bows", {0, 669, 669});

  // Client scoring should consider snippet but not URL matches
  CheckScoring(
      {
          {"DocumentUseServerScore", "false"},
          {"DocumentUseClientScore", "true"},
          {"DocumentCapScorePerRank", "false"},
          {"DocumentBoostOwned", "false"},
      },
      R"({"results": [
          {"title": "rainbow", "score": 1000, "url": "url"},
          {"title": "rainbow", "score": 900, "url": "bow"},
          {"title": "rainbow", "score": 900, "url": "bow",
            "snippet": {"snippet": "bow bow"}}
        ]})",
      "rain bow", {669, 669, 793});
}

TEST_F(DocumentProviderTest, Caching) {
  auto MakeTestResponse = [](const std::vector<std::string>& doc_ids) {
    std::string results = "";
    for (auto doc_id : doc_ids)
      results += base::StringPrintf(
          R"({
              "title": "Document %s",
              "score": 1150,
              "url": "https://drive.google.com/open?id=%s",
              "originalUrl": "https://drive.google.com/open?id=%s",
            },)",
          doc_id.c_str(), doc_id.c_str(), doc_id.c_str());
    return base::StringPrintf(R"({"results": [%s]})", results.c_str());
  };

  auto GetTestProviderMatches = [this](const std::string& input_text,
                                       const std::string& response_str) {
    provider_->input_.UpdateText(base::UTF8ToUTF16(input_text), 0, {});
    provider_->UpdateResults(response_str);
    return provider_->matches_;
  };

  // Partially fill the cache as setup for following tests.
  auto matches =
      GetTestProviderMatches("input", MakeTestResponse({"0", "1", "2"}));
  EXPECT_EQ(matches.size(), size_t(3));
  EXPECT_EQ(matches[0].contents, base::UTF8ToUTF16("Document 0"));
  EXPECT_EQ(matches[1].contents, base::UTF8ToUTF16("Document 1"));
  EXPECT_EQ(matches[2].contents, base::UTF8ToUTF16("Document 2"));

  // Cache should remove duplicates.
  matches = GetTestProviderMatches("input", MakeTestResponse({"1", "2", "3"}));
  EXPECT_EQ(matches.size(), size_t(4));
  EXPECT_EQ(matches[0].contents, base::UTF8ToUTF16("Document 1"));
  EXPECT_EQ(matches[1].contents, base::UTF8ToUTF16("Document 2"));
  EXPECT_EQ(matches[2].contents, base::UTF8ToUTF16("Document 3"));
  EXPECT_EQ(matches[3].contents, base::UTF8ToUTF16("Document 0"));

  // Cache size (4) should not restrict number of matches from the current
  // response.
  matches = GetTestProviderMatches("input", MakeTestResponse({"3", "4", "5"}));
  EXPECT_EQ(matches.size(), size_t(6));
  EXPECT_EQ(matches[0].contents, base::UTF8ToUTF16("Document 3"));
  EXPECT_EQ(matches[1].contents, base::UTF8ToUTF16("Document 4"));
  EXPECT_EQ(matches[2].contents, base::UTF8ToUTF16("Document 5"));
  EXPECT_EQ(matches[3].contents, base::UTF8ToUTF16("Document 1"));
  EXPECT_EQ(matches[4].contents, base::UTF8ToUTF16("Document 2"));
  EXPECT_EQ(matches[5].contents, base::UTF8ToUTF16("Document 0"));

  // Cache size (4) should restrict number of cached matches appended.
  matches = GetTestProviderMatches("input", MakeTestResponse({"0", "4", "6"}));
  EXPECT_EQ(matches.size(), size_t(6));
  EXPECT_EQ(matches[0].contents, base::UTF8ToUTF16("Document 0"));
  EXPECT_EQ(matches[1].contents, base::UTF8ToUTF16("Document 4"));
  EXPECT_EQ(matches[2].contents, base::UTF8ToUTF16("Document 6"));
  EXPECT_EQ(matches[3].contents, base::UTF8ToUTF16("Document 3"));
  EXPECT_EQ(matches[4].contents, base::UTF8ToUTF16("Document 5"));
  EXPECT_EQ(matches[5].contents, base::UTF8ToUTF16("Document 1"));

  // Cached results should update match |additional_info|, |relevance|, and
  // |contents_class|.
  matches = GetTestProviderMatches("docum", MakeTestResponse({"5", "4", "7"}));
  EXPECT_EQ(matches.size(), size_t(6));
  EXPECT_EQ(matches[0].contents, base::UTF8ToUTF16("Document 5"));
  EXPECT_EQ(matches[0].GetAdditionalInfo("from cache"), "");
  EXPECT_EQ(matches[0].relevance, 1150);
  EXPECT_THAT(matches[0].contents_class,
              testing::ElementsAre(ACMatchClassification{0, 2},
                                   ACMatchClassification{5, 0}));
  EXPECT_EQ(matches[1].contents, base::UTF8ToUTF16("Document 4"));
  EXPECT_EQ(matches[1].GetAdditionalInfo("from cache"), "");
  EXPECT_EQ(matches[1].relevance, 1149);
  EXPECT_THAT(matches[1].contents_class,
              testing::ElementsAre(ACMatchClassification{0, 2},
                                   ACMatchClassification{5, 0}));
  EXPECT_EQ(matches[2].contents, base::UTF8ToUTF16("Document 7"));
  EXPECT_EQ(matches[2].GetAdditionalInfo("from cache"), "");
  EXPECT_EQ(matches[2].relevance, 1148);
  EXPECT_THAT(matches[2].contents_class,
              testing::ElementsAre(ACMatchClassification{0, 2},
                                   ACMatchClassification{5, 0}));
  EXPECT_EQ(matches[3].contents, base::UTF8ToUTF16("Document 0"));
  EXPECT_EQ(matches[3].GetAdditionalInfo("from cache"), "true");
  EXPECT_EQ(matches[3].relevance, 0);
  EXPECT_THAT(matches[3].contents_class,
              testing::ElementsAre(ACMatchClassification{0, 2},
                                   ACMatchClassification{5, 0}));
  EXPECT_EQ(matches[4].contents, base::UTF8ToUTF16("Document 6"));
  EXPECT_EQ(matches[4].GetAdditionalInfo("from cache"), "true");
  EXPECT_EQ(matches[4].relevance, 0);
  EXPECT_THAT(matches[4].contents_class,
              testing::ElementsAre(ACMatchClassification{0, 2},
                                   ACMatchClassification{5, 0}));
  EXPECT_EQ(matches[5].contents, base::UTF8ToUTF16("Document 3"));
  EXPECT_EQ(matches[5].GetAdditionalInfo("from cache"), "true");
  EXPECT_EQ(matches[5].relevance, 0);
  EXPECT_THAT(matches[5].contents_class,
              testing::ElementsAre(ACMatchClassification{0, 2},
                                   ACMatchClassification{5, 0}));
}

TEST_F(DocumentProviderTest, MinQueryLength) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kDocumentProvider);
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsAuthenticated()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsSyncActive()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));

  // Expect document provider to ignore inputs shorter than min_query_length_.
  AutocompleteInput short_input(base::ASCIIToUTF16("12"),
                                metrics::OmniboxEventProto::OTHER,
                                TestSchemeClassifier());
  short_input.set_want_asynchronous_matches(false);
  provider_->Start(short_input, false);
  EXPECT_NE(short_input.text(), provider_->input_.text());

  // Expect document provider to process inputs longer than min_query_length_.
  AutocompleteInput long_input(base::ASCIIToUTF16("123456"),
                               metrics::OmniboxEventProto::OTHER,
                               TestSchemeClassifier());
  long_input.set_want_asynchronous_matches(false);
  provider_->Start(long_input, false);
  EXPECT_EQ(long_input.text(), provider_->input_.text());
}
