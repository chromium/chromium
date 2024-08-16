// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/most_visited_sites.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/history/core/browser/top_sites.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/ntp_tiles/custom_links_manager.h"
#include "components/ntp_tiles/features.h"
#include "components/ntp_tiles/icon_cacher.h"
#include "components/ntp_tiles/popular_sites_impl.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/section_type.h"
#include "components/ntp_tiles/switches.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/webapps/common/constants.h"
#include "extensions/buildflags/buildflags.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#endif

namespace ntp_tiles {

// Defined for googletest. Must be defined in the same namespace.
void PrintTo(const NTPTile& tile, std::ostream* os) {
  *os << "{\"" << tile.title << "\", \"" << tile.url << "\", "
      << static_cast<int>(tile.source) << "}";
}

namespace {

using history::MostVisitedURL;
using history::MostVisitedURLList;
using history::TopSites;
using testing::_;
using testing::AllOf;
using testing::AnyNumber;
using testing::AtLeast;
using testing::ByMove;
using testing::Contains;
using testing::DoAll;
using testing::ElementsAre;
using testing::Eq;
using testing::Ge;
using testing::InSequence;
using testing::Invoke;
using testing::IsEmpty;
using testing::Key;
using testing::Mock;
using testing::Not;
using testing::Pair;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;
using testing::SizeIs;
using testing::StrictMock;

const char kHomepageUrl[] = "http://homepa.ge/";
const char16_t kHomepageTitle[] = u"Homepage";

std::string PrintTile(const std::u16string& title,
                      const std::string& url,
                      TileSource source) {
  return std::string("has title \"") + base::UTF16ToUTF8(title) +
         std::string("\" and url \"") + url + std::string("\" and source ") +
         testing::PrintToString(static_cast<int>(source));
}

MATCHER_P3(MatchesTile, title, url, source, PrintTile(title, url, source)) {
  return arg.title == title && arg.url == GURL(url) && arg.source == source;
}

MATCHER_P3(LastTileIs,
           title,
           url,
           source,
           std::string("last tile ") + PrintTile(title, url, source)) {
  const NTPTilesVector& tiles = arg.at(SectionType::PERSONALIZED);
  if (tiles.empty()) {
    return false;
  }

  const NTPTile& last = tiles.back();
  return last.title == title && last.url == GURL(url) && last.source == source;
}

MATCHER_P3(FirstPersonalizedTileIs,
           title,
           url,
           source,
           std::string("first tile ") + PrintTile(title, url, source)) {
  if (arg.count(SectionType::PERSONALIZED) == 0) {
    return false;
  }
  const NTPTilesVector& tiles = arg.at(SectionType::PERSONALIZED);
  return !tiles.empty() && tiles[0].title == title &&
         tiles[0].url == GURL(url) && tiles[0].source == source;
}

NTPTile MakeTile(const std::u16string& title,
                 const std::string& url,
                 TileSource source) {
  NTPTile tile;
  tile.title = title;
  tile.url = GURL(url);
  tile.source = source;
  return tile;
}

MostVisitedURL MakeMostVisitedURL(const std::u16string& title,
                                  const std::string& url) {
  MostVisitedURL result;
  result.title = title;
  result.url = GURL(url);
  return result;
}

class MockTopSites : public TopSites {
 public:
  MOCK_METHOD0(ShutdownOnUIThread, void());
  MOCK_METHOD1(GetMostVisitedURLs, void(GetMostVisitedURLsCallback callback));
  MOCK_METHOD0(SyncWithHistory, void());
  MOCK_CONST_METHOD0(HasBlockedUrls, bool());
  MOCK_METHOD1(AddBlockedUrl, void(const GURL& url));
  MOCK_METHOD1(RemoveBlockedUrl, void(const GURL& url));
  MOCK_METHOD1(IsBlocked, bool(const GURL& url));
  MOCK_METHOD0(ClearBlockedUrls, void());
  MOCK_METHOD0(StartQueryForMostVisited, base::CancelableTaskTracker::TaskId());
  MOCK_METHOD1(IsKnownURL, bool(const GURL& url));
  MOCK_CONST_METHOD1(GetCanonicalURLString,
                     const std::string&(const GURL& url));
  MOCK_METHOD0(IsFull, bool());
  MOCK_CONST_METHOD0(loaded, bool());
  MOCK_METHOD0(GetPrepopulatedPages, history::PrepopulatedPageList());
  MOCK_METHOD1(OnNavigationCommitted, void(const GURL& url));

  // Publicly expose notification to observers, since the implementation cannot
  // be overriden.
  using TopSites::NotifyTopSitesChanged;

 protected:
  ~MockTopSites() override = default;
};

class MockMostVisitedSitesObserver : public MostVisitedSites::Observer {
 public:
  MOCK_METHOD1(OnURLsAvailable,
               void(const std::map<SectionType, NTPTilesVector>& sections));
  MOCK_METHOD1(OnIconMadeAvailable, void(const GURL& site_url));
};

class FakeHomepageClient : public MostVisitedSites::HomepageClient {
 public:
  FakeHomepageClient()
      : homepage_tile_enabled_(false), homepage_url_(kHomepageUrl) {}
  ~FakeHomepageClient() override = default;

  bool IsHomepageTileEnabled() const override { return homepage_tile_enabled_; }

  GURL GetHomepageUrl() const override { return homepage_url_; }

  void QueryHomepageTitle(TitleCallback title_callback) override {
    std::move(title_callback).Run(homepage_title_);
  }

  void SetHomepageTileEnabled(bool homepage_tile_enabled) {
    homepage_tile_enabled_ = homepage_tile_enabled;
  }

  void SetHomepageUrl(GURL homepage_url) { homepage_url_ = homepage_url; }

  void SetHomepageTitle(const std::optional<std::u16string>& homepage_title) {
    homepage_title_ = homepage_title;
  }

 private:
  bool homepage_tile_enabled_;
  GURL homepage_url_;
  std::optional<std::u16string> homepage_title_;
};

class MockIconCacher : public IconCacher {
 public:
  MOCK_METHOD3(StartFetchPopularSites,
               void(PopularSites::Site site,
                    base::OnceClosure icon_available,
                    base::OnceClosure preliminary_icon_available));
  MOCK_METHOD2(StartFetchMostLikely,
               void(const GURL& page_url, base::OnceClosure icon_available));
};

class MockCustomLinksManager : public CustomLinksManager {
 public:
  MOCK_METHOD1(Initialize, bool(const NTPTilesVector& tiles));
  MOCK_METHOD0(Uninitialize, void());
  MOCK_CONST_METHOD0(IsInitialized, bool());
  MOCK_CONST_METHOD0(GetLinks, const std::vector<CustomLinksManager::Link>&());
  MOCK_METHOD2(AddLink, bool(const GURL& url, const std::u16string& title));
  MOCK_METHOD3(UpdateLink,
               bool(const GURL& url,
                    const GURL& new_url,
                    const std::u16string& new_title));
  MOCK_METHOD2(ReorderLink, bool(const GURL& url, size_t new_pos));
  MOCK_METHOD1(DeleteLink, bool(const GURL& url));
  MOCK_METHOD0(UndoAction, bool());
  MOCK_METHOD1(RegisterCallbackForOnChanged,
               base::CallbackListSubscription(base::RepeatingClosure callback));
};

class PopularSitesFactoryForTest {
 public:
  explicit PopularSitesFactoryForTest(
      sync_preferences::TestingPrefServiceSyncable* pref_service)
      : prefs_(pref_service) {
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    PopularSitesImpl::RegisterProfilePrefs(pref_service->registry());
  }

  void SeedWithSampleData() {
    prefs_->SetString(prefs::kPopularSitesOverrideCountry, "IN");
    prefs_->SetString(prefs::kPopularSitesOverrideVersion, "5");

    test_url_loader_factory_.ClearResponses();
    test_url_loader_factory_.AddResponse(
        "https://www.gstatic.com/chrome/ntp/suggested_sites_IN_5.json",
        R"([{
              "title": "PopularSite1",
              "url": "http://popularsite1/",
              "favicon_url": "http://popularsite1/favicon.ico"
            },
            {
              "title": "PopularSite2",
              "url": "http://popularsite2/",
              "favicon_url": "http://popularsite2/favicon.ico"
            }
           ])");

