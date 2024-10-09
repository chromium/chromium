// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/document_provider.h"

#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kSampleOriginalURL[] =
    "https://www.google.com/url?url=https://drive.google.com/a/domain.tld/"
    "open?id%3D_0123_ID_4567_&_placeholder_";

const char kSampleStrippedURL[] =
    "https://drive.google.com/open?id=_0123_ID_4567_";

using testing::Return;

class FakeAutocompleteProviderClient : public MockAutocompleteProviderClient {
 public:
  FakeAutocompleteProviderClient() {
    set_template_url_service(
        search_engines_test_environment_.template_url_service());
    search_engines_test_environment_.pref_service()
        .registry()
        ->RegisterBooleanPref(omnibox::kDocumentSuggestEnabled, true);
  }

  ~FakeAutocompleteProviderClient() {
    // We do that because the `TemplateURLService` object lives in
    // `MockAutocompleteProviderClient` which is the parent class while its pref
    // live in `SearchEnginesTestEnvironment`.
    set_template_url_service(nullptr);
  }

  FakeAutocompleteProviderClient(const FakeAutocompleteProviderClient&) =
      delete;
  FakeAutocompleteProviderClient& operator=(
      const FakeAutocompleteProviderClient&) = delete;

  bool SearchSuggestEnabled() const override { return true; }

  PrefService* GetPrefs() const override {
    return &search_engines_test_environment_.pref_service();
  }

  std::string ProfileUserName() const override { return "goodEmail@gmail.com"; }

 private:
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
};

}  // namespace

class FakeDocumentProvider : public DocumentProvider {
 public:
  FakeDocumentProvider(AutocompleteProviderClient* client,
                       AutocompleteProviderListener* listener)
      : DocumentProvider(client, listener) {
    matches_cache_ = MatchesCache(4);
  }

  using DocumentProvider::backoff_for_session_;
  using DocumentProvider::done_;
  using DocumentProvider::GenerateLastModifiedString;
  using DocumentProvider::input_;
  using DocumentProvider::IsDocumentProviderAllowed;
  using DocumentProvider::IsInputLikelyURL;
  using DocumentProvider::matches_;
  using DocumentProvider::OnDocumentSuggestionsLoaderAvailable;
  using DocumentProvider::ParseDocumentSearchResults;
  using DocumentProvider::time_run_invoked_;
  using DocumentProvider::UpdateResults;

 protected:
  ~FakeDocumentProvider() override = default;
};

class DocumentProviderTest : public testing::Test,
                             public AutocompleteProviderListener {
 public:
  DocumentProviderTest();
  DocumentProviderTest(const DocumentProviderTest&) = delete;
  DocumentProviderTest& operator=(const DocumentProviderTest&) = delete;

  void SetUp() override;

 protected:
  // AutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches,
                        const AutocompleteProvider* provider) override;

  // Set's up |client_| call expectations to enable the doc suggestions; i.e. so
  // that |IsDocumentProviderAllowed()| returns true. This is not necessary when
  // invoking helper methods directly, but is required when invoking |Start()|.
  void InitClient();

  // Return a mock server response containing 1 doc per ID in |doc_ids|.
  static std::string MakeTestResponse(const std::vector<std::string>& doc_ids,
                                      int scores) {
    std::string results = "";
    for (auto doc_id : doc_ids)
      results += base::StringPrintf(
          R"({
              "title": "Document %s longer title",
              "score": %d,
              "url": "https://drive.google.com/open?id=%s",
              "originalUrl": "https://drive.google.com/open?id=%s",
            },)",
          doc_id.c_str(), scores, doc_id.c_str(), doc_id.c_str());
    return base::StringPrintf(R"({"results": [%s]})", results.c_str());
  }

  // Convert matches to tuples of `contents`, `relevance`, & `from cache`.
  using Summary = std::tuple<const std::u16string, int, bool>;
  static std::vector<Summary> ExtractMatchSummary(const ACMatches& matches) {
    std::vector<Summary> summaries;
    base::ranges::transform(
        matches, std::back_inserter(summaries), [](const auto& match) {
          return Summary{
              match.contents, match.relevance,
              match.GetAdditionalInfoForDebugging("from cache") == "true"};
        });
    return summaries;
  }

  // Not enabled by default on mobile, so have to enable it explicitly.
  base::test::ScopedFeatureList feature_list_{omnibox::kDocumentProvider};
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<FakeDocumentProvider> provider_;
  raw_ptr<TemplateURL> default_template_url_;
};

DocumentProviderTest::DocumentProviderTest() = default;

void DocumentProviderTest::SetUp() {
  client_ = std::make_unique<FakeAutocompleteProviderClient>();

  TemplateURLService* turl_model = client_->GetTemplateURLService();
  turl_model->Load();

  TemplateURLData data;
  data.SetShortName(u"t");
  data.SetURL("https://www.google.com/?q={searchTerms}");
  data.suggestions_url = "https://www.google.com/complete/?q={searchTerms}";
  default_template_url_ = turl_model->Add(std::make_unique<TemplateURL>(data));
  turl_model->SetUserSelectedDefaultSearchProvider(default_template_url_);

  // Add a keyword provider.
  data.SetShortName(u"wiki");
  data.SetKeyword(u"wikipedia.org");
  data.SetURL("https://en.wikipedia.org/w/index.php?search={searchTerms}");
  data.suggestions_url =
      "https://en.wikipedia.org/w/index.php?search={searchTerms}";
  turl_model->Add(std::make_unique<TemplateURL>(data));

  // Add another.
  data.SetShortName(u"drive");
  data.SetKeyword(u"drive.google.com");
  data.SetURL("https://drive.google.com/drive/search?q={searchTerms}");
  data.suggestions_url =
      "https://drive.google.com/drive/search?q={searchTerms}";
  turl_model->Add(std::make_unique<TemplateURL>(data));

  provider_ = new FakeDocumentProvider(client_.get(), this);
}

