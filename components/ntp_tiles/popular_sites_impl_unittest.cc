// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/popular_sites_impl.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/ntp_tiles/features.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/tile_source.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Contains;
using testing::ElementsAre;
using testing::Eq;
using testing::Gt;
using testing::IsEmpty;
using testing::Not;
using testing::Pair;
using testing::SizeIs;

namespace ntp_tiles {
namespace {

const char kTitle[] = "title";
const char kUrl[] = "url";
const char kLargeIconUrl[] = "large_icon_url";
const char kFaviconUrl[] = "favicon_url";
const char kSection[] = "section";
const char kSites[] = "sites";
const char kTitleSource[] = "title_source";
#if BUILDFLAG(IS_IOS)
const char kIOSDefaultPopularSitesLocaleUS[] =
    "https://www.gstatic.com/chrome/ntp/ios/"
    "suggested_sites_US_2023q1_mvt_experiment_with_popular_sites.json";
#endif

using TestPopularSite = std::map<std::string, std::string>;
using TestPopularSiteVector = std::vector<TestPopularSite>;
using TestPopularSection = std::pair<SectionType, TestPopularSiteVector>;
using TestPopularSectionVector = std::vector<TestPopularSection>;

::testing::Matcher<const std::u16string&> Str16Eq(const std::string& s) {
  return ::testing::Eq(base::UTF8ToUTF16(s));
}

::testing::Matcher<const GURL&> URLEq(const std::string& s) {
  return ::testing::Eq(GURL(s));
}

size_t GetNumberOfDefaultPopularSitesForPlatform() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return 8ul;
#else
  return 0ul;
#endif
}

class PopularSitesTest : public ::testing::Test {
 protected:
  PopularSitesTest()
      : kWikipedia{
            {kTitle, "Wikipedia, fhta Ph'nglui mglw'nafh"},
            {kUrl, "https://zz.m.wikipedia.org/"},
            {kLargeIconUrl, "https://zz.m.wikipedia.org/wikipedia.png"},
            {kTitleSource, "3"},  // Title extracted from title tag.
        },
        kYouTube{
            {kTitle, "YouTube"},
            {kUrl, "https://m.youtube.com/"},
            {kLargeIconUrl, "https://s.ytimg.com/apple-touch-icon.png"},
            {kTitleSource, "1"},  // Title extracted from manifest.
        },
        kChromium{
            {kTitle, "The Chromium Project"},
            {kUrl, "https://www.chromium.org/"},
            {kFaviconUrl, "https://www.chromium.org/favicon.ico"},
            // No "title_source" (like in v5 or earlier). Defaults to TITLE_TAG.
        },
        prefs_(new sync_preferences::TestingPrefServiceSyncable()),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    PopularSitesImpl::RegisterProfilePrefs(prefs_->registry());
  }

  void SetCountryAndVersion(const std::string& country,
                            const std::string& version) {
    prefs_->SetString(prefs::kPopularSitesOverrideCountry, country);
    prefs_->SetString(prefs::kPopularSitesOverrideVersion, version);
  }

  base::Value::List CreateListFromTestSites(
      const TestPopularSiteVector& sites) {
    base::Value::List sites_value;
    for (const TestPopularSite& site : sites) {
      base::Value::Dict site_value;
      for (const std::pair<const std::string, std::string>& kv : site) {
        if (kv.first == kTitleSource) {
          int source;
          bool convert_success = base::StringToInt(kv.second, &source);
          DCHECK(convert_success);
          site_value.Set(kv.first, source);
          continue;
        }
        site_value.Set(kv.first, kv.second);
      }
      sites_value.Append(std::move(site_value));
    }
    return sites_value;
  }

  void RespondWithV5JSON(const std::string& url,
                         const TestPopularSiteVector& sites) {
    std::string sites_string;
    base::JSONWriter::Write(CreateListFromTestSites(sites), &sites_string);
    test_url_loader_factory_.AddResponse(url, sites_string);
  }