    test_url_loader_factory_.AddResponse(
        "https://www.gstatic.com/chrome/ntp/suggested_sites_US_5.json",
        R"([{
              "title": "ESPN",
              "url": "http://www.espn.com",
              "favicon_url": "http://www.espn.com/favicon.ico"
            }, {
              "title": "Mobile",
              "url": "http://www.mobile.de",
              "favicon_url": "http://www.mobile.de/favicon.ico"
            }, {
              "title": "Google News",
              "url": "http://news.google.com",
              "favicon_url": "http://news.google.com/favicon.ico"
            }
           ])");
#if BUILDFLAG(IS_IOS)
    test_url_loader_factory_.AddResponse(
        "https://www.gstatic.com/chrome/ntp/ios/"
        "suggested_sites_US_2023q1_mvt_experiment_with_popular_sites.json",
        R"([{
              "title": "ESPN",
              "url": "http://www.espn.com",
              "favicon_url": "http://www.espn.com/favicon.ico"
            }, {
              "title": "Mobile",
              "url": "http://www.mobile.de",
              "favicon_url": "http://www.mobile.de/favicon.ico"
            }, {
              "title": "Google News",
              "url": "http://news.google.com",
              "favicon_url": "http://news.google.com/favicon.ico"
            }
           ])");
#endif

    test_url_loader_factory_.AddResponse(
        "https://www.gstatic.com/chrome/ntp/suggested_sites_IN_6.json",
        R"([{
              "section": 1, // PERSONALIZED
              "sites": [{
                  "title": "PopularSite1",
                  "url": "http://popularsite1/",
                  "favicon_url": "http://popularsite1/favicon.ico"
                },
                {
                  "title": "PopularSite2",
                  "url": "http://popularsite2/",
                  "favicon_url": "http://popularsite2/favicon.ico"
                },
               ]
            },
            {
                "section": 4,  // NEWS
                "sites": [{
                    "large_icon_url": "https://news.google.com/icon.ico",
                    "title": "Google News",
                    "url": "https://news.google.com/"
                },
                {
                    "favicon_url": "https://news.google.com/icon.ico",
                    "title": "Google News Germany",
                    "url": "https://news.google.de/"
                }]
            },
            {
                "section": 2,  // SOCIAL
                "sites": [{
                    "large_icon_url": "https://ssl.gstatic.com/icon.png",
                    "title": "Google+",
                    "url": "https://plus.google.com/"
                }]
            },
            {
                "section": 3,  // ENTERTAINMENT
                "sites": [
                    // Intentionally empty site list.
                ]
            }
        ])");
  }

  std::unique_ptr<PopularSites> New() {
    return std::make_unique<PopularSitesImpl>(prefs_,
                                              /*template_url_service=*/nullptr,
                                              /*variations_service=*/nullptr,
                                              test_shared_loader_factory_);
  }

 private:
  raw_ptr<PrefService> prefs_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

}  // namespace

// Param specifies whether Popular Sites is enabled via variations.
class MostVisitedSitesTest : public ::testing::TestWithParam<bool> {
 protected:
  using TopSitesCallbackList =
      base::OnceCallbackList<TopSites::GetMostVisitedURLsCallback::RunType>;

  MostVisitedSitesTest() {
    MostVisitedSites::RegisterProfilePrefs(pref_service_.registry());
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    supervised_user::RegisterProfilePrefs(pref_service_.registry());
#endif

    std::vector<base::test::FeatureRef> enabled_features;
    // Disable FaviconServer in most tests and override in specific tests.
    std::vector<base::test::FeatureRef> disabled_features = {
        kNtpMostLikelyFaviconsFromServerFeature};
    if (IsPopularSitesFeatureEnabled()) {
      enabled_features.push_back(kUsePopularSitesSuggestions);
    } else {
      disabled_features.push_back(kUsePopularSitesSuggestions);
    }

    // Updating list value in pref with default gmail URL for unit testing.
    // Also adding migration feature to be enabled for unit test.
    auto defaults =
        base::Value::List().Append("pjkljhegncpnkpknbcohdijeoejaedia");
    pref_service_.registry()->RegisterListPref(
        webapps::kWebAppsMigratedPreinstalledApps, std::move(defaults));

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
    if (IsPopularSitesFeatureEnabled()) {
      popular_sites_factory_.SeedWithSampleData();
    }

    RecreateMostVisitedSites();
  }

  void RecreateMostVisitedSites() {
    // We use StrictMock to make sure the object is not used unless Popular
    // Sites is enabled.
    auto icon_cacher = std::make_unique<StrictMock<MockIconCacher>>();
    icon_cacher_ = icon_cacher.get();

    // Custom links needs to be nullptr when MostVisitedSites is created, unless
    // the custom links feature is enabled. Custom links is disabled for
    // Android, iOS, and third-party NTPs.
    std::unique_ptr<StrictMock<MockCustomLinksManager>> mock_custom_links;
    if (is_custom_links_enabled_) {
      mock_custom_links =
          std::make_unique<StrictMock<MockCustomLinksManager>>();
      mock_custom_links_ = mock_custom_links.get();
    }

    if (IsPopularSitesFeatureEnabled()) {
      // Populate Popular Sites' internal cache by mimicking a past usage of
      // PopularSitesImpl.
      auto tmp_popular_sites = popular_sites_factory_.New();
      base::RunLoop loop;
      bool save_success = false;
      tmp_popular_sites->MaybeStartFetch(
          /*force_download=*/true,
          base::BindOnce(
              [](bool* save_success, base::RunLoop* loop, bool success) {
                *save_success = success;
                loop->Quit();
              },
              &save_success, &loop));
      loop.Run();
      EXPECT_TRUE(save_success);

      // With PopularSites enabled, blocked urls is exercised.
      EXPECT_CALL(*mock_top_sites_, IsBlocked(_)).WillRepeatedly(Return(false));
      // Mock icon cacher never replies, and we also don't verify whether the
      // code uses it correctly.
      EXPECT_CALL(*icon_cacher, StartFetchPopularSites(_, _, _))
          .Times(AtLeast(0));
    }

    EXPECT_CALL(*icon_cacher, StartFetchMostLikely(_, _)).Times(AtLeast(0));

    most_visited_sites_ = std::make_unique<MostVisitedSites>(
        &pref_service_, /*identity_manager=*/nullptr,
        /*supervised_user_service=*/nullptr, mock_top_sites_,
        popular_sites_factory_.New(), std::move(mock_custom_links),
        std::move(icon_cacher), true);
  }

  bool IsPopularSitesFeatureEnabled() const { return GetParam(); }

  bool VerifyAndClearExpectations() {
    base::RunLoop().RunUntilIdle();
    const bool success =
        Mock::VerifyAndClearExpectations(mock_top_sites_.get()) &&
        Mock::VerifyAndClearExpectations(&mock_observer_);
    // For convenience, restore the expectations for IsBlocked().
    if (IsPopularSitesFeatureEnabled()) {
      EXPECT_CALL(*mock_top_sites_, IsBlocked(_)).WillRepeatedly(Return(false));
    }
    return success;
  }

  FakeHomepageClient* RegisterNewHomepageClient() {
    auto homepage_client = std::make_unique<FakeHomepageClient>();
    FakeHomepageClient* raw_client_ptr = homepage_client.get();
    most_visited_sites_->SetHomepageClient(std::move(homepage_client));
    return raw_client_ptr;
  }

  void EnableCustomLinks() { is_custom_links_enabled_ = true; }

  bool is_custom_links_enabled_ = false;
  TopSitesCallbackList top_sites_callbacks_;

  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  PopularSitesFactoryForTest popular_sites_factory_{&pref_service_};
  scoped_refptr<StrictMock<MockTopSites>> mock_top_sites_ =
      base::MakeRefCounted<StrictMock<MockTopSites>>();
  StrictMock<MockMostVisitedSitesObserver> mock_observer_;
  StrictMock<MockMostVisitedSitesObserver> mock_other_observer_;
  std::unique_ptr<MostVisitedSites> most_visited_sites_;
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<MockCustomLinksManager> mock_custom_links_;
  raw_ptr<MockIconCacher> icon_cacher_;
};

TEST_P(MostVisitedSitesTest, ShouldStartNoCallInConstructor) {
  // No call to mocks expected by the mere fact of instantiating
  // MostVisitedSites.
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldRefreshBackends) {
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  most_visited_sites_->Refresh();
}