void DocumentProviderTest::OnProviderUpdate(
    bool updated_matches,
    const AutocompleteProvider* provider) {
  // No action required.
}

void DocumentProviderTest::InitClient() {
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsAuthenticated()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsSyncActive()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));
}

TEST_F(DocumentProviderTest, IsDocumentProviderAllowed) {
  // Setup so that all checks pass.
  InitClient();
  AutocompleteInput ac_input = AutocompleteInput(
      u"text text", metrics::OmniboxEventProto::OTHER, TestSchemeClassifier());

  // Check |IsDocumentProviderAllowed()| returns true when all conditions pass.
  EXPECT_TRUE(provider_->IsDocumentProviderAllowed(ac_input));

  // Fail each condition individually and ensure |IsDocumentProviderAllowed()|
  // returns false.

  // Feature must be enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(omnibox::kDocumentProvider);
    EXPECT_FALSE(provider_->IsDocumentProviderAllowed(ac_input));
  }

  // Search suggestions must be enabled.
  EXPECT_CALL(*client_.get(), IsSyncActive()).WillRepeatedly(Return(false));
  EXPECT_FALSE(provider_->IsDocumentProviderAllowed(ac_input));
  EXPECT_CALL(*client_.get(), IsSyncActive()).WillRepeatedly(Return(true));
  EXPECT_TRUE(provider_->IsDocumentProviderAllowed(ac_input));

  // Client-side toggle must be enabled. This should be enabled by default; i.e.
  // we didn't explicitly enable this above.
  PrefService* fake_prefs = client_->GetPrefs();
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(omnibox::kDocumentProviderNoSetting);

    fake_prefs->SetBoolean(omnibox::kDocumentSuggestEnabled, false);
    EXPECT_FALSE(provider_->IsDocumentProviderAllowed(ac_input));
    fake_prefs->SetBoolean(omnibox::kDocumentSuggestEnabled, true);
    EXPECT_TRUE(provider_->IsDocumentProviderAllowed(ac_input));
  }

  // Unless the "no setting" Feature is enabled, in which case the setting state
  // shouldn't matter.
  {
    base::test::ScopedFeatureList feature_list{
        omnibox::kDocumentProviderNoSetting};

    fake_prefs->SetBoolean(omnibox::kDocumentSuggestEnabled, false);
    EXPECT_TRUE(provider_->IsDocumentProviderAllowed(ac_input));
    fake_prefs->SetBoolean(omnibox::kDocumentSuggestEnabled, true);
    EXPECT_TRUE(provider_->IsDocumentProviderAllowed(ac_input));
  }

  // Should not be an incognito window.
  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(true));
  EXPECT_FALSE(provider_->IsDocumentProviderAllowed(ac_input));
  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));
  EXPECT_TRUE(provider_->IsDocumentProviderAllowed(ac_input));

  // Sync should be enabled.
  EXPECT_CALL(*client_.get(), IsSyncActive()).WillRepeatedly(Return(false));
  EXPECT_FALSE(provider_->IsDocumentProviderAllowed(ac_input));
  EXPECT_CALL(*client_.get(), IsSyncActive()).WillRepeatedly(Return(true));
  EXPECT_TRUE(provider_->IsDocumentProviderAllowed(ac_input));

  // Unless the "no sync requirement" Feature is enabled, in which case the Sync
  // state shouldn't matter.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        omnibox::kDocumentProviderNoSyncRequirement);

    EXPECT_CALL(*client_.get(), IsSyncActive()).WillRepeatedly(Return(false));
    EXPECT_TRUE(provider_->IsDocumentProviderAllowed(ac_input));
    EXPECT_CALL(*client_.get(), IsSyncActive()).WillRepeatedly(Return(true));
    EXPECT_TRUE(provider_->IsDocumentProviderAllowed(ac_input));
  }

  // |backoff_for_session_| should be false. This should be the case by default;
  // i.e. we didn't explicitly set this to false above.
  provider_->backoff_for_session_ = true;
  EXPECT_FALSE(provider_->IsDocumentProviderAllowed(ac_input));
  provider_->backoff_for_session_ = false;
  EXPECT_TRUE(provider_->IsDocumentProviderAllowed(ac_input));

  // Google should be the default search provider. This should be the case by
  // default; i.e. we didn't explicitly set this above.
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  TemplateURLData data;
  data.SetShortName(u"t");
  data.SetURL("https://www.notgoogle.com/?q={searchTerms}");
  data.suggestions_url = "https://www.notgoogle.com/complete/?q={searchTerms}";
  TemplateURL* new_default_provider =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(
      new_default_provider);
  EXPECT_FALSE(provider_->IsDocumentProviderAllowed(ac_input));
  template_url_service->SetUserSelectedDefaultSearchProvider(
      default_template_url_);
  template_url_service->Remove(new_default_provider);
  EXPECT_TRUE(provider_->IsDocumentProviderAllowed(ac_input));

  // Input should not be on-focus.
  {
    AutocompleteInput input(u"text text", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
    EXPECT_FALSE(provider_->IsDocumentProviderAllowed(input));
  }

  // Input should not be empty.
  {
    AutocompleteInput input(u"                           ",
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    EXPECT_FALSE(provider_->IsDocumentProviderAllowed(input));
  }

  // Input should be of sufficient length. The default limit is 4, which can't
  // be set here since it's read when the doc provider is constructed.
  {
    AutocompleteInput input(u"12", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    EXPECT_FALSE(provider_->IsDocumentProviderAllowed(input));
  }

  // Input should not look like a URL.
  {
    AutocompleteInput input(u"www.x.com", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
    EXPECT_FALSE(provider_->IsDocumentProviderAllowed(input));
  }
}

TEST_F(DocumentProviderTest, IsInputLikelyURL) {
  auto IsInputLikelyURL_Wrapper = [](const std::string& input_ascii) {
    const AutocompleteInput autocomplete_input(
        base::ASCIIToUTF16(input_ascii), metrics::OmniboxEventProto::OTHER,
        TestSchemeClassifier());
    return FakeDocumentProvider::IsInputLikelyURL(autocomplete_input);
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
          "title": "Document 1 longer title",
          "url": "https://documentprovider.tld/doc?id=1",
          "score": 1234,
          "originalUrl": "%s"
        },
        {
          "title": "Document 2 longer title",
          "url": "https://documentprovider.tld/doc?id=2"
        },
        {
          "title": "Document 3 longer title",
          "url": "https://documentprovider.tld/doc?id=3",
          "originalUrl": "http://sites.google.com/google.com/abc/def"
        }
      ]
     })",
      kSampleOriginalURL);

  std::optional<base::Value> response =
      base::JSONReader::Read(kGoodJSONResponse);
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->is_dict());

  // Docs scores are the min of the server and client scores. To avoid client
  // scores coming into play in this test, set the input to match the title
  // similarly enough that the client score will surpass the server score.
  provider_->input_.UpdateText(u"document longer title", 0, {});
  ACMatches matches = provider_->ParseDocumentSearchResults(*response);
  EXPECT_EQ(matches.size(), 3u);

  EXPECT_EQ(matches[0].contents, u"Document 1 longer title");
  EXPECT_EQ(matches[0].destination_url,
            GURL("https://documentprovider.tld/doc?id=1"));
  EXPECT_EQ(matches[0].relevance, 1234);  // Server-specified.
  EXPECT_EQ(matches[0].stripped_destination_url, GURL(kSampleStrippedURL));
  EXPECT_EQ(matches[0].fill_into_edit,
            base::UTF8ToUTF16(std::string(kSampleOriginalURL)));

  EXPECT_EQ(matches[1].contents, u"Document 2 longer title");
  EXPECT_EQ(matches[1].destination_url,
            GURL("https://documentprovider.tld/doc?id=2"));
  EXPECT_EQ(matches[1].relevance, 0);
  EXPECT_EQ(matches[1].stripped_destination_url,
            GURL("http://documentprovider.tld/doc?id=2"));
  EXPECT_EQ(matches[1].fill_into_edit,
            u"https://documentprovider.tld/doc?id=2");

  EXPECT_EQ(matches[2].contents, u"Document 3 longer title");
  EXPECT_EQ(matches[2].destination_url,
            GURL("https://documentprovider.tld/doc?id=3"));
  EXPECT_EQ(matches[2].relevance, 0);
  // Matches with an original URL that doesn't contain a doc ID should resort to
  // using |AutocompleteMatch::GURLToStrippedGURL()|.
  EXPECT_EQ(matches[2].stripped_destination_url,
            "http://sites.google.com/google.com/abc/def");
  EXPECT_EQ(matches[2].fill_into_edit,
            u"http://sites.google.com/google.com/abc/def");

  EXPECT_FALSE(provider_->backoff_for_session_);
}