  void RespondWithV6JSON(const std::string& url,
                         const TestPopularSectionVector& sections) {
    base::Value::List sections_value;
    sections_value.reserve(sections.size());
    for (const TestPopularSection& section : sections) {
      base::Value::Dict section_value;
      section_value.Set(kSection, static_cast<int>(section.first));
      section_value.Set(kSites, CreateListFromTestSites(section.second));
      sections_value.Append(std::move(section_value));
    }
    std::string sites_string;
    base::JSONWriter::Write(sections_value, &sites_string);
    test_url_loader_factory_.AddResponse(url, sites_string);
  }

  void RespondWithData(const std::string& url, const std::string& data) {
    test_url_loader_factory_.AddResponse(url, data);
  }

  void RespondWith404(const std::string& url) {
    test_url_loader_factory_.AddResponse(url, "", net::HTTP_NOT_FOUND);
  }

  void ReregisterProfilePrefs() {
    prefs_ = std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    PopularSitesImpl::RegisterProfilePrefs(prefs_->registry());
  }

  // Returns an optional bool representing whether the completion callback was
  // called at all, and if yes which was the returned bool value.
  std::optional<bool> FetchPopularSites(bool force_download,
                                        PopularSites::SitesVector* sites) {
    std::map<SectionType, PopularSites::SitesVector> sections;
    std::optional<bool> save_success =
        FetchAllSections(force_download, &sections);
    *sites = sections.at(SectionType::PERSONALIZED);
    return save_success;
  }

  // Returns an optional bool representing whether the completion callback was
  // called at all, and if yes which was the returned bool value.
  std::optional<bool> FetchAllSections(
      bool force_download,
      std::map<SectionType, PopularSites::SitesVector>* sections) {
    std::unique_ptr<PopularSites> popular_sites = CreatePopularSites();

    base::RunLoop loop;
    std::optional<bool> save_success;
    if (popular_sites->MaybeStartFetch(
            force_download, base::BindOnce(
                                [](std::optional<bool>* save_success,
                                   base::RunLoop* loop, bool success) {
                                  save_success->emplace(success);
                                  loop->Quit();
                                },
                                &save_success, &loop))) {
      loop.Run();
    }
    *sections = popular_sites->sections();
    return save_success;
  }

  std::unique_ptr<PopularSites> CreatePopularSites() {
    return std::make_unique<PopularSitesImpl>(prefs_.get(),
                                              /*template_url_service=*/nullptr,
                                              /*variations_service=*/nullptr,
                                              test_shared_loader_factory_);
  }

  const TestPopularSite kWikipedia;
  const TestPopularSite kYouTube;
  const TestPopularSite kChromium;

  base::test::TaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

TEST_F(PopularSitesTest, ContainsDefaultTilesRightAfterConstruction) {
  auto popular_sites = CreatePopularSites();
  EXPECT_THAT(
      popular_sites->sections(),
      ElementsAre(Pair(SectionType::PERSONALIZED,
                       SizeIs(GetNumberOfDefaultPopularSitesForPlatform()))));
}

TEST_F(PopularSitesTest, IsEmptyOnConstructionIfDisabledByTrial) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndDisableFeature(kPopularSitesBakedInContentFeature);
  ReregisterProfilePrefs();

  auto popular_sites = CreatePopularSites();

  EXPECT_THAT(popular_sites->sections(),
              ElementsAre(Pair(SectionType::PERSONALIZED, IsEmpty())));
}

TEST_F(PopularSitesTest, ShouldSucceedFetching) {
  SetCountryAndVersion("ZZ", "5");
  RespondWithV5JSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_5.json",
      {kWikipedia});

  PopularSites::SitesVector sites;
  EXPECT_THAT(FetchPopularSites(/*force_download=*/true, &sites),
              Eq(std::optional<bool>(true)));

  ASSERT_THAT(sites.size(), Eq(1u));
  EXPECT_THAT(sites[0].title, Str16Eq("Wikipedia, fhta Ph'nglui mglw'nafh"));
  EXPECT_THAT(sites[0].url, URLEq("https://zz.m.wikipedia.org/"));
  EXPECT_THAT(sites[0].large_icon_url,
              URLEq("https://zz.m.wikipedia.org/wikipedia.png"));
  EXPECT_THAT(sites[0].favicon_url, URLEq(""));
  EXPECT_THAT(sites[0].title_source, Eq(TileTitleSource::TITLE_TAG));
}