TEST_P(MostVisitedSitesTest, ShouldIncludeTileForHomepage) {
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>(MostVisitedURLList{}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlocked(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(FirstPersonalizedTileIs(
                                  u"", kHomepageUrl, TileSource::HOMEPAGE)));
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/3);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldNotIncludeHomepageWithoutClient) {
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>(MostVisitedURLList{}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(mock_observer_,
              OnURLsAvailable(Contains(
                  Pair(SectionType::PERSONALIZED,
                       Not(Contains(MatchesTile(u"", kHomepageUrl,
                                                TileSource::HOMEPAGE)))))));
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/3);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldIncludeHomeTileWithUrlBeforeQueryingName) {
  // Because the query time for the real name might take a while, provide the
  // home tile with URL as title immediately and update the tiles as soon as the
  // real title was found.
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);
  homepage_client->SetHomepageTitle(kHomepageTitle);
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>(MostVisitedURLList{}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlocked(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  {
    testing::Sequence seq;
    EXPECT_CALL(mock_observer_,
                OnURLsAvailable(Contains(
                    Pair(SectionType::PERSONALIZED,
                         Not(Contains(MatchesTile(u"", kHomepageUrl,
                                                  TileSource::HOMEPAGE)))))));
    EXPECT_CALL(mock_observer_,
                OnURLsAvailable(Contains(
                    Pair(SectionType::PERSONALIZED,
                         Not(Contains(MatchesTile(kHomepageTitle, kHomepageUrl,
                                                  TileSource::HOMEPAGE)))))));
  }
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/3);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldUpdateHomepageTileWhenRefreshHomepageTile) {
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);

  // Ensure that home tile is available as usual.
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>(MostVisitedURLList{}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlocked(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(FirstPersonalizedTileIs(
                                  u"", kHomepageUrl, TileSource::HOMEPAGE)));
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/3);
  base::RunLoop().RunUntilIdle();
  VerifyAndClearExpectations();

  // Disable home page and rebuild _without_ Resync. The tile should be gone.
  homepage_client->SetHomepageTileEnabled(false);
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>(MostVisitedURLList{}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory()).Times(0);
  EXPECT_CALL(mock_observer_, OnURLsAvailable(Not(FirstPersonalizedTileIs(
                                  u"", kHomepageUrl, TileSource::HOMEPAGE))));
  most_visited_sites_->RefreshTiles();
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldNotIncludeHomepageIfNoTileRequested) {
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>(MostVisitedURLList{}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlocked(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(
      mock_observer_,
      OnURLsAvailable(Contains(Pair(SectionType::PERSONALIZED, IsEmpty()))));
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/0);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldReturnHomepageIfOneTileRequested) {
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>((
          MostVisitedURLList{MakeMostVisitedURL(u"Site 1", "http://site1/")})));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlocked(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(
      mock_observer_,
      OnURLsAvailable(Contains(Pair(
          SectionType::PERSONALIZED,
          ElementsAre(MatchesTile(u"", kHomepageUrl, TileSource::HOMEPAGE))))));
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/1);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldHaveHomepageFirstInListWhenFull) {
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>((MostVisitedURLList{
              MakeMostVisitedURL(u"Site 1", "http://site1/"),
              MakeMostVisitedURL(u"Site 2", "http://site2/"),
              MakeMostVisitedURL(u"Site 3", "http://site3/"),
              MakeMostVisitedURL(u"Site 4", "http://site4/"),
              MakeMostVisitedURL(u"Site 5", "http://site5/"),
          })));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlocked(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  std::map<SectionType, NTPTilesVector> sections;
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections));
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/4);
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(sections, Contains(Key(SectionType::PERSONALIZED)));
  NTPTilesVector tiles = sections.at(SectionType::PERSONALIZED);
  ASSERT_THAT(tiles.size(), Ge(4ul));
  // Assert that the home page is appended as the final tile.
  EXPECT_THAT(tiles[0], MatchesTile(u"", kHomepageUrl, TileSource::HOMEPAGE));
}

// The following test exercises behavior with a preinstalled chrome app; this
// is only relevant if extensions and apps are enabled.
#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_P(MostVisitedSitesTest, ShouldNotContainDefaultPreinstalledApp) {
  const char kTestUrl[] = "http://site1/";
  const char16_t kTestTitle[] = u"Site 1";
  const char kGmailUrl[] =
      "chrome-extension://pjkljhegncpnkpknbcohdijeoejaedia/index.html";
  const char16_t kGmailTitle[] = u"Gmail";

  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(
          MostVisitedURLList{MakeMostVisitedURL(kGmailTitle, kGmailUrl),
                             MakeMostVisitedURL(kTestTitle, kTestUrl)}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  std::map<SectionType, NTPTilesVector> sections;
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillRepeatedly(SaveArg<0>(&sections));

  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/2);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(sections.at(SectionType::PERSONALIZED),
              AllOf(Not(Contains(MatchesTile(kGmailTitle, kGmailUrl,
                                             TileSource::TOP_SITES))),
                    Contains(MatchesTile(kTestTitle, kTestUrl,
                                         TileSource::TOP_SITES))));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

TEST_P(MostVisitedSitesTest, ShouldHaveHomepageFirstInListWhenNotFull) {
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>((MostVisitedURLList{
              MakeMostVisitedURL(u"Site 1", "http://site1/"),
              MakeMostVisitedURL(u"Site 2", "http://site2/"),
              MakeMostVisitedURL(u"Site 3", "http://site3/"),
              MakeMostVisitedURL(u"Site 4", "http://site4/"),
              MakeMostVisitedURL(u"Site 5", "http://site5/"),
          })));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlocked(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  std::map<SectionType, NTPTilesVector> sections;
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections));
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/8);
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(sections, Contains(Key(SectionType::PERSONALIZED)));
  NTPTilesVector tiles = sections.at(SectionType::PERSONALIZED);
  ASSERT_THAT(tiles.size(), Ge(6ul));
  // Assert that the home page is the first tile.
  EXPECT_THAT(tiles[0], MatchesTile(u"", kHomepageUrl, TileSource::HOMEPAGE));
}

TEST_P(MostVisitedSitesTest, ShouldDeduplicateHomepageWithTopSites) {
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(
          (MostVisitedURLList{MakeMostVisitedURL(u"Site 1", "http://site1/"),
                              MakeMostVisitedURL(u"", kHomepageUrl)})));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlocked(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(
      mock_observer_,
      OnURLsAvailable(Contains(Pair(
          SectionType::PERSONALIZED,
          AllOf(Contains(MatchesTile(u"", kHomepageUrl, TileSource::HOMEPAGE)),
                Not(Contains(MatchesTile(u"", kHomepageUrl,
                                         TileSource::TOP_SITES))))))));
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/3);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldNotIncludeHomepageIfThereIsNone) {
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(false);
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>(MostVisitedURLList{}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlocked(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_observer_,
              OnURLsAvailable(Contains(
                  Pair(SectionType::PERSONALIZED,
                       Not(Contains(MatchesTile(u"", kHomepageUrl,
                                                TileSource::HOMEPAGE)))))));
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/3);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldNotIncludeHomepageIfEmptyUrl) {
  const std::string kEmptyHomepageUrl;
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);
  homepage_client->SetHomepageUrl(GURL(kEmptyHomepageUrl));
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>(MostVisitedURLList{}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlocked(Eq(kEmptyHomepageUrl)))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_observer_,
              OnURLsAvailable(Not(FirstPersonalizedTileIs(
                  u"", kEmptyHomepageUrl, TileSource::HOMEPAGE))));
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/3);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesTest, ShouldNotIncludeHomepageIfBlocked) {
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(
          (MostVisitedURLList{MakeMostVisitedURL(u"", kHomepageUrl)})));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlocked(Eq(GURL(kHomepageUrl))))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));

  EXPECT_CALL(*mock_top_sites_, IsBlocked(Eq(GURL(kHomepageUrl))))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_observer_,
              OnURLsAvailable(Contains(
                  Pair(SectionType::PERSONALIZED,
                       Not(Contains(MatchesTile(u"", kHomepageUrl,
                                                TileSource::HOMEPAGE)))))));

  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/3);
  base::RunLoop().RunUntilIdle();
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
TEST_P(MostVisitedSitesTest, ShouldPinHomepageAgainIfBlockedUndone) {
  FakeHomepageClient* homepage_client = RegisterNewHomepageClient();
  homepage_client->SetHomepageTileEnabled(true);
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillOnce(base::test::RunOnceCallback<0>(
          (MostVisitedURLList{MakeMostVisitedURL(u"", kHomepageUrl)})));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  EXPECT_CALL(*mock_top_sites_, IsBlocked(Eq(GURL(kHomepageUrl))))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_observer_,
              OnURLsAvailable(Contains(
                  Pair(SectionType::PERSONALIZED,
                       Not(Contains(MatchesTile(u"", kHomepageUrl,
                                                TileSource::HOMEPAGE)))))));

  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/3);
  base::RunLoop().RunUntilIdle();
  VerifyAndClearExpectations();
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillOnce(base::test::RunOnceCallback<0>(MostVisitedURLList{}));
  EXPECT_CALL(*mock_top_sites_, IsBlocked(Eq(GURL(kHomepageUrl))))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(
      mock_observer_,
      OnURLsAvailable(Contains(Pair(
          SectionType::PERSONALIZED,
          Contains(MatchesTile(u"", kHomepageUrl, TileSource::HOMEPAGE))))));

  most_visited_sites_->OnURLFilterChanged();

  base::RunLoop().RunUntilIdle();
}
#endif