#if BUILDFLAG(IS_IOS) && BUILDFLAG(USE_BLINK)
#define MAYBE_ProductDescriptionStringsAndAccessibleLabels \
  DISABLED_ProductDescriptionStringsAndAccessibleLabels
#else
#define MAYBE_ProductDescriptionStringsAndAccessibleLabels \
  ProductDescriptionStringsAndAccessibleLabels
#endif
TEST_F(DocumentProviderTest,
       MAYBE_ProductDescriptionStringsAndAccessibleLabels) {
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
      kSampleOriginalURL);

  std::optional<base::Value> response =
      base::JSONReader::Read(kGoodJSONResponseWithMimeTypes);
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->is_dict());

  provider_->input_.UpdateText(u"input", 0, {});
  ACMatches matches = provider_->ParseDocumentSearchResults(*response);
  EXPECT_EQ(matches.size(), 3u);

  // match.destination_url is used as the match's temporary text in the Omnibox.
  EXPECT_EQ(AutocompleteMatchType::ToAccessibilityLabel(
                matches[0],
                base::ASCIIToUTF16(matches[0].destination_url.spec()), 1, 4),
            u"My Google Doc, 10/15/07 - Google Docs, "
            u"https://documentprovider.tld/doc?id=1, 2 of 4");
  // Unhandled MIME Type falls back to "Google Drive" where the file was stored.
  EXPECT_EQ(AutocompleteMatchType::ToAccessibilityLabel(
                matches[1],
                base::ASCIIToUTF16(matches[1].destination_url.spec()), 2, 4),
            u"My File in Drive, 10/10/10 - Google Drive, "
            "https://documentprovider.tld/doc?id=2, 3 of 4");
  // No modified time was specified for the last file.
  EXPECT_EQ(AutocompleteMatchType::ToAccessibilityLabel(
                matches[2],
                base::ASCIIToUTF16(matches[2].destination_url.spec()), 3, 4),
            u"Shared Spreadsheet, Google Sheets, "
            "https://documentprovider.tld/doc?id=3, 4 of 4");
}