#if BUILDFLAG(IS_IOS)
TEST_F(PopularSitesTest, ShouldSucceedFetchingDefaultPopularSitesForLocaleUS) {
  SetCountryAndVersion("US", "5");
  RespondWithV5JSON(kIOSDefaultPopularSitesLocaleUS, {kWikipedia});

  PopularSites::SitesVector sites;
  EXPECT_THAT(FetchPopularSites(/*force_download=*/true, &sites),
              Eq(std::optional<bool>(true)));

  ASSERT_THAT(sites.size(), Eq(1u));
  EXPECT_THAT(sites[0].title, Str16Eq("Wikipedia, fhta Ph'nglui mglw'nafh"));
  EXPECT_THAT(sites[0].url, URLEq("https://zz.m.wikipedia.org/"));
  EXPECT_THAT(sites[0].large_icon_url,
              URLEq("https://zz.m.wikipedia.org/wikipedia.png"));
  EXPECT_THAT(sites[0].favicon_url, URLEq(""));
  EXPECT_THAT(sites[0].title_source, Eq(TileTitleSource::TITLE_TAG));
}
#endif

TEST_F(PopularSitesTest, Fallback) {
  SetCountryAndVersion("ZZ", "5");
  RespondWith404(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_5.json");
  RespondWithV5JSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_DEFAULT_5.json",
      {kYouTube, kChromium});

  PopularSites::SitesVector sites;
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(std::optional<bool>(true)));

  ASSERT_THAT(sites.size(), Eq(2u));
  EXPECT_THAT(sites[0].title, Str16Eq("YouTube"));
  EXPECT_THAT(sites[0].url, URLEq("https://m.youtube.com/"));
  EXPECT_THAT(sites[0].large_icon_url,
              URLEq("https://s.ytimg.com/apple-touch-icon.png"));
  EXPECT_THAT(sites[0].favicon_url, URLEq(""));
  EXPECT_THAT(sites[0].title_source, Eq(TileTitleSource::MANIFEST));
  EXPECT_THAT(sites[1].title, Str16Eq("The Chromium Project"));
  EXPECT_THAT(sites[1].url, URLEq("https://www.chromium.org/"));
  EXPECT_THAT(sites[1].large_icon_url, URLEq(""));
  EXPECT_THAT(sites[1].favicon_url,
              URLEq("https://www.chromium.org/favicon.ico"));
  // Fall back to TITLE_TAG if there is no "title_source". Version 5 or before
  // haven't had this property and get titles from <title> tags exclusively.
  EXPECT_THAT(sites[1].title_source, Eq(TileTitleSource::TITLE_TAG));
}