TEST_P(MostVisitedSitesTest, ShouldInformSuggestionSourcesWhenBlocked) {
  EXPECT_CALL(*mock_top_sites_, AddBlockedUrl(Eq(GURL(kHomepageUrl)))).Times(1);
  most_visited_sites_->AddOrRemoveBlockedUrl(GURL(kHomepageUrl),
                                             /*add_url=*/true);
  EXPECT_CALL(*mock_top_sites_, RemoveBlockedUrl(Eq(GURL(kHomepageUrl))))
      .Times(1);
  most_visited_sites_->AddOrRemoveBlockedUrl(GURL(kHomepageUrl),
                                             /*add_url=*/false);
}

TEST_P(MostVisitedSitesTest,
       ShouldDeduplicatePopularSitesWithMostVisitedIffHostAndTitleMatches) {
  pref_service_.SetString(prefs::kPopularSitesOverrideCountry, "US");
  RecreateMostVisitedSites();  // Refills cache with ESPN and Google News.
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>(MostVisitedURLList{
              MakeMostVisitedURL(u"ESPN", "http://espn.com/"),
              MakeMostVisitedURL(u"Mobile", "http://m.mobile.de/"),
              MakeMostVisitedURL(u"Google", "http://www.google.com/")}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  std::map<SectionType, NTPTilesVector> sections;
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections));

  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/6);
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(sections, Contains(Key(SectionType::PERSONALIZED)));
  EXPECT_THAT(sections.at(SectionType::PERSONALIZED),
              Contains(MatchesTile(u"Google", "http://www.google.com/",
                                   TileSource::TOP_SITES)));
  if (IsPopularSitesFeatureEnabled()) {
    EXPECT_THAT(sections.at(SectionType::PERSONALIZED),
                Contains(MatchesTile(u"Google News", "http://news.google.com/",
                                     TileSource::POPULAR)));
  }
  EXPECT_THAT(sections.at(SectionType::PERSONALIZED),
              AllOf(Contains(MatchesTile(u"ESPN", "http://espn.com/",
                                         TileSource::TOP_SITES)),
                    Contains(MatchesTile(u"Mobile", "http://m.mobile.de/",
                                         TileSource::TOP_SITES)),
                    Not(Contains(MatchesTile(u"ESPN", "http://www.espn.com/",
                                             TileSource::POPULAR))),
                    Not(Contains(MatchesTile(u"Mobile", "http://www.mobile.de/",
                                             TileSource::POPULAR)))));
}

TEST_P(MostVisitedSitesTest, ShouldHandleTopSitesCacheHit) {
  // If cached, TopSites returns the tiles synchronously, running the callback
  // even before the function returns.
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(
          MostVisitedURLList{MakeMostVisitedURL(u"Site 1", "http://site1/")}));

  InSequence seq;
  if (IsPopularSitesFeatureEnabled()) {
    EXPECT_CALL(
        mock_observer_,
        OnURLsAvailable(Contains(Pair(
            SectionType::PERSONALIZED,
            ElementsAre(
                MatchesTile(u"Site 1", "http://site1/", TileSource::TOP_SITES),
                MatchesTile(u"PopularSite1", "http://popularsite1/",
                            TileSource::POPULAR),
                MatchesTile(u"PopularSite2", "http://popularsite2/",
                            TileSource::POPULAR))))));
  } else {
    EXPECT_CALL(mock_observer_,
                OnURLsAvailable(Contains(
                    Pair(SectionType::PERSONALIZED,
                         ElementsAre(MatchesTile(u"Site 1", "http://site1/",
                                                 TileSource::TOP_SITES))))));
  }
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());

  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/3);
  VerifyAndClearExpectations();
  CHECK(top_sites_callbacks_.empty());

  // Update by TopSites is propagated.
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillOnce(base::test::RunOnceCallback<0>(
          MostVisitedURLList{MakeMostVisitedURL(u"Site 2", "http://site2/")}));
  if (IsPopularSitesFeatureEnabled()) {
    EXPECT_CALL(*mock_top_sites_, IsBlocked(_)).WillRepeatedly(Return(false));
  }
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_));
  mock_top_sites_->NotifyTopSitesChanged(
      history::TopSitesObserver::ChangeReason::MOST_VISITED);
  base::RunLoop().RunUntilIdle();
}

// Tests that multiple observers can be added to the MostVisitedSites.
TEST_P(MostVisitedSitesTest, MultipleObservers) {
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(
          base::test::RunOnceCallbackRepeatedly<0>(MostVisitedURLList{
              MakeMostVisitedURL(u"ESPN", "http://espn.com/"),
              MakeMostVisitedURL(u"Mobile", "http://m.mobile.de/"),
              MakeMostVisitedURL(u"Google", "http://www.google.com/")}));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  std::map<SectionType, NTPTilesVector> sections;
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections));

  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/2);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      sections.at(SectionType::PERSONALIZED),
      AllOf(Contains(MatchesTile(u"ESPN", "http://espn.com/",
                                 TileSource::TOP_SITES)),
            Contains(MatchesTile(u"Mobile", "http://m.mobile.de/",
                                 TileSource::TOP_SITES)),
            Not(Contains(MatchesTile(u"Google", "http://www.google.com/",
                                     TileSource::TOP_SITES)))));

  // Verifies that multiple observers can be added.
  sections.clear();
  std::map<SectionType, NTPTilesVector> sections_other;
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory()).Times(1);
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillRepeatedly(SaveArg<0>(&sections));
  EXPECT_CALL(mock_other_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections_other));
  most_visited_sites_->RefreshTiles();
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_other_observer_,
                                                  /*max_num_sites=*/2);
  base::RunLoop().RunUntilIdle();

  // Verifies that two observers will be notified with the same suggestions.
  EXPECT_TRUE(sections == sections_other);
  ASSERT_THAT(sections, Contains(Key(SectionType::PERSONALIZED)));
  EXPECT_THAT(
      sections.at(SectionType::PERSONALIZED),
      AllOf(Contains(MatchesTile(u"ESPN", "http://espn.com/",
                                 TileSource::TOP_SITES)),
            Contains(MatchesTile(u"Mobile", "http://m.mobile.de/",
                                 TileSource::TOP_SITES)),
            Not(Contains(MatchesTile(u"Google", "http://www.google.com/",
                                     TileSource::TOP_SITES)))));
}

INSTANTIATE_TEST_SUITE_P(MostVisitedSitesTest,
                         MostVisitedSitesTest,
                         ::testing::Bool());

TEST(MostVisitedSitesTest, ShouldDeduplicateDomainWithNoWwwDomain) {
  EXPECT_TRUE(MostVisitedSites::IsHostOrMobilePageKnown({"www.mobile.de"},
                                                        "mobile.de"));
  EXPECT_TRUE(MostVisitedSites::IsHostOrMobilePageKnown({"mobile.de"},
                                                        "www.mobile.de"));
  EXPECT_TRUE(MostVisitedSites::IsHostOrMobilePageKnown({"mobile.co.uk"},
                                                        "www.mobile.co.uk"));
}