TEST_F(DocumentProviderTest, MatchDescriptionString) {
  const std::string kGoodJSONResponseWithMimeTypes = base::StringPrintf(
      R"({
      "results": [
        {
          "title": "Date, mime, and owner provided",
          "url": "https://documentprovider.tld/doc?id=1",
          "score": 999,
          "originalUrl": "%s",
          "metadata": {
            "updateTime": "1994-01-12T08:10:05Z",
            "mimeType": "application/vnd.google-apps.document",
            "owner": {
              "personNames": [
                {"displayName": "Green Moon"}
              ]
            }
          }
        },
        {
          "title": "Missing mime",
          "score": 998,
          "url": "https://documentprovider.tld/doc?id=2",
          "metadata": {
            "updateTime": "12 Jan 1994 08:10:05 GMT",
            "owner": {
              "personNames": [
                {"displayName": "Blue Sunset"},
                {"displayName": "White Aurora"}
              ]
            }
          }
        },
        {
          "title": "Missing owner",
          "score": 997,
          "url": "https://documentprovider.tld/doc?id=3",
          "metadata": {
            "updateTime": "12 Jan 1994 08:10:05 GMT",
            "mimeType": "application/vnd.google-apps.spreadsheet"
          }
        },
        {
          "title": "Missing date",
          "score": 997,
          "url": "https://documentprovider.tld/doc?id=3",
          "metadata": {
            "mimeType": "application/vnd.google-apps.spreadsheet",
            "owner": {
              "personNames": [
                {"displayName": "Red Lightning"}
              ]
            }
          }
        },
        {
          "title": "Missing metadata",
          "score": 997,
          "url": "https://documentprovider.tld/doc?id=4"
        }
      ]
    })",
      kSampleOriginalURL);

  std::optional<base::Value> response =
      base::JSONReader::Read(kGoodJSONResponseWithMimeTypes);
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->is_dict());
  provider_->input_.UpdateText(u"input", 0, {});

  // Verify correct formatting when displaying owner.
  ACMatches matches = provider_->ParseDocumentSearchResults(*response);

  EXPECT_EQ(matches.size(), 5u);
  EXPECT_EQ(matches[0].description, u"1/12/94 - Green Moon - Google Docs");
  EXPECT_EQ(matches[1].description, u"1/12/94 - Blue Sunset - Google Drive");
  EXPECT_EQ(matches[2].description, u"1/12/94 - Google Sheets");
  EXPECT_EQ(matches[3].description, u"Red Lightning - Google Sheets");
  EXPECT_EQ(matches[4].description, u"");

  // Also verify description_for_shortcuts does not include dates & owners.
  EXPECT_EQ(matches.size(), 5u);
  EXPECT_EQ(matches[0].description_for_shortcuts, u"Google Docs");
  EXPECT_EQ(matches[1].description_for_shortcuts, u"Google Drive");
  EXPECT_EQ(matches[2].description_for_shortcuts, u"Google Sheets");
  EXPECT_EQ(matches[3].description_for_shortcuts, u"Google Sheets");
  EXPECT_EQ(matches[4].description_for_shortcuts, u"");
}