TEST_F(PopularSitesTest, PopulatesWithDefaultResoucesOnFailure) {
  SetCountryAndVersion("ZZ", "5");
  RespondWith404(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_5.json");
  RespondWith404(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_DEFAULT_5.json");

  PopularSites::SitesVector sites;
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(std::optional<bool>(false)));
  EXPECT_THAT(sites.size(), Eq(GetNumberOfDefaultPopularSitesForPlatform()));
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
TEST_F(PopularSitesTest, AddsIconResourcesToDefaultPages) {
  std::unique_ptr<PopularSites> popular_sites = CreatePopularSites();

  const PopularSites::SitesVector& sites =
      popular_sites->sections().at(SectionType::PERSONALIZED);
  ASSERT_FALSE(sites.empty());
  for (const auto& site : sites) {
    EXPECT_TRUE(site.baked_in);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    EXPECT_THAT(site.default_icon_resource, Gt(0));
#endif
  }
}
#endif

TEST_F(PopularSitesTest, ProvidesDefaultSitesUntilCallbackReturns) {
  SetCountryAndVersion("ZZ", "5");
  RespondWithV5JSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_5.json",
      {kWikipedia});
  std::unique_ptr<PopularSites> popular_sites = CreatePopularSites();

  base::RunLoop loop;
  std::optional<bool> save_success = false;

  bool callback_was_scheduled = popular_sites->MaybeStartFetch(
      /*force_download=*/true, base::BindOnce(
                                   [](std::optional<bool>* save_success,
                                      base::RunLoop* loop, bool success) {
                                     save_success->emplace(success);
                                     loop->Quit();
                                   },
                                   &save_success, &loop));

  // Assert that callback was scheduled so we can wait for its completion.
  ASSERT_TRUE(callback_was_scheduled);
  // There should be 8 default sites as nothing was fetched yet.
  EXPECT_THAT(popular_sites->sections().at(SectionType::PERSONALIZED).size(),
              Eq(GetNumberOfDefaultPopularSitesForPlatform()));

  loop.Run();  // Wait for the fetch to finish and the callback to return.

  EXPECT_TRUE(save_success.value());
  // The 1 fetched site should replace the default sites.
  EXPECT_THAT(popular_sites->sections(),
              ElementsAre(Pair(SectionType::PERSONALIZED, SizeIs(1ul))));
}

TEST_F(PopularSitesTest, UsesCachedJson) {
  SetCountryAndVersion("ZZ", "5");
  RespondWithV5JSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_5.json",
      {kWikipedia});

  // First request succeeds and gets cached.
  PopularSites::SitesVector sites;
  ASSERT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(std::optional<bool>(true)));

  // File disappears from server, but we don't need it because it's cached.
  RespondWith404(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_5.json");
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(std::nullopt));
  EXPECT_THAT(sites[0].url, URLEq("https://zz.m.wikipedia.org/"));
}

TEST_F(PopularSitesTest, CachesEmptyFile) {
  SetCountryAndVersion("ZZ", "5");
  RespondWithData(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_5.json", "[]");
  RespondWithV5JSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_DEFAULT_5.json",
      {kWikipedia});

  // First request succeeds and caches empty suggestions list (no fallback).
  PopularSites::SitesVector sites;
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(std::optional<bool>(true)));
  EXPECT_THAT(sites, IsEmpty());

  // File appears on server, but we continue to use our cached empty file.
  RespondWithV5JSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_5.json",
      {kWikipedia});
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(std::nullopt));
  EXPECT_THAT(sites, IsEmpty());
}

TEST_F(PopularSitesTest, DoesntUseCachedFileIfDownloadForced) {
  SetCountryAndVersion("ZZ", "5");
  RespondWithV5JSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_5.json",
      {kWikipedia});

  // First request succeeds and gets cached.
  PopularSites::SitesVector sites;
  EXPECT_THAT(FetchPopularSites(/*force_download=*/true, &sites),
              Eq(std::optional<bool>(true)));
  EXPECT_THAT(sites[0].url, URLEq("https://zz.m.wikipedia.org/"));

  // File disappears from server. Download is forced, so we get the new file.
  RespondWithV5JSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_5.json",
      {kChromium});
  EXPECT_THAT(FetchPopularSites(/*force_download=*/true, &sites),
              Eq(std::optional<bool>(true)));
  EXPECT_THAT(sites[0].url, URLEq("https://www.chromium.org/"));
}

TEST_F(PopularSitesTest, DoesntUseCacheWithDeprecatedVersion) {
  SetCountryAndVersion("ZZ", "5");
  RespondWithV5JSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_5.json",
      {kWikipedia});

  // First request succeeds and gets cached.
  PopularSites::SitesVector sites;
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(std::optional<bool>(true)));
  EXPECT_THAT(sites[0].url, URLEq("https://zz.m.wikipedia.org/"));
  EXPECT_THAT(prefs_->GetInteger(prefs::kPopularSitesVersionPref), Eq(5));

  // The client is updated to use V6. Drop old data and refetch.
  SetCountryAndVersion("ZZ", "6");
  RespondWithV6JSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_6.json",
      {{SectionType::PERSONALIZED, {kChromium}}});
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(std::optional<bool>(true)));
  EXPECT_THAT(sites[0].url, URLEq("https://www.chromium.org/"));
  EXPECT_THAT(prefs_->GetInteger(prefs::kPopularSitesVersionPref), Eq(6));
}