TEST(MostVisitedSitesTest, ShouldDeduplicateDomainByRemovingMobilePrefixes) {
  EXPECT_TRUE(
      MostVisitedSites::IsHostOrMobilePageKnown({"bbc.co.uk"}, "m.bbc.co.uk"));
  EXPECT_TRUE(
      MostVisitedSites::IsHostOrMobilePageKnown({"m.bbc.co.uk"}, "bbc.co.uk"));
  EXPECT_TRUE(MostVisitedSites::IsHostOrMobilePageKnown({"cnn.com"},
                                                        "edition.cnn.com"));
  EXPECT_TRUE(MostVisitedSites::IsHostOrMobilePageKnown({"edition.cnn.com"},
                                                        "cnn.com"));
  EXPECT_TRUE(
      MostVisitedSites::IsHostOrMobilePageKnown({"cnn.com"}, "mobile.cnn.com"));
  EXPECT_TRUE(
      MostVisitedSites::IsHostOrMobilePageKnown({"mobile.cnn.com"}, "cnn.com"));
}

TEST(MostVisitedSitesTest, ShouldDeduplicateDomainByReplacingMobilePrefixes) {
  EXPECT_TRUE(MostVisitedSites::IsHostOrMobilePageKnown({"www.bbc.co.uk"},
                                                        "m.bbc.co.uk"));
  EXPECT_TRUE(MostVisitedSites::IsHostOrMobilePageKnown({"m.mobile.de"},
                                                        "www.mobile.de"));
  EXPECT_TRUE(MostVisitedSites::IsHostOrMobilePageKnown({"www.cnn.com"},
                                                        "edition.cnn.com"));
  EXPECT_TRUE(MostVisitedSites::IsHostOrMobilePageKnown({"mobile.cnn.com"},
                                                        "www.cnn.com"));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
class MostVisitedSitesWithCustomLinksTest : public MostVisitedSitesTest {
 public:
  MostVisitedSitesWithCustomLinksTest() {
    EnableCustomLinks();
    RecreateMostVisitedSites();
  }

  void ExpectBuildWithTopSites(
      const MostVisitedURLList& expected_list,
      std::map<SectionType, NTPTilesVector>* sections) {
    EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
        .WillRepeatedly(
            base::test::RunOnceCallbackRepeatedly<0>(expected_list));
    EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
    EXPECT_CALL(*mock_custom_links_, IsInitialized())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
        .WillOnce(SaveArg<0>(sections));
  }

  void ExpectBuildWithCustomLinks(
      const std::vector<CustomLinksManager::Link>& expected_links,
      std::map<SectionType, NTPTilesVector>* sections) {
    EXPECT_CALL(*mock_custom_links_, IsInitialized())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_custom_links_, GetLinks())
        .WillOnce(ReturnRef(expected_links));
    EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
        .WillOnce(SaveArg<0>(sections));
  }
};

TEST_P(MostVisitedSitesWithCustomLinksTest, ChangeVisibility) {
  const char kTestUrl[] = "http://site1/";
  const char16_t kTestTitle[] = u"Site 1";
  std::map<SectionType, NTPTilesVector> sections;

  // Build tiles when custom links is not initialized. Tiles should be Top
  // Sites.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithTopSites(
      MostVisitedURLList{MakeMostVisitedURL(kTestTitle, kTestUrl)}, &sections);
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/1);
  base::RunLoop().RunUntilIdle();
  NTPTilesVector tiles = sections.at(SectionType::PERSONALIZED);
  ASSERT_THAT(tiles.size(), Ge(1ul));
  ASSERT_THAT(tiles[0],
              MatchesTile(kTestTitle, kTestUrl, TileSource::TOP_SITES));

  EXPECT_TRUE(most_visited_sites_->IsCustomLinksEnabled());
  EXPECT_TRUE(most_visited_sites_->IsShortcutsVisible());

  // Hide shortcuts. Observer should get notified.
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_)).Times(1);
  most_visited_sites_->SetShortcutsVisible(false);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(most_visited_sites_->IsCustomLinksEnabled());
  EXPECT_FALSE(most_visited_sites_->IsShortcutsVisible());

  // Attempt to hide the shortcuts again. This should be ignored.
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_)).Times(0);
  most_visited_sites_->SetShortcutsVisible(false);
  base::RunLoop().RunUntilIdle();

  // Make the shortcuts visible. Observer should get notified.
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_)).Times(1);
  most_visited_sites_->SetShortcutsVisible(true);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(most_visited_sites_->IsCustomLinksEnabled());
  EXPECT_TRUE(most_visited_sites_->IsShortcutsVisible());
}

TEST_P(MostVisitedSitesWithCustomLinksTest,
       ShouldOnlyBuildCustomLinksWhenInitialized) {
  const char kTestUrl[] = "http://site1/";
  const char16_t kTestTitle[] = u"Site 1";
  std::vector<CustomLinksManager::Link> expected_links(
      {CustomLinksManager::Link{GURL(kTestUrl), kTestTitle}});
  std::map<SectionType, NTPTilesVector> sections;

  // Build tiles when custom links is not initialized. Tiles should be Top
  // Sites.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithTopSites(
      MostVisitedURLList{MakeMostVisitedURL(kTestTitle, kTestUrl)}, &sections);
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/1);
  base::RunLoop().RunUntilIdle();
  NTPTilesVector tiles = sections.at(SectionType::PERSONALIZED);
  ASSERT_THAT(tiles.size(), Ge(1ul));
  ASSERT_THAT(tiles[0],
              MatchesTile(kTestTitle, kTestUrl, TileSource::TOP_SITES));

  // Initialize custom links and rebuild tiles. Tiles should be custom links.
  EXPECT_CALL(*mock_custom_links_, Initialize(_)).WillOnce(Return(true));
  ExpectBuildWithCustomLinks(expected_links, &sections);
  most_visited_sites_->InitializeCustomLinks();
  most_visited_sites_->RefreshTiles();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle, kTestUrl, TileSource::CUSTOM_LINKS)));

  // Uninitialize custom links and rebuild tiles. Tiles should be Top Sites.
  EXPECT_CALL(*mock_custom_links_, Uninitialize());
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(
          MostVisitedURLList{MakeMostVisitedURL(kTestTitle, kTestUrl)}));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections));
  most_visited_sites_->UninitializeCustomLinks();
  base::RunLoop().RunUntilIdle();
  tiles = sections.at(SectionType::PERSONALIZED);
  ASSERT_THAT(tiles.size(), Ge(1ul));
  ASSERT_THAT(tiles[0],
              MatchesTile(kTestTitle, kTestUrl, TileSource::TOP_SITES));
}

TEST_P(MostVisitedSitesWithCustomLinksTest,
       ShouldFavorCustomLinksOverTopSites) {
  const char kTestUrl[] = "http://site1/";
  const char16_t kTestTitle[] = u"Site 1";
  std::vector<CustomLinksManager::Link> expected_links(
      {CustomLinksManager::Link{GURL(kTestUrl), kTestTitle}});
  std::map<SectionType, NTPTilesVector> sections;

  // Build tiles when custom links is not initialized. Tiles should be Top
  // Sites.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithTopSites(
      MostVisitedURLList{MakeMostVisitedURL(kTestTitle, kTestUrl)}, &sections);
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/1);
  base::RunLoop().RunUntilIdle();
  NTPTilesVector tiles = sections.at(SectionType::PERSONALIZED);
  ASSERT_THAT(tiles.size(), Ge(1ul));
  ASSERT_THAT(tiles[0],
              MatchesTile(kTestTitle, kTestUrl, TileSource::TOP_SITES));

  // Initialize custom links and rebuild tiles. Tiles should be custom links.
  EXPECT_CALL(*mock_custom_links_, Initialize(_)).WillOnce(Return(true));
  ExpectBuildWithCustomLinks(expected_links, &sections);
  most_visited_sites_->InitializeCustomLinks();
  most_visited_sites_->RefreshTiles();
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle, kTestUrl, TileSource::CUSTOM_LINKS)));

  // Initiate notification for new Top Sites. This should be ignored.
  VerifyAndClearExpectations();
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_)).Times(0);
  top_sites_callbacks_.Notify(
      MostVisitedURLList({MakeMostVisitedURL(u"Site 2", "http://site2/")}));
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesWithCustomLinksTest,
       DisableCustomLinksWhenNotInitialized) {
  const char kTestUrl[] = "http://site1/";
  const char16_t kTestTitle16[] = u"Site 1";
  std::vector<CustomLinksManager::Link> expected_links(
      {CustomLinksManager::Link{GURL(kTestUrl), kTestTitle16}});
  std::map<SectionType, NTPTilesVector> sections;

  // Build tiles when custom links is not initialized. Tiles should be from
  // Top Sites.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithTopSites(
      MostVisitedURLList{MakeMostVisitedURL(kTestTitle16, kTestUrl)},
      &sections);
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/1);
  base::RunLoop().RunUntilIdle();
  NTPTilesVector tiles = sections.at(SectionType::PERSONALIZED);
  ASSERT_THAT(tiles.size(), Ge(1ul));
  ASSERT_THAT(tiles[0],
              MatchesTile(kTestTitle16, kTestUrl, TileSource::TOP_SITES));

  // Disable custom links. Tiles should rebuild.
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(
          MostVisitedURLList{MakeMostVisitedURL(kTestTitle16, kTestUrl)}));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_)).Times(1);
  most_visited_sites_->EnableCustomLinks(false);
  base::RunLoop().RunUntilIdle();

  // Try to disable custom links again. This should not rebuild the tiles.
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_)).Times(0);
  EXPECT_CALL(*mock_custom_links_, GetLinks()).Times(0);
  most_visited_sites_->EnableCustomLinks(false);
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesWithCustomLinksTest, DisableCustomLinksWhenInitialized) {
  const char kTestUrl[] = "http://site1/";
  const char16_t kTestTitle16[] = u"Site 1";
  std::vector<CustomLinksManager::Link> expected_links(
      {CustomLinksManager::Link{GURL(kTestUrl), kTestTitle16}});
  std::map<SectionType, NTPTilesVector> sections;

  // Build tiles when custom links is initialized and not disabled. Tiles should
  // be custom links.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  EXPECT_CALL(*mock_top_sites_, SyncWithHistory());
  ExpectBuildWithCustomLinks(expected_links, &sections);
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/1);
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(sections.at(SectionType::PERSONALIZED),
              ElementsAre(MatchesTile(kTestTitle16, kTestUrl,
                                      TileSource::CUSTOM_LINKS)));

  // Disable custom links. Tiles should rebuild and return Top Sites.
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(
          MostVisitedURLList{MakeMostVisitedURL(kTestTitle16, kTestUrl)}));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections));
  most_visited_sites_->EnableCustomLinks(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle16, kTestUrl, TileSource::TOP_SITES)));

  // Re-enable custom links. Tiles should rebuild and return custom links.
  ExpectBuildWithCustomLinks(expected_links, &sections);
  most_visited_sites_->EnableCustomLinks(true);
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(sections.at(SectionType::PERSONALIZED),
              ElementsAre(MatchesTile(kTestTitle16, kTestUrl,
                                      TileSource::CUSTOM_LINKS)));
}