TEST_F(DocumentProviderTest, ParseDocumentSearchResultsBreakTies) {
  // Tie breaking is disabled when client scoring is enabled.
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
      kSampleOriginalURL);

  std::optional<base::Value> response =
      base::JSONReader::Read(kGoodJSONResponseWithTies);
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->is_dict());

  provider_->input_.UpdateText(u"document", 0, {});
  ACMatches matches = provider_->ParseDocumentSearchResults(*response);
  EXPECT_EQ(matches.size(), 3u);

  // Server is suggesting relevances of [1234, 1234, 1234]
  // We should break ties to [1234, 1233, 1232]
  EXPECT_EQ(matches[0].contents, u"Document 1");
  EXPECT_EQ(matches[0].destination_url,
            GURL("https://documentprovider.tld/doc?id=1"));
  EXPECT_EQ(matches[0].relevance, 1234);  // As the server specified.
  EXPECT_EQ(matches[0].stripped_destination_url, GURL(kSampleStrippedURL));

  EXPECT_EQ(matches[1].contents, u"Document 2");
  EXPECT_EQ(matches[1].destination_url,
            GURL("https://documentprovider.tld/doc?id=2"));
  EXPECT_EQ(matches[1].relevance, 1233);  // Tie demoted
  EXPECT_EQ(matches[1].stripped_destination_url,
            GURL("http://documentprovider.tld/doc?id=2"));

  EXPECT_EQ(matches[2].contents, u"Document 3");
  EXPECT_EQ(matches[2].destination_url,
            GURL("https://documentprovider.tld/doc?id=3"));
  EXPECT_EQ(matches[2].relevance, 1232);  // Tie demoted, twice.
  EXPECT_EQ(matches[2].stripped_destination_url,
            GURL("http://documentprovider.tld/doc?id=3"));

  EXPECT_FALSE(provider_->backoff_for_session_);
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
      kSampleOriginalURL);

  std::optional<base::Value> response =
      base::JSONReader::Read(kGoodJSONResponseWithTies);
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->is_dict());

  provider_->input_.UpdateText(u"document", 0, {});
  ACMatches matches = provider_->ParseDocumentSearchResults(*response);
  EXPECT_EQ(matches.size(), 3u);

  // Server is suggesting relevances of [1233, 1234, 1233, 1000, 1000]
  // We should break ties to [1234, 1233, 1232, 1000, 999]
  EXPECT_EQ(matches[0].contents, u"Document 1");
  EXPECT_EQ(matches[0].destination_url,
            GURL("https://documentprovider.tld/doc?id=1"));
  EXPECT_EQ(matches[0].relevance, 1234);  // As the server specified.
  EXPECT_EQ(matches[0].stripped_destination_url, GURL(kSampleStrippedURL));

  EXPECT_EQ(matches[1].contents, u"Document 2");
  EXPECT_EQ(matches[1].destination_url,
            GURL("https://documentprovider.tld/doc?id=2"));
  EXPECT_EQ(matches[1].relevance, 1233);  // Tie demoted
  EXPECT_EQ(matches[1].stripped_destination_url,
            GURL("http://documentprovider.tld/doc?id=2"));

  EXPECT_EQ(matches[2].contents, u"Document 3");
  EXPECT_EQ(matches[2].destination_url,
            GURL("https://documentprovider.tld/doc?id=3"));
  // Document 2's demotion caused an implicit tie.
  // Ensure we demote this one as well.
  EXPECT_EQ(matches[2].relevance, 1232);
  EXPECT_EQ(matches[2].stripped_destination_url,
            GURL("http://documentprovider.tld/doc?id=3"));

  EXPECT_FALSE(provider_->backoff_for_session_);
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
      kSampleOriginalURL);

  std::optional<base::Value> response =
      base::JSONReader::Read(kGoodJSONResponseWithTies);
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->is_dict());

  provider_->input_.UpdateText(u"input", 0, {});
  ACMatches matches = provider_->ParseDocumentSearchResults(*response);
  EXPECT_EQ(matches.size(), 3u);

  // Server is suggesting relevances of [1, 1, 1]
  // We should break ties, but not below zero, to [1, 0, 0]
  EXPECT_EQ(matches[0].contents, u"Document 1");
  EXPECT_EQ(matches[0].destination_url,
            GURL("https://documentprovider.tld/doc?id=1"));
  EXPECT_EQ(matches[0].relevance, 1);  // As the server specified.
  EXPECT_EQ(matches[0].stripped_destination_url, GURL(kSampleStrippedURL));

  EXPECT_EQ(matches[1].contents, u"Document 2");
  EXPECT_EQ(matches[1].destination_url,
            GURL("https://documentprovider.tld/doc?id=2"));
  EXPECT_EQ(matches[1].relevance, 0);  // Tie demoted
  EXPECT_EQ(matches[1].stripped_destination_url,
            GURL("http://documentprovider.tld/doc?id=2"));

  EXPECT_EQ(matches[2].contents, u"Document 3");
  EXPECT_EQ(matches[2].destination_url,
            GURL("https://documentprovider.tld/doc?id=3"));
  // Tie is demoted further.
  EXPECT_EQ(matches[2].relevance, 0);
  EXPECT_EQ(matches[2].stripped_destination_url,
            GURL("http://documentprovider.tld/doc?id=3"));

  EXPECT_FALSE(provider_->backoff_for_session_);
}

TEST_F(DocumentProviderTest, ParseDocumentSearchResultsWithBadResponse) {
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

  std::optional<base::Value> bad_response = base::JSONReader::Read(
      kMismatchedMessageJSON, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(bad_response);
  ASSERT_TRUE(bad_response->is_dict());
  matches = provider_->ParseDocumentSearchResults(*bad_response);
  EXPECT_EQ(matches.size(), 0u);
  // Shouldn't prohibit future requests or trigger backoff.
  EXPECT_FALSE(provider_->backoff_for_session_);
}

// This test is affected by an iOS 10 simulator bug: https://crbug.com/782033.
#if !BUILDFLAG(IS_IOS)
TEST_F(DocumentProviderTest, GenerateLastModifiedString) {
  static constexpr base::Time::Exploded kLocalExploded = {.year = 2018,
                                                          .month = 8,
                                                          .day_of_month = 27,
                                                          .hour = 3,
                                                          .minute = 18,
                                                          .second = 54};
  base::Time local_now;
  EXPECT_TRUE(base::Time::FromLocalExploded(kLocalExploded, &local_now));

  base::Time modified_today = local_now + base::Hours(-1);
  base::Time modified_this_year = local_now + base::Days(-8);
  base::Time modified_last_year = local_now + base::Days(-365);

  // GenerateLastModifiedString should accept any parsable timestamp, but use
  // ISO8601 UTC timestamp strings since the service returns them in practice.
  EXPECT_EQ(FakeDocumentProvider::GenerateLastModifiedString(
                base::TimeFormatAsIso8601(modified_today), local_now),
            u"2:18\u202FAM");
  EXPECT_EQ(FakeDocumentProvider::GenerateLastModifiedString(
                base::TimeFormatAsIso8601(modified_this_year), local_now),
            u"Aug 19");
  EXPECT_EQ(FakeDocumentProvider::GenerateLastModifiedString(
                base::TimeFormatAsIso8601(modified_last_year), local_now),
            u"8/27/17");
}
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_WIN)

TEST_F(DocumentProviderTest, GetURLForDeduping) {
  // Checks that |url_string| is a URL for opening |expected_id|. An empty ID
  // signifies |url_string| is not a Drive document and |GetURLForDeduping()| is
  // expected to simply return an empty (invalid) GURL.
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
      EXPECT_FALSE(got_output.is_valid()) << url_string;
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

  // URLs that do not represent docs should return an empty (invalid) URL.
  CheckDeduper("https://support.google.com/a/users/answer/1?id=2", "");
  CheckDeduper("https://www.google.com", "");
  CheckDeduper("https://www.google.com/url?url=https://drive.google.com/homepage", "");
  CheckDeduper("https://www.google.com/url?url=https://www.youtube.com/view", "");
  CheckDeduper("https://notdrive.google.com/?x=https%3A%2F%2Fdocs.google.com%2Fa%2Fgoogle.com%2Fdocument%2Fd%2FtH3_d0C-1d%2Fedit", "");
  CheckDeduper("https://sites.google.com/google.com/abc/def", "");

  // clang-format on
}