TEST_F(PopularSitesTest, FallsBackToDefaultParserIfVersionContainsNoNumber) {
  SetCountryAndVersion("ZZ", "staging");
  // The version is used in the URL, as planned when setting it.
  RespondWithV5JSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_staging.json",
      {kChromium});
  PopularSites::SitesVector sites;
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(std::optional<bool>(true)));
  ASSERT_THAT(sites.size(), Eq(1u));
  EXPECT_THAT(sites[0].url, URLEq("https://www.chromium.org/"));
}

TEST_F(PopularSitesTest, RefetchesAfterCountryMoved) {
  RespondWithV5JSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_5.json",
      {kWikipedia});
  RespondWithV5JSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZX_5.json",
      {kChromium});

  PopularSites::SitesVector sites;

  // First request (in ZZ) saves Wikipedia.
  SetCountryAndVersion("ZZ", "5");
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(std::optional<bool>(true)));
  EXPECT_THAT(sites[0].url, URLEq("https://zz.m.wikipedia.org/"));

  // Second request (now in ZX) saves Chromium.
  SetCountryAndVersion("ZX", "5");
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              std::optional<bool>(true));
  EXPECT_THAT(sites[0].url, URLEq("https://www.chromium.org/"));
}

TEST_F(PopularSitesTest, DoesntCacheInvalidFile) {
  SetCountryAndVersion("ZZ", "5");
  RespondWithData(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_5.json",
      "ceci n'est pas un json");
  RespondWith404(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_DEFAULT_5.json");

  // First request falls back and gets nothing there either.
  PopularSites::SitesVector sites;
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(std::optional<bool>(false)));

  // Second request refetches ZZ_9, which now has data.
  RespondWithV5JSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_5.json",
      {kChromium});
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(std::optional<bool>(true)));
  ASSERT_THAT(sites.size(), Eq(1u));
  EXPECT_THAT(sites[0].url, URLEq("https://www.chromium.org/"));
}

TEST_F(PopularSitesTest, RefetchesAfterFallback) {
  SetCountryAndVersion("ZZ", "5");
  RespondWith404(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_5.json");
  RespondWithV5JSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_DEFAULT_5.json",
      {kWikipedia});

  // First request falls back.
  PopularSites::SitesVector sites;
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(std::optional<bool>(true)));
  ASSERT_THAT(sites.size(), Eq(1u));
  EXPECT_THAT(sites[0].url, URLEq("https://zz.m.wikipedia.org/"));

  // Second request refetches ZZ_9, which now has data.
  RespondWithV5JSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_5.json",
      {kChromium});
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(std::optional<bool>(true)));
  ASSERT_THAT(sites.size(), Eq(1u));
  EXPECT_THAT(sites[0].url, URLEq("https://www.chromium.org/"));
}

TEST_F(PopularSitesTest, ShouldOverrideDirectory) {
  SetCountryAndVersion("ZZ", "5");
  prefs_->SetString(prefs::kPopularSitesOverrideDirectory, "foo/bar/");
  RespondWithV5JSON("https://www.gstatic.com/foo/bar/suggested_sites_ZZ_5.json",
                    {kWikipedia});

  PopularSites::SitesVector sites;
  EXPECT_THAT(FetchPopularSites(/*force_download=*/false, &sites),
              Eq(std::optional<bool>(true)));

  EXPECT_THAT(sites.size(), Eq(1u));
}

TEST_F(PopularSitesTest, DoesNotFetchExplorationSites) {
  SetCountryAndVersion("ZZ", "6");
  RespondWithV6JSON(
      "https://www.gstatic.com/chrome/ntp/suggested_sites_ZZ_6.json",
      {{SectionType::PERSONALIZED, {kChromium}},
       {SectionType::NEWS, {kYouTube}}});

  std::map<SectionType, PopularSites::SitesVector> sections;
  EXPECT_THAT(FetchAllSections(/*force_download=*/false, &sections),
              Eq(std::optional<bool>(true)));

  // The fetched news section should not be propagated without enabled feature.
  EXPECT_THAT(sections, Not(Contains(Pair(SectionType::NEWS, _))));
}

}  // namespace
}  // namespace ntp_tiles