TEST_P(MostVisitedSitesWithCustomLinksTest,
       ShouldGenerateShortTitleForTopSites) {
  std::string kTestUrl1 = "https://www.imdb.com/";
  std::u16string kTestTitle1 = u"IMDb - Movies, TV and Celebrities - IMDb";
  std::string kTestUrl2 = "https://drive.google.com/";
  std::u16string kTestTitle2 =
      u"Google Drive - Cloud Storage & File Backup for Photos, Docs & More";
  std::string kTestUrl3 = "https://amazon.com/";
  std::u16string kTestTitle3 =
      u"Amazon.com: Online Shopping for Electronics, Apparel, Computers, "
      u"Books, "
      u"DVDs & more";
  std::map<SectionType, NTPTilesVector> sections;

  // Build tiles from Top Sites. The tiles should have short titles.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithTopSites(
      MostVisitedURLList{MakeMostVisitedURL(kTestTitle1, kTestUrl1),
                         MakeMostVisitedURL(kTestTitle2, kTestUrl2),
                         MakeMostVisitedURL(kTestTitle3, kTestUrl3)},
      &sections);
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/3);
  base::RunLoop().RunUntilIdle();

  NTPTilesVector tiles = sections.at(SectionType::PERSONALIZED);
  ASSERT_THAT(tiles.size(), Ge(3ul));
  ASSERT_THAT(
      tiles[0],
      MatchesTile(/* The short title generated by the heuristic */ u"IMDb",
                  kTestUrl1, TileSource::TOP_SITES));
  ASSERT_THAT(
      tiles[1],
      MatchesTile(
          /* The short title generated by the heuristic */ u"Google Drive",
          kTestUrl2, TileSource::TOP_SITES));
  ASSERT_THAT(
      tiles[2],
      MatchesTile(
          /* The short title generated by the heuristic */ u"Amazon.com",
          kTestUrl3, TileSource::TOP_SITES));
}

// Test all delimiters
TEST_P(MostVisitedSitesWithCustomLinksTest,
       ShouldSplitTitleWithSpaceAfterDelimiter) {
  // No space before delimiter
  std::string kTestUrl1 = "https://example1.com/";
  std::u16string kTestTitle1 = u"Example1: Amazing Website";
  std::string kTestUrl2 = "https://example2.com/";
  std::u16string kTestTitle2 = u"Example2; Amazing Website";
  // Has space before delimiter
  std::string kTestUrl3 = "https://example3.com/";
  std::u16string kTestTitle3 = u"Example3 - Amazing Website";
  std::string kTestUrl4 = "https://example4.com/";
  std::u16string kTestTitle4 = u"Example4 | Amazing Website";
  // Repeated delimiter
  std::string kTestUrl5 = "https://example5.com/";
  std::u16string kTestTitle5 = u"Example5 :: Amazing Website";
  // Multiple delimiters
  std::string kTestUrl6 = "https://example6.com/";
  std::u16string kTestTitle6 = u"Example6 - Amazing Website - More Title";
  std::map<SectionType, NTPTilesVector> sections;

  // Build tiles from Top Sites. The tiles should have short titles.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithTopSites(
      MostVisitedURLList{MakeMostVisitedURL(kTestTitle1, kTestUrl1),
                         MakeMostVisitedURL(kTestTitle2, kTestUrl2),
                         MakeMostVisitedURL(kTestTitle3, kTestUrl3),
                         MakeMostVisitedURL(kTestTitle4, kTestUrl4),
                         MakeMostVisitedURL(kTestTitle5, kTestUrl5),
                         MakeMostVisitedURL(kTestTitle6, kTestUrl6)},
      &sections);
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/6);
  base::RunLoop().RunUntilIdle();

  NTPTilesVector tiles = sections.at(SectionType::PERSONALIZED);
  ASSERT_THAT(tiles.size(), Ge(6ul));
  ASSERT_THAT(
      tiles[0],
      MatchesTile(/* The short title generated by the heuristic */ u"Example1",
                  kTestUrl1, TileSource::TOP_SITES));
  ASSERT_THAT(tiles[1],
              MatchesTile(
                  /* The short title generated by the heuristic */ u"Example2",
                  kTestUrl2, TileSource::TOP_SITES));
  ASSERT_THAT(tiles[2],
              MatchesTile(
                  /* The short title generated by the heuristic */ u"Example3",
                  kTestUrl3, TileSource::TOP_SITES));
  ASSERT_THAT(tiles[3],
              MatchesTile(
                  /* The short title generated by the heuristic */ u"Example4",
                  kTestUrl4, TileSource::TOP_SITES));
  ASSERT_THAT(tiles[4],
              MatchesTile(
                  /* The short title generated by the heuristic */ u"Example5",
                  kTestUrl5, TileSource::TOP_SITES));
  ASSERT_THAT(tiles[5],
              MatchesTile(
                  /* The short title generated by the heuristic */ u"Example6",
                  kTestUrl6, TileSource::TOP_SITES));
}