TEST_F(DocumentProviderTest, CachingForAsyncMatches) {
  auto GetTestProviderMatches = [&](const std::string& input_text,
                                    const std::string& response_str) {
    provider_->input_.UpdateText(base::UTF8ToUTF16(input_text), 0, {});
    provider_->UpdateResults(response_str);
    return provider_->matches_;
  };

  // Partially fill the cache as setup for following tests.
  auto matches =
      GetTestProviderMatches("title", MakeTestResponse({"0", "1", "2"}, 1150));
  EXPECT_THAT(
      ExtractMatchSummary(matches),
      testing::ElementsAre(Summary{u"Document 0 longer title", 1150, false},
                           Summary{u"Document 1 longer title", 1149, false},
                           Summary{u"Document 2 longer title", 1148, false}));

  // Cached matches should be scored 0. Cache size (4) should not restrict
  // number of matches from the current response.
  matches =
      GetTestProviderMatches("title", MakeTestResponse({"1", "2", "3"}, 1150));
  EXPECT_THAT(
      ExtractMatchSummary(matches),
      testing::ElementsAre(Summary{u"Document 1 longer title", 1150, false},
                           Summary{u"Document 2 longer title", 1149, false},
                           Summary{u"Document 3 longer title", 1148, false},
                           Summary{u"Document 0 longer title", 0, true},
                           Summary{u"Document 1 longer title", 0, true},
                           Summary{u"Document 2 longer title", 0, true}));

  // Cache size (4) should restrict number of cached matches appended.
  matches =
      GetTestProviderMatches("title", MakeTestResponse({"3", "4", "5"}, 1150));
  EXPECT_THAT(
      ExtractMatchSummary(matches),
      testing::ElementsAre(Summary{u"Document 3 longer title", 1150, false},
                           Summary{u"Document 4 longer title", 1149, false},
                           Summary{u"Document 5 longer title", 1148, false},
                           Summary{u"Document 1 longer title", 0, true},
                           Summary{u"Document 2 longer title", 0, true},
                           Summary{u"Document 3 longer title", 0, true},
                           Summary{u"Document 0 longer title", 0, true}));

  matches =
      GetTestProviderMatches("title", MakeTestResponse({"0", "4", "6"}, 1150));
  EXPECT_THAT(
      ExtractMatchSummary(matches),
      testing::ElementsAre(Summary{u"Document 0 longer title", 1150, false},
                           Summary{u"Document 4 longer title", 1149, false},
                           Summary{u"Document 6 longer title", 1148, false},
                           Summary{u"Document 3 longer title", 0, true},
                           Summary{u"Document 4 longer title", 0, true},
                           Summary{u"Document 5 longer title", 0, true},
                           Summary{u"Document 1 longer title", 0, true}));

  // Cached results should update match |contents_class|.
  matches = GetTestProviderMatches("docum longer title",
                                   MakeTestResponse({"5", "4", "7"}, 1140));
  EXPECT_THAT(
      ExtractMatchSummary(matches),
      testing::ElementsAre(Summary{u"Document 5 longer title", 1140, false},
                           Summary{u"Document 4 longer title", 1139, false},
                           Summary{u"Document 7 longer title", 1138, false},
                           Summary{u"Document 0 longer title", 0, true},
                           Summary{u"Document 4 longer title", 0, true},
                           Summary{u"Document 6 longer title", 0, true},
                           Summary{u"Document 3 longer title", 0, true}));
  for (const auto& m : matches) {
    EXPECT_THAT(m.contents_class,
                testing::ElementsAre(
                    ACMatchClassification{0, 2}, ACMatchClassification{5, 0},
                    ACMatchClassification{11, 2}, ACMatchClassification{17, 0},
                    ACMatchClassification{18, 2}));
  }

  // Responses larger than the cache max size (4) should be included by scored
  // 0.
  matches = GetTestProviderMatches(
      "title", MakeTestResponse({"8", "9", "10", "11", "12"}, 1150));
  EXPECT_THAT(
      ExtractMatchSummary(matches),
      testing::ElementsAre(Summary{u"Document 8 longer title", 1150, false},
                           Summary{u"Document 9 longer title", 1149, false},
                           Summary{u"Document 10 longer title", 1148, false},
                           Summary{u"Document 11 longer title", 0, false},
                           Summary{u"Document 12 longer title", 0, false},
                           Summary{u"Document 5 longer title", 0, true},
                           Summary{u"Document 4 longer title", 0, true},
                           Summary{u"Document 7 longer title", 0, true},
                           Summary{u"Document 0 longer title", 0, true}));
}