TEST_P(MostVisitedSitesWithCustomLinksTest,
       ShouldUseFullTitleIfTitleDoesNotContainDelimiterFollowedBySpace) {
  // No space after delimiter
  std::string kTestUrl1 = "https://example1.com/";
  std::u16string kTestTitle1 = u"Example1 Web Services Sign-In";
  std::string kTestUrl2 = "https://example2.com/";
  std::u16string kTestTitle2 = u"Example2 Sign-In";
  // No delimiter
  std::string kTestUrl3 = "https://example3.com/";
  std::u16string kTestTitle3 = u"Example3 is an Amazing Website";
  // Many spaces between words
  std::string kTestUrl4 = "https://example4.com/";
  std::u16string kTestTitle4 = u"Example4   Many   Spaces";
  std::string kTestUrl5 = "https://example5.com/";
  std::u16string kTestTitle5 = u"   Example5   Padded   With   Spaces  ";
  // No space after delimiter
  std::string kTestUrl6 = "https://example6.com/";
  std::u16string kTestTitle6 = u"This::Is::Fancy::Title";
  std::string kTestUrl7 = "https://example7.com/";
  std::u16string kTestTitle7 = u"Example7-Hello";
  std::map<SectionType, NTPTilesVector> sections;

  // Build tiles from Top Sites. The tiles should have short titles.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithTopSites(
      MostVisitedURLList{MakeMostVisitedURL(kTestTitle1, kTestUrl1),
                         MakeMostVisitedURL(kTestTitle2, kTestUrl2),
                         MakeMostVisitedURL(kTestTitle3, kTestUrl3),
                         MakeMostVisitedURL(kTestTitle4, kTestUrl4),
                         MakeMostVisitedURL(kTestTitle5, kTestUrl5),
                         MakeMostVisitedURL(kTestTitle6, kTestUrl6),
                         MakeMostVisitedURL(kTestTitle7, kTestUrl7)},
      &sections);
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/7);
  base::RunLoop().RunUntilIdle();

  NTPTilesVector tiles = sections.at(SectionType::PERSONALIZED);
  ASSERT_THAT(tiles.size(), Ge(7ul));
  ASSERT_THAT(tiles[0],
              MatchesTile(kTestTitle1, kTestUrl1, TileSource::TOP_SITES));
  ASSERT_THAT(tiles[1],
              MatchesTile(kTestTitle2, kTestUrl2, TileSource::TOP_SITES));
  ASSERT_THAT(tiles[2],
              MatchesTile(kTestTitle3, kTestUrl3, TileSource::TOP_SITES));
  ASSERT_THAT(tiles[3],
              MatchesTile(kTestTitle4, kTestUrl4, TileSource::TOP_SITES));
  ASSERT_THAT(tiles[4], MatchesTile(u"Example5   Padded   With   Spaces",
                                    kTestUrl5, TileSource::TOP_SITES));
  ASSERT_THAT(tiles[5],
              MatchesTile(kTestTitle6, kTestUrl6, TileSource::TOP_SITES));
  ASSERT_THAT(tiles[6],
              MatchesTile(kTestTitle7, kTestUrl7, TileSource::TOP_SITES));
}

TEST_P(MostVisitedSitesWithCustomLinksTest,
       ShouldNotCrashIfReceiveAnEmptyTitle) {
  std::string kTestUrl1 = "https://site1/";
  std::u16string kTestTitle1 = u"";  // Empty title
  std::string kTestUrl2 = "https://site2/";
  std::u16string kTestTitle2 = u"       ";  // Title only contains spaces
  std::map<SectionType, NTPTilesVector> sections;

  // Build tiles from Top Sites. The tiles should have short titles.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithTopSites(
      MostVisitedURLList{MakeMostVisitedURL(kTestTitle1, kTestUrl1),
                         MakeMostVisitedURL(kTestTitle2, kTestUrl2)},
      &sections);
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/2);
  base::RunLoop().RunUntilIdle();

  // Both cases should not crash and generate an empty title tile.
  NTPTilesVector tiles = sections.at(SectionType::PERSONALIZED);
  ASSERT_THAT(tiles.size(), Ge(2ul));
  ASSERT_THAT(tiles[0], MatchesTile(u"", kTestUrl1, TileSource::TOP_SITES));
  ASSERT_THAT(tiles[1], MatchesTile(u"", kTestUrl2, TileSource::TOP_SITES));
}

TEST_P(MostVisitedSitesWithCustomLinksTest,
       UninitializeCustomLinksOnUndoAfterFirstAction) {
  const char kTestUrl[] = "http://site1/";
  const char16_t kTestTitle[] = u"Site 1";
  std::vector<CustomLinksManager::Link> expected_links(
      {CustomLinksManager::Link{GURL(kTestUrl), kTestTitle}});
  std::map<SectionType, NTPTilesVector> sections;

  // Build initial tiles with Top Sites.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithTopSites(
      MostVisitedURLList{MakeMostVisitedURL(kTestTitle, kTestUrl)}, &sections);
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/1);
  base::RunLoop().RunUntilIdle();
  NTPTilesVector tiles = sections.at(SectionType::PERSONALIZED);
  ASSERT_THAT(tiles.size(), Ge(1ul));
  ASSERT_THAT(tiles[0],
              MatchesTile(kTestTitle, kTestUrl, TileSource::TOP_SITES));

  // Initialize custom links and complete a custom link action.
  EXPECT_CALL(*mock_custom_links_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, AddLink(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_custom_links_, GetLinks())
      .WillRepeatedly(ReturnRef(expected_links));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections));
  most_visited_sites_->AddCustomLink(GURL("test.com"), u"test");
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle, kTestUrl, TileSource::CUSTOM_LINKS)));

  // Undo the action. This should uninitialize custom links.
  EXPECT_CALL(*mock_custom_links_, UndoAction()).Times(0);
  EXPECT_CALL(*mock_custom_links_, Uninitialize());
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(
          MostVisitedURLList{MakeMostVisitedURL(kTestTitle, kTestUrl)}));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections));
  most_visited_sites_->UndoCustomLinkAction();
  base::RunLoop().RunUntilIdle();
  tiles = sections.at(SectionType::PERSONALIZED);
  ASSERT_THAT(tiles.size(), Ge(1ul));
  ASSERT_THAT(tiles[0],
              MatchesTile(kTestTitle, kTestUrl, TileSource::TOP_SITES));
}

TEST_P(MostVisitedSitesWithCustomLinksTest,
       DontUninitializeCustomLinksOnUndoAfterMultipleActions) {
  const char kTestUrl[] = "http://site1/";
  const char16_t kTestTitle[] = u"Site 1";
  std::vector<CustomLinksManager::Link> expected_links(
      {CustomLinksManager::Link{GURL(kTestUrl), kTestTitle}});
  std::map<SectionType, NTPTilesVector> sections;

  // Build initial tiles with Top Sites.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithTopSites(
      MostVisitedURLList{MakeMostVisitedURL(kTestTitle, kTestUrl)}, &sections);
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/1);
  base::RunLoop().RunUntilIdle();
  NTPTilesVector tiles = sections.at(SectionType::PERSONALIZED);
  ASSERT_THAT(tiles.size(), Ge(1ul));
  ASSERT_THAT(tiles[0],
              MatchesTile(kTestTitle, kTestUrl, TileSource::TOP_SITES));

  // Initialize custom links and complete a custom link action.
  EXPECT_CALL(*mock_custom_links_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, UpdateLink(_, _, _)).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_custom_links_, GetLinks())
      .WillRepeatedly(ReturnRef(expected_links));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillRepeatedly(SaveArg<0>(&sections));
  most_visited_sites_->UpdateCustomLink(GURL("test.com"), GURL("test.com"),
                                        u"test");
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(
      sections.at(SectionType::PERSONALIZED),
      ElementsAre(MatchesTile(kTestTitle, kTestUrl, TileSource::CUSTOM_LINKS)));

  // Complete a second custom link action.
  EXPECT_CALL(*mock_custom_links_, Initialize(_)).WillOnce(Return(false));
  EXPECT_CALL(*mock_custom_links_, DeleteLink(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_custom_links_, GetLinks())
      .WillOnce(ReturnRef(expected_links));
  most_visited_sites_->DeleteCustomLink(GURL("test.com"));
  base::RunLoop().RunUntilIdle();

  // Undo the second action. This should not uninitialize custom links.
  EXPECT_CALL(*mock_custom_links_, UndoAction()).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, Uninitialize()).Times(0);
  EXPECT_CALL(*mock_custom_links_, IsInitialized()).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, GetLinks())
      .WillOnce(ReturnRef(expected_links));
  most_visited_sites_->UndoCustomLinkAction();
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesWithCustomLinksTest,
       UninitializeCustomLinksIfFirstActionFails) {
  const char kTestUrl[] = "http://site1/";
  const char16_t kTestTitle[] = u"Site 1";
  std::vector<CustomLinksManager::Link> expected_links(
      {CustomLinksManager::Link{GURL(kTestUrl), kTestTitle}});
  std::map<SectionType, NTPTilesVector> sections;

  // Build initial tiles with Top Sites.
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_));
  ExpectBuildWithTopSites(
      MostVisitedURLList{MakeMostVisitedURL(kTestTitle, kTestUrl)}, &sections);
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/1);
  base::RunLoop().RunUntilIdle();
  NTPTilesVector tiles = sections.at(SectionType::PERSONALIZED);
  ASSERT_THAT(tiles.size(), Ge(1ul));
  ASSERT_THAT(tiles[0],
              MatchesTile(kTestTitle, kTestUrl, TileSource::TOP_SITES));

  // Fail to add a custom link. This should not initialize custom links but
  // notify.
  EXPECT_CALL(*mock_custom_links_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, AddLink(_, _)).WillOnce(Return(false));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_custom_links_, Uninitialize());
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_)).Times(1);
  most_visited_sites_->AddCustomLink(GURL(kTestUrl), u"test");
  base::RunLoop().RunUntilIdle();

  // Fail to edit a custom link. This should not initialize custom links but
  // notify.
  EXPECT_CALL(*mock_custom_links_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, UpdateLink(_, _, _)).WillOnce(Return(false));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_custom_links_, Uninitialize());
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_)).Times(1);
  most_visited_sites_->UpdateCustomLink(GURL("test.com"), GURL("test2.com"),
                                        u"test");
  base::RunLoop().RunUntilIdle();

  // Fail to reorder a custom link. This should not initialize custom links but
  // notify.
  EXPECT_CALL(*mock_custom_links_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, ReorderLink(_, _)).WillOnce(Return(false));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_custom_links_, Uninitialize());
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_)).Times(1);
  most_visited_sites_->ReorderCustomLink(GURL("test.com"), 1);
  base::RunLoop().RunUntilIdle();

  // Fail to delete a custom link. This should not initialize custom links but
  // notify.
  EXPECT_CALL(*mock_custom_links_, Initialize(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_custom_links_, DeleteLink(_)).WillOnce(Return(false));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_custom_links_, Uninitialize());
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_)).Times(1);
  most_visited_sites_->DeleteCustomLink(GURL("test.com"));
  base::RunLoop().RunUntilIdle();
}