TEST_F(DocumentProviderTest, CachingForSyncMatches) {
  InitClient();

  AutocompleteInput input(u"document", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_omit_asynchronous_matches(true);

  // Sync matches should have scores.
  // Sync matches beyond |provider_max_matches_| should have scores set to 0.
  provider_->input_ = input;
  provider_->UpdateResults(MakeTestResponse({"0", "1", "2", "3", "4"}, 1000));
  provider_->Start(input, false);
  EXPECT_THAT(
      ExtractMatchSummary(provider_->matches_),
      testing::ElementsAre(Summary{u"Document 0 longer title", 1000, true},
                           Summary{u"Document 1 longer title", 999, true},
                           Summary{u"Document 2 longer title", 998, true},
                           Summary{u"Document 3 longer title", 0, true}));

  // Sync matches from the latest response should have scores.
  // Sync matches from previous responses should not have scores.
  // Sync matches beyond |provider_max_matches_| should have scores set to 0.
  provider_->UpdateResults(MakeTestResponse({"4", "5"}, 600));
  provider_->Start(input, false);
  EXPECT_THAT(
      ExtractMatchSummary(provider_->matches_),
      testing::ElementsAre(Summary{u"Document 4 longer title", 600, true},
                           Summary{u"Document 5 longer title", 599, true},
                           Summary{u"Document 0 longer title", 0, true},
                           Summary{u"Document 1 longer title", 0, true}));
}

TEST_F(DocumentProviderTest, MaxMatches) {
  InitClient();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{omnibox::kUrlScoringModel, {}},
                            {omnibox::kMlUrlScoring,
                             {{"MlUrlScoringUnlimitedNumCandidates", "true"}}}},
      /*disabled_feature=*/{});

  OmniboxFieldTrial::ScopedMLConfigForTesting scoped_ml_config;

  AutocompleteInput input(u"document", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_omit_asynchronous_matches(true);

  // Sync matches should have scores.
  // Sync matches beyond |provider_max_matches_| should NOT have scores set to 0
  // (since the "unlimited candidate matches" param is enabled).
  provider_->input_ = input;
  provider_->UpdateResults(MakeTestResponse({"0", "1", "2", "3", "4"}, 1000));
  provider_->Start(input, false);
  EXPECT_THAT(
      ExtractMatchSummary(provider_->matches_),
      testing::ElementsAre(Summary{u"Document 0 longer title", 1000, true},
                           Summary{u"Document 1 longer title", 999, true},
                           Summary{u"Document 2 longer title", 998, true},
                           Summary{u"Document 3 longer title", 997, true}));

  // Sync matches from the latest response should have scores.
  // Sync matches from previous responses should not have scores.
  provider_->UpdateResults(MakeTestResponse({"4", "5"}, 600));
  provider_->Start(input, false);
  EXPECT_THAT(
      ExtractMatchSummary(provider_->matches_),
      testing::ElementsAre(Summary{u"Document 4 longer title", 600, true},
                           Summary{u"Document 5 longer title", 599, true},
                           Summary{u"Document 0 longer title", 0, true},
                           Summary{u"Document 1 longer title", 0, true}));

  // Unlimited matches should ignore the provider max matches, even if the
  // `kMlUrlScoringMaxMatchesByProvider` param is set.
  scoped_ml_config.GetMLConfig().ml_url_scoring_max_matches_by_provider = "*:2";

  provider_->UpdateResults(MakeTestResponse({"6", "7", "8", "9"}, 2000));
  provider_->Start(input, false);
  EXPECT_THAT(
      ExtractMatchSummary(provider_->matches_),
      testing::ElementsAre(Summary{u"Document 6 longer title", 2000, true},
                           Summary{u"Document 7 longer title", 1999, true},
                           Summary{u"Document 8 longer title", 1998, true},
                           Summary{u"Document 9 longer title", 1997, true}));
}

TEST_F(DocumentProviderTest, StartCallsStop) {
  // Test that a call to ::Start will stop old requests to prevent their results
  // from appearing with the new input
  InitClient();

  AutocompleteInput invalid_input(u"12", metrics::OmniboxEventProto::OTHER,
                                  TestSchemeClassifier());
  invalid_input.set_omit_asynchronous_matches(false);

  provider_->done_ = false;
  provider_->Start(invalid_input, false);
  EXPECT_TRUE(provider_->done());
}

TEST_F(DocumentProviderTest, Logging) {
  // The code flow is:
  // 1) `Start()` is invoked when document matches are desired.
  // 2) `Run()` is invoked from `Start()` after a potential debouncing delay.
  // 3) A request is asyncly made to the document backend once an auth token is
  //    ready.
  // 4) A response is asyncly received from the document backend.
  // At any point, the chain of events can be interrupted by a `Stop()`
  // invocation; usually when there's a new input.
  // The below 3 cases test the logged histograms when `Stop()` is invoked after
  // steps 1, 2, and 3.

  {
    SCOPED_TRACE("Case: Stop() before Run().");
    base::HistogramTester histogram_tester;
    provider_->Stop(false, false);
    histogram_tester.ExpectTotalCount("Omnibox.DocumentSuggest.Requests", 0);
    histogram_tester.ExpectTotalCount("Omnibox.DocumentSuggest.TotalTime", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.DocumentSuggest.TotalTime.Interrupted", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.DocumentSuggest.TotalTime.NotInterrupted", 0);
    histogram_tester.ExpectTotalCount("Omnibox.DocumentSuggest.RequestTime", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.DocumentSuggest.RequestTime.Interrupted", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.DocumentSuggest.RequestTime.NotInterrupted", 0);
  }

  {
    SCOPED_TRACE("Case: Stop() before request.");
    base::HistogramTester histogram_tester;
    provider_->time_run_invoked_ = base::TimeTicks::Now();
    provider_->Stop(false, false);
    histogram_tester.ExpectTotalCount("Omnibox.DocumentSuggest.Requests", 0);
    histogram_tester.ExpectTotalCount("Omnibox.DocumentSuggest.TotalTime", 1);
    histogram_tester.ExpectTotalCount(
        "Omnibox.DocumentSuggest.TotalTime.Interrupted", 1);
    histogram_tester.ExpectTotalCount(
        "Omnibox.DocumentSuggest.TotalTime.NotInterrupted", 0);
    histogram_tester.ExpectTotalCount("Omnibox.DocumentSuggest.RequestTime", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.DocumentSuggest.RequestTime.Interrupted", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.DocumentSuggest.RequestTime.NotInterrupted", 0);
  }

  {
    SCOPED_TRACE("Case: Stop() before response.");
    base::HistogramTester histogram_tester;
    provider_->time_run_invoked_ = base::TimeTicks::Now();
    provider_->OnDocumentSuggestionsLoaderAvailable(
        network::SimpleURLLoader::Create(
            std::make_unique<network::ResourceRequest>(),
            net::DefineNetworkTrafficAnnotation("test", "test")));
    provider_->Stop(false, false);
    histogram_tester.ExpectTotalCount("Omnibox.DocumentSuggest.Requests", 2);
    histogram_tester.ExpectBucketCount("Omnibox.DocumentSuggest.Requests", 1,
                                       1);
    histogram_tester.ExpectBucketCount("Omnibox.DocumentSuggest.Requests", 2,
                                       1);
    histogram_tester.ExpectTotalCount("Omnibox.DocumentSuggest.TotalTime", 1);
    histogram_tester.ExpectTotalCount(
        "Omnibox.DocumentSuggest.TotalTime.Interrupted", 1);
    histogram_tester.ExpectTotalCount(
        "Omnibox.DocumentSuggest.TotalTime.NotInterrupted", 0);
    histogram_tester.ExpectTotalCount("Omnibox.DocumentSuggest.RequestTime", 1);
    histogram_tester.ExpectTotalCount(
        "Omnibox.DocumentSuggest.RequestTime.Interrupted", 1);
    histogram_tester.ExpectTotalCount(
        "Omnibox.DocumentSuggest.RequestTime.NotInterrupted", 0);
  }

  // It's difficult to simulate a completed `SimpleURLLoader` response, so we
  // don't test the "Case: Stop() after response" or "Case: No Stop()."
}

TEST_F(DocumentProviderTest, LowQualitySuggestions) {
  auto test = [&](const std::string& response_str,
                  const std::string& input_text,
                  const std::vector<int> expected_scores) {
    std::optional<base::Value> response = base::JSONReader::Read(response_str);
    provider_->input_.UpdateText(base::UTF8ToUTF16(input_text), 0, {});
    ACMatches matches = provider_->ParseDocumentSearchResults(*response);

    ASSERT_EQ(matches.size(), expected_scores.size());
    for (size_t i = 0; i < matches.size(); i++)
      EXPECT_EQ(matches[i].relevance, expected_scores[i]) << "Match " << i;
  };

  {
    SCOPED_TRACE(
        "Unowned and non-title matching docs are limited. Title matching docs "
        "are not limited.");
    test(R"({"results": [
          {"title": "bad title1 title2",  "score": 1000, "url": "good url isn't sufficient"},
          {"title": "bad title1 title2",  "score": 999,  "url": "url"},
          {"title": "bad title1 title2",  "score": 998,  "url": "url"},
          {"title": "goOd tItLE1 title2", "score": 997,  "url": "url"},
          {"title": "good title1 title2", "score": 996,  "url": "url"},
          {"title": "good title1 title2", "score": 995,  "url": "url"},
          {"title": "good title1 title2", "score": 994,  "url": "url"}
        ]})",
         // - 'goo': prefix matches are ok.
         // - 'title1': all input terms must be in the title or owner, but not
         //   all title terms must be in the input (e.g. 'title2').
         // - "goOd tItLE1 title2": Case insensitive.
         "gOo Title1", {1000, 0, 0, 997, 996, 995, 994});
  }

  {
    SCOPED_TRACE("Owned docs are not limited.");
    test(
        R"({"results": [
          {"title": "bad title1 title2",  "score": 1000, "url": "good url isn't sufficient"},
          {"title": "bad title1 title2",  "score": 999,  "url": "url"},
          {"title": "bad title1 title2",  "score": 998,  "url": "url", "metadata": {"owner": {"emailAddresses": [{"emailAddress": "badEmail1@gmail.com"}, {"emailAddress": "gOOdemaIl@gmail.com"}]}}},
          {"title": "bad title1 title2",  "score": 997,  "url": "url", "metadata": {"owner": {"emailAddresses": [{"emailAddress": "badEmail2@gmail.com"}]}}},
          {"title": "good title1 title2", "score": 996,  "url": "url"},
          {"title": "good title1 title2", "score": 995,  "url": "url"},
          {"title": "good title1 title2", "score": 994,  "url": "url"}
        ]})",
        "goo title1", {1000, 0, 998, 0, 996, 995, 994});
  }

  {
    SCOPED_TRACE("Responses with missing owner don't crash and are limited.");
    test(R"({"results": [
            {"title": "title", "score": 1000,  "url": "url", "metadata":
              { "owner": { "emailAddresses": [{}] } }
            },
            {"title": "title", "score": 999,  "url": "url", "metadata":
              { "owner": { "emailAddresses": [{}] } }
            },
            {"title": "title", "score": 998,  "url": "url", "metadata":
              { "owner": { "emailAddresses": [] } }
            },
            {"title": "title", "score": 997,  "url": "url", "metadata":
              { "owner": {} }
            },
            {"title": "title", "score": 996,  "url": "url", "metadata": {}},
            {"title": "title", "score": 995,  "url": "url"},
            {}
          ]})",
         "input", {1000, 0, 0, 0, 0, 0});
  }
}