TEST_P(MostVisitedSitesWithCustomLinksTest, RebuildTilesOnCustomLinksChanged) {
  const char kTestUrl1[] = "http://site1/";
  const char kTestUrl2[] = "http://site2/";
  const char16_t kTestTitle1[] = u"Site 1";
  const char16_t kTestTitle2[] = u"Site 2";
  std::vector<CustomLinksManager::Link> expected_links(
      {CustomLinksManager::Link{GURL(kTestUrl2), kTestTitle2}});
  std::map<SectionType, NTPTilesVector> sections;

  // Build initial tiles with Top Sites.
  base::RepeatingClosure custom_links_callback;
  EXPECT_CALL(*mock_custom_links_, RegisterCallbackForOnChanged(_))
      .WillOnce(DoAll(SaveArg<0>(&custom_links_callback),
                      Return(ByMove(base::CallbackListSubscription()))));
  ExpectBuildWithTopSites(
      MostVisitedURLList{MakeMostVisitedURL(kTestTitle1, kTestUrl1)},
      &sections);
  most_visited_sites_->AddMostVisitedURLsObserver(&mock_observer_,
                                                  /*max_num_sites=*/1);
  base::RunLoop().RunUntilIdle();
  NTPTilesVector tiles = sections.at(SectionType::PERSONALIZED);
  ASSERT_THAT(tiles.size(), Ge(1ul));
  ASSERT_THAT(tiles[0],
              MatchesTile(kTestTitle1, kTestUrl1, TileSource::TOP_SITES));

  // Notify that there is a new set of custom links. This should replace the
  // current tiles with custom links.
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_custom_links_, GetLinks())
      .WillRepeatedly(ReturnRef(expected_links));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections));
  custom_links_callback.Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(sections.at(SectionType::PERSONALIZED),
              ElementsAre(MatchesTile(kTestTitle2, kTestUrl2,
                                      TileSource::CUSTOM_LINKS)));

  // Notify that custom links have been uninitialized. This should rebuild the
  // tiles with Top Sites.
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_top_sites_, GetMostVisitedURLs(_))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(
          MostVisitedURLList{MakeMostVisitedURL(kTestTitle1, kTestUrl1)}));
  EXPECT_CALL(*mock_custom_links_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_observer_, OnURLsAvailable(_))
      .WillOnce(SaveArg<0>(&sections));
  custom_links_callback.Run();
  base::RunLoop().RunUntilIdle();
  tiles = sections.at(SectionType::PERSONALIZED);
  ASSERT_THAT(tiles.size(), Ge(1ul));
  ASSERT_THAT(tiles[0],
              MatchesTile(kTestTitle1, kTestUrl1, TileSource::TOP_SITES));
}

// These exclude Android and iOS.
INSTANTIATE_TEST_SUITE_P(MostVisitedSitesWithCustomLinksTest,
                         MostVisitedSitesWithCustomLinksTest,
                         ::testing::Bool());
#endif

// This a test for MostVisitedSites::MergeTiles(...) method, and thus has the
// same scope as the method itself. This tests merging popular sites with
// personal tiles.
// More important things out of the scope of testing presently:
// - Removing blocked tiles.
// - Correct host extraction from the URL.
// - Ensuring personal tiles are not duplicated in popular tiles.
TEST(MostVisitedSitesMergeTest, ShouldMergeTilesWithPersonalOnly) {
  std::vector<NTPTile> personal_tiles{
      MakeTile(u"Site 1", "https://www.site1.com/", TileSource::TOP_SITES),
      MakeTile(u"Site 2", "https://www.site2.com/", TileSource::TOP_SITES),
      MakeTile(u"Site 3", "https://www.site3.com/", TileSource::TOP_SITES),
      MakeTile(u"Site 4", "https://www.site4.com/", TileSource::TOP_SITES),
  };
  // Without any popular tiles, the result after merge should be the personal
  // tiles.
  EXPECT_THAT(MostVisitedSites::MergeTiles(std::move(personal_tiles),
                                           /*popular_tiles=*/NTPTilesVector()),
              ElementsAre(MatchesTile(u"Site 1", "https://www.site1.com/",
                                      TileSource::TOP_SITES),
                          MatchesTile(u"Site 2", "https://www.site2.com/",
                                      TileSource::TOP_SITES),
                          MatchesTile(u"Site 3", "https://www.site3.com/",
                                      TileSource::TOP_SITES),
                          MatchesTile(u"Site 4", "https://www.site4.com/",
                                      TileSource::TOP_SITES)));
}

TEST(MostVisitedSitesMergeTest, ShouldMergeTilesWithPopularOnly) {
  std::vector<NTPTile> popular_tiles{
      MakeTile(u"Site 1", "https://www.site1.com/", TileSource::POPULAR),
      MakeTile(u"Site 2", "https://www.site2.com/", TileSource::POPULAR),
      MakeTile(u"Site 3", "https://www.site3.com/", TileSource::POPULAR),
      MakeTile(u"Site 4", "https://www.site4.com/", TileSource::POPULAR),
  };
  // Without any personal tiles, the result after merge should be the popular
  // tiles.
  EXPECT_THAT(
      MostVisitedSites::MergeTiles(/*personal_tiles=*/NTPTilesVector(),
                                   /*popular_tiles=*/std::move(popular_tiles)),
      ElementsAre(
          MatchesTile(u"Site 1", "https://www.site1.com/", TileSource::POPULAR),
          MatchesTile(u"Site 2", "https://www.site2.com/", TileSource::POPULAR),
          MatchesTile(u"Site 3", "https://www.site3.com/", TileSource::POPULAR),
          MatchesTile(u"Site 4", "https://www.site4.com/",
                      TileSource::POPULAR)));
}

TEST(MostVisitedSitesMergeTest, ShouldMergeTilesFavoringPersonalOverPopular) {
  std::vector<NTPTile> popular_tiles{
      MakeTile(u"Site 1", "https://www.site1.com/", TileSource::POPULAR),
      MakeTile(u"Site 2", "https://www.site2.com/", TileSource::POPULAR),
  };
  std::vector<NTPTile> personal_tiles{
      MakeTile(u"Site 3", "https://www.site3.com/", TileSource::TOP_SITES),
      MakeTile(u"Site 4", "https://www.site4.com/", TileSource::TOP_SITES),
  };
  EXPECT_THAT(
      MostVisitedSites::MergeTiles(std::move(personal_tiles),
                                   /*popular_tiles=*/std::move(popular_tiles)),
      ElementsAre(
          MatchesTile(u"Site 3", "https://www.site3.com/",
                      TileSource::TOP_SITES),
          MatchesTile(u"Site 4", "https://www.site4.com/",
                      TileSource::TOP_SITES),
          MatchesTile(u"Site 1", "https://www.site1.com/", TileSource::POPULAR),
          MatchesTile(u"Site 2", "https://www.site2.com/",
                      TileSource::POPULAR)));
}

}  // namespace ntp_tiles
