// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/history_backend.h"

#include <stddef.h>

#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/favicon/core/favicon_backend.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/in_memory_database.h"
#include "components/history/core/browser/in_memory_history_backend.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/sync/typed_url_sync_bridge.h"
#include "components/history/core/test/database_test_utils.h"
#include "components/history/core/test/history_client_fake_bookmarks.h"
#include "components/history/core/test/test_history_database.h"
#include "components/history/core/test/visit_annotations_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/sqlite/sqlite3.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

// This file only tests functionality where it is most convenient to call the
// backend directly. Most of the history backend functions are tested by the
// history unit test. Because of the elaborate callbacks involved, this is no
// harder than calling it directly for many things.

namespace history {

namespace {

using favicon::FaviconBitmap;
using favicon::FaviconBitmapType;
using favicon::IconMapping;
using favicon_base::IconType;
using favicon_base::IconTypeSet;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

const int kSmallEdgeSize = 16;
const int kLargeEdgeSize = 32;

const gfx::Size kSmallSize = gfx::Size(kSmallEdgeSize, kSmallEdgeSize);
const gfx::Size kLargeSize = gfx::Size(kLargeEdgeSize, kLargeEdgeSize);

MATCHER_P(HasVisitID, visit_id, "") {
  return arg.visit_id == visit_id;
}

// Minimal representation of a `Cluster` for verifying 2 clusters are equal.
struct ClusterExpectation {
  int64_t cluster_id;
  std::vector<VisitID> visit_ids;
};

using SimulateNotificationCallback =
    base::RepeatingCallback<void(const URLRow*, const URLRow*, const URLRow*)>;

void SimulateNotificationURLVisited(HistoryServiceObserver* observer,
                                    const URLRow* row1,
                                    const URLRow* row2,
                                    const URLRow* row3) {
  std::vector<URLRow> rows;
  rows.push_back(*row1);
  if (row2)
    rows.push_back(*row2);
  if (row3)
    rows.push_back(*row3);

  for (const URLRow& row : rows) {
    observer->OnURLVisited(nullptr, row, VisitRow());
  }
}

void SimulateNotificationURLsModified(HistoryServiceObserver* observer,
                                      const URLRow* row1,
                                      const URLRow* row2,
                                      const URLRow* row3) {
  URLRows rows;
  rows.push_back(*row1);
  if (row2)
    rows.push_back(*row2);
  if (row3)
    rows.push_back(*row3);

  observer->OnURLsModified(nullptr, rows);
}

VisitContextAnnotations MakeContextAnnotations(bool omnibox_url_copied) {
  VisitContextAnnotations result;
  result.omnibox_url_copied = omnibox_url_copied;
  return result;
}

}  // namespace

class HistoryBackendTestBase;

// This must be a separate object since HistoryBackend manages its lifetime.
// This just forwards the messages we're interested in to the test object.
class HistoryBackendTestDelegate : public HistoryBackend::Delegate {
 public:
  explicit HistoryBackendTestDelegate(HistoryBackendTestBase* test)
      : test_(test) {}
  HistoryBackendTestDelegate(const HistoryBackendTestDelegate&) = delete;
  HistoryBackendTestDelegate& operator=(const HistoryBackendTestDelegate&) =
      delete;

  bool CanAddURL(const GURL& url) const override;
  void NotifyProfileError(sql::InitStatus init_status,
                          const std::string& diagnostics) override {}
  void SetInMemoryBackend(
      std::unique_ptr<InMemoryHistoryBackend> backend) override;
  void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                             const GURL& icon_url) override;
  void NotifyURLVisited(const URLRow& url_row,
                        const VisitRow& visit_row) override;
  void NotifyURLsModified(const URLRows& changed_urls) override;
  void NotifyURLsDeleted(DeletionInfo deletion_info) override;
  void NotifyKeywordSearchTermUpdated(const URLRow& row,
                                      KeywordID keyword_id,
                                      const std::u16string& term) override;
  void NotifyKeywordSearchTermDeleted(URLID url_id) override;
  void DBLoaded() override;

 private:
  // Not owned by us.
  raw_ptr<HistoryBackendTestBase> test_;
};

// Exposes some of `HistoryBackend`'s private methods.
class TestHistoryBackend : public HistoryBackend {
 public:
  using HistoryBackend::AddPageVisit;
  using HistoryBackend::DeleteAllHistory;
  using HistoryBackend::DeleteFTSIndexDatabases;
  using HistoryBackend::HistoryBackend;
  using HistoryBackend::UpdateVisitDuration;

  using HistoryBackend::db_;
  using HistoryBackend::expirer_;
  using HistoryBackend::favicon_backend_;
  using HistoryBackend::recent_redirects_;

  VisitTracker& visit_tracker() { return tracker_; }

 private:
  ~TestHistoryBackend() override = default;
};

class HistoryBackendTestBase : public testing::Test {
 public:
  typedef std::vector<std::pair<URLRow, VisitRow>> URLVisitedList;
  typedef std::vector<URLRows> URLsModifiedList;
  typedef std::vector<std::pair<bool, bool>> URLsDeletedList;

  HistoryBackendTestBase() = default;
  HistoryBackendTestBase(const HistoryBackendTestBase&) = delete;
  HistoryBackendTestBase& operator=(const HistoryBackendTestBase&) = delete;
  ~HistoryBackendTestBase() override = default;

 protected:
  std::vector<GURL> favicon_changed_notifications_page_urls() const {
    return favicon_changed_notifications_page_urls_;
  }

  std::vector<GURL> favicon_changed_notifications_icon_urls() const {
    return favicon_changed_notifications_icon_urls_;
  }

  int num_url_visited_notifications() const {
    return url_visited_notifications_.size();
  }

  const URLVisitedList& url_visited_notifications() const {
    return url_visited_notifications_;
  }

  int num_urls_modified_notifications() const {
    return urls_modified_notifications_.size();
  }

  const URLsModifiedList& urls_modified_notifications() const {
    return urls_modified_notifications_;
  }

  const URLsDeletedList& urls_deleted_notifications() const {
    return urls_deleted_notifications_;
  }

  void ClearBroadcastedNotifications() {
    url_visited_notifications_.clear();
    urls_modified_notifications_.clear();
    urls_deleted_notifications_.clear();
    favicon_changed_notifications_page_urls_.clear();
    favicon_changed_notifications_icon_urls_.clear();
  }

  base::FilePath test_dir() { return test_dir_; }

  void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                             const GURL& icon_url) {
    favicon_changed_notifications_page_urls_.insert(
        favicon_changed_notifications_page_urls_.end(), page_urls.begin(),
        page_urls.end());
    if (!icon_url.is_empty())
      favicon_changed_notifications_icon_urls_.push_back(icon_url);
  }

  void NotifyURLVisited(const URLRow& url_row, const VisitRow& new_visit) {
    // Send the notifications directly to the in-memory database.
    mem_backend_->OnURLVisited(nullptr, url_row, new_visit);
    url_visited_notifications_.push_back(std::make_pair(url_row, new_visit));
  }

  void NotifyURLsModified(const URLRows& changed_urls) {
    // Send the notifications directly to the in-memory database.
    mem_backend_->OnURLsModified(nullptr, changed_urls);
    urls_modified_notifications_.push_back(changed_urls);
  }

  void NotifyURLsDeleted(DeletionInfo deletion_info) {
    mem_backend_->OnURLsDeleted(nullptr, deletion_info);
    urls_deleted_notifications_.push_back(std::make_pair(
        deletion_info.IsAllHistory(), deletion_info.is_from_expiration()));
  }

  void NotifyKeywordSearchTermUpdated(const URLRow& row,
                                      KeywordID keyword_id,
                                      const std::u16string& term) {
    mem_backend_->OnKeywordSearchTermUpdated(nullptr, row, keyword_id, term);
  }

  void NotifyKeywordSearchTermDeleted(URLID url_id) {
    mem_backend_->OnKeywordSearchTermDeleted(nullptr, url_id);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  HistoryClientFakeBookmarks history_client_;
  scoped_refptr<TestHistoryBackend> backend_;  // Will be NULL on init failure.
  std::unique_ptr<InMemoryHistoryBackend> mem_backend_;
  bool loaded_ = false;

 private:
  friend class HistoryBackendTestDelegate;

  // testing::Test
  void SetUp() override {
    if (!base::CreateNewTempDirectory(FILE_PATH_LITERAL("BackendTest"),
                                      &test_dir_))
      return;
    backend_ = base::MakeRefCounted<TestHistoryBackend>(
        std::make_unique<HistoryBackendTestDelegate>(this),
        history_client_.CreateBackendClient(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    backend_->Init(false, TestHistoryDatabaseParamsForPath(test_dir_));
  }

  void TearDown() override {
    if (backend_)
      backend_->Closing();
    backend_ = nullptr;
    mem_backend_.reset();
    base::DeletePathRecursively(test_dir_);
    base::RunLoop().RunUntilIdle();
    history_client_.ClearAllBookmarks();
  }

  void SetInMemoryBackend(std::unique_ptr<InMemoryHistoryBackend> backend) {
    mem_backend_.swap(backend);
  }

  // The types and details of notifications which were broadcasted.
  std::vector<GURL> favicon_changed_notifications_page_urls_;
  std::vector<GURL> favicon_changed_notifications_icon_urls_;
  URLVisitedList url_visited_notifications_;
  URLsModifiedList urls_modified_notifications_;
  URLsDeletedList urls_deleted_notifications_;

  base::FilePath test_dir_;
};

bool HistoryBackendTestDelegate::CanAddURL(const GURL& url) const {
  // For the purposes of these tests, accept all valid URLs except "chrome://".
  return url.is_valid() && !url.SchemeIs("chrome");
}

void HistoryBackendTestDelegate::SetInMemoryBackend(
    std::unique_ptr<InMemoryHistoryBackend> backend) {
  test_->SetInMemoryBackend(std::move(backend));
}

void HistoryBackendTestDelegate::NotifyFaviconsChanged(
    const std::set<GURL>& page_urls,
    const GURL& icon_url) {
  test_->NotifyFaviconsChanged(page_urls, icon_url);
}

void HistoryBackendTestDelegate::NotifyURLVisited(const URLRow& url_row,
                                                  const VisitRow& new_visit) {
  test_->NotifyURLVisited(url_row, new_visit);
}

void HistoryBackendTestDelegate::NotifyURLsModified(
    const URLRows& changed_urls) {
  test_->NotifyURLsModified(changed_urls);
}

void HistoryBackendTestDelegate::NotifyURLsDeleted(DeletionInfo deletion_info) {
  test_->NotifyURLsDeleted(std::move(deletion_info));
}

void HistoryBackendTestDelegate::NotifyKeywordSearchTermUpdated(
    const URLRow& row,
    KeywordID keyword_id,
    const std::u16string& term) {
  test_->NotifyKeywordSearchTermUpdated(row, keyword_id, term);
}

void HistoryBackendTestDelegate::NotifyKeywordSearchTermDeleted(URLID url_id) {
  test_->NotifyKeywordSearchTermDeleted(url_id);
}

void HistoryBackendTestDelegate::DBLoaded() {
  test_->loaded_ = true;
}

class HistoryBackendTest : public HistoryBackendTestBase {
 public:
  HistoryBackendTest() = default;
  HistoryBackendTest(const HistoryBackendTest&) = delete;
  HistoryBackendTest& operator=(const HistoryBackendTest&) = delete;
  ~HistoryBackendTest() override = default;

 protected:
  favicon::FaviconDatabase* favicon_db() {
    return backend_->favicon_backend_ ? backend_->favicon_backend_->db()
                                      : nullptr;
  }

  void AddRedirectChain(const char* sequence[], int nav_entry_id) {
    AddRedirectChainWithTransitionAndTime(
        sequence, nav_entry_id, ui::PAGE_TRANSITION_LINK, base::Time::Now());
  }

  void AddRedirectChainWithTransitionAndTime(const char* sequence[],
                                             int nav_entry_id,
                                             ui::PageTransition transition,
                                             base::Time time) {
    RedirectList redirects;
    for (int i = 0; sequence[i] != nullptr; ++i)
      redirects.push_back(GURL(sequence[i]));

    ContextID context_id = 1;
    HistoryAddPageArgs request(redirects.back(), time, context_id, nav_entry_id,
                               GURL(), redirects, transition, false,
                               SOURCE_BROWSED, true, true);
    backend_->AddPage(request);
  }

  // Adds CLIENT_REDIRECT page transition.
  // `url1` is the source URL and `url2` is the destination.
  // `did_replace` is true if the transition is non-user initiated and the
  // navigation entry for `url2` has replaced that for `url1`. The possibly
  // updated transition code of the visit records for `url1` and `url2` is
  // returned by filling in `*transition1` and `*transition2`, respectively,
  // unless null. `time` is a time of the redirect.
  void AddClientRedirect(const GURL& url1,
                         const GURL& url2,
                         bool did_replace,
                         base::Time time,
                         int* transition1,
                         int* transition2) {
    ContextID dummy_context_id = 0x87654321;
    RedirectList redirects;
    if (url1.is_valid())
      redirects.push_back(url1);
    if (url2.is_valid())
      redirects.push_back(url2);
    HistoryAddPageArgs request(url2, time, dummy_context_id, 0, url1, redirects,
                               ui::PAGE_TRANSITION_CLIENT_REDIRECT, false,
                               SOURCE_BROWSED, did_replace, true);
    backend_->AddPage(request);

    if (transition1)
      *transition1 = GetTransition(url1);

    if (transition2)
      *transition2 = GetTransition(url2);
  }

  // Adds SERVER_REDIRECT page transition.
  // `url1` is the source URL and `url2` is the destination.
  // `did_replace` is true if the transition is non-user initiated and the
  // navigation entry for `url2` has replaced that for `url1`. The possibly
  // updated transition code of the visit records for `url1` and `url2` is
  // returned by filling in `*transition1` and `*transition2`, respectively,
  // unless null. `time` is a time of the redirect.
  void AddServerRedirect(const GURL& url1,
                         const GURL& url2,
                         bool did_replace,
                         base::Time time,
                         const std::u16string& page2_title,
                         int& transition1,
                         int& transition2) {
    ContextID dummy_context_id = 0x87654321;
    RedirectList redirects;
    redirects.push_back(url1);
    redirects.push_back(url2);
    ui::PageTransition redirect_transition = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_FORM_SUBMIT | ui::PAGE_TRANSITION_SERVER_REDIRECT);
    HistoryAddPageArgs request(url2, time, dummy_context_id, 0, url1, redirects,
                               redirect_transition, false, SOURCE_BROWSED,
                               did_replace, true,
                               absl::optional<std::u16string>(page2_title));
    backend_->AddPage(request);

    transition1 = GetTransition(url1);
    transition2 = GetTransition(url2);
  }

  int GetTransition(const GURL& url) {
    if (!url.is_valid())
      return 0;
    URLRow row;
    URLID id = backend_->db()->GetRowForURL(url, &row);
    VisitVector visits;
    EXPECT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
    return visits[0].transition;
  }

  // Returns a vector with the small and large edge sizes.
  const std::vector<int> GetEdgeSizesSmallAndLarge() {
    std::vector<int> sizes_small_and_large;
    sizes_small_and_large.push_back(kSmallEdgeSize);
    sizes_small_and_large.push_back(kLargeEdgeSize);
    return sizes_small_and_large;
  }

  // Returns the number of icon mappings of `icon_type` to `page_url`.
  size_t NumIconMappingsForPageURL(const GURL& page_url, IconType icon_type) {
    std::vector<IconMapping> icon_mappings;
    favicon_db()->GetIconMappingsForPageURL(page_url, {icon_type},
                                            &icon_mappings);
    return icon_mappings.size();
  }

  // Returns the icon mappings for `page_url`.
  std::vector<IconMapping> GetIconMappingsForPageURL(const GURL& page_url) {
    std::vector<IconMapping> icon_mappings;
    favicon_db()->GetIconMappingsForPageURL(page_url, &icon_mappings);
    return icon_mappings;
  }

  // Returns the favicon bitmaps for `icon_id` sorted by pixel size in
  // ascending order. Returns true if there is at least one favicon bitmap.
  bool GetSortedFaviconBitmaps(favicon_base::FaviconID icon_id,
                               std::vector<FaviconBitmap>* favicon_bitmaps) {
    if (!favicon_db()->GetFaviconBitmaps(icon_id, favicon_bitmaps))
      return false;
    std::sort(favicon_bitmaps->begin(), favicon_bitmaps->end(),
              [](const FaviconBitmap& a, const FaviconBitmap& b) {
                return a.pixel_size.GetArea() < b.pixel_size.GetArea();
              });
    return true;
  }

  // Returns true if there is exactly one favicon bitmap associated to
  // `favicon_id`. If true, returns favicon bitmap in output parameter.
  bool GetOnlyFaviconBitmap(const favicon_base::FaviconID icon_id,
                            FaviconBitmap* favicon_bitmap) {
    std::vector<FaviconBitmap> favicon_bitmaps;
    if (!favicon_db()->GetFaviconBitmaps(icon_id, &favicon_bitmaps))
      return false;
    if (favicon_bitmaps.size() != 1)
      return false;
    *favicon_bitmap = favicon_bitmaps[0];
    return true;
  }

  // Creates an `edge_size`x`edge_size` bitmap of `color`.
  SkBitmap CreateBitmap(SkColor color, int edge_size) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(edge_size, edge_size);
    bitmap.eraseColor(color);
    return bitmap;
  }

  // Returns true if `bitmap_data` is equal to `expected_data`.
  bool BitmapDataEqual(char expected_data,
                       scoped_refptr<base::RefCountedMemory> bitmap_data) {
    return bitmap_data.get() && bitmap_data->size() == 1u &&
           *bitmap_data->front() == expected_data;
  }

  // Helper to add visit, URL, and context annotation entries to the
  // corresponding databases.
  void AddAnnotatedVisit(int relative_seconds) {
    const auto ids = backend_->AddPageVisit(
        GURL("https://google.com/" + base::NumberToString(relative_seconds)),
        GetRelativeTime(relative_seconds), 0,
        ui::PageTransition::PAGE_TRANSITION_FIRST, false, SOURCE_BROWSED, false,
        false);
    backend_->AddContextAnnotationsForVisit(ids.second, {});
  }

  // Helper to add a cluster.
  void AddCluster(const std::vector<int64_t>& visit_ids) {
    backend_->db_->AddClusters({CreateCluster(visit_ids)});
  }

  // Verifies a cluster has the expected ID and visit IDs.
  void VerifyCluster(const Cluster& actual,
                     const ClusterExpectation& expected) {
    EXPECT_EQ(actual.cluster_id, expected.cluster_id);
    EXPECT_EQ(GetVisitIds(actual.visits), expected.visit_ids);
  }

  // Verifies clusters have the expected IDs and visit IDs.
  void VerifyClusters(const std::vector<Cluster>& actual,
                      const std::vector<ClusterExpectation>& expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (size_t i = 0; i < actual.size(); i++) {
      SCOPED_TRACE(i);
      VerifyCluster(actual[i], expected[i]);
    }
  }

  // Helper to get a consistent time; i.e. given the same `relative_seconds`,
  // will return the same `Time`.
  base::Time GetRelativeTime(int relative_seconds) {
    static const base::Time time_now = base::Time::Now();
    return time_now + base::Seconds(relative_seconds);
  }
};

class InMemoryHistoryBackendTest : public HistoryBackendTestBase {
 public:
  InMemoryHistoryBackendTest() = default;
  InMemoryHistoryBackendTest(const InMemoryHistoryBackendTest&) = delete;
  InMemoryHistoryBackendTest& operator=(const InMemoryHistoryBackendTest&) =
      delete;
  ~InMemoryHistoryBackendTest() override = default;

 protected:
  void SimulateNotificationURLsDeleted(const URLRow* row1,
                                       const URLRow* row2 = nullptr,
                                       const URLRow* row3 = nullptr) {
    URLRows rows;
    rows.push_back(*row1);
    if (row2) rows.push_back(*row2);
    if (row3) rows.push_back(*row3);

    NotifyURLsDeleted(DeletionInfo::ForUrls(rows, std::set<GURL>()));
  }

  size_t GetNumberOfMatchingSearchTerms(const int keyword_id,
                                        const std::u16string& prefix) {
    KeywordSearchTermVisitList matching_terms;
    mem_backend_->db()->GetMostRecentKeywordSearchTerms(
        keyword_id, prefix, 1, &matching_terms);
    return matching_terms.size();
  }

  static URLRow CreateTestTypedURL() {
    URLRow url_row(GURL("https://www.google.com/"));
    url_row.set_id(10);
    url_row.set_title(u"Google Search");
    url_row.set_typed_count(1);
    url_row.set_visit_count(1);
    url_row.set_last_visit(base::Time::Now() - base::Hours(1));
    return url_row;
  }

  static URLRow CreateAnotherTestTypedURL() {
    URLRow url_row(GURL("https://maps.google.com/"));
    url_row.set_id(20);
    url_row.set_title(u"Google Maps");
    url_row.set_typed_count(2);
    url_row.set_visit_count(3);
    url_row.set_last_visit(base::Time::Now() - base::Hours(2));
    return url_row;
  }

  static URLRow CreateTestNonTypedURL() {
    URLRow url_row(GURL("https://news.google.com/"));
    url_row.set_id(30);
    url_row.set_title(u"Google News");
    url_row.set_visit_count(5);
    url_row.set_last_visit(base::Time::Now() - base::Hours(3));
    return url_row;
  }

  void PopulateTestURLsAndSearchTerms(URLRow* row1,
                                      URLRow* row2,
                                      const std::u16string& term1,
                                      const std::u16string& term2);

  void TestAddingAndChangingURLRows(
      const SimulateNotificationCallback& callback);

  static const KeywordID kTestKeywordId;
  static const char16_t kTestSearchTerm1[];
  static const char16_t kTestSearchTerm2[];
};

const KeywordID InMemoryHistoryBackendTest::kTestKeywordId = 42;
const char16_t InMemoryHistoryBackendTest::kTestSearchTerm1[] = u"banana";
const char16_t InMemoryHistoryBackendTest::kTestSearchTerm2[] = u"orange";

// http://crbug.com/114287
#if BUILDFLAG(IS_WIN)
#define MAYBE_Loaded DISABLED_Loaded
#else
#define MAYBE_Loaded Loaded
#endif  // BUILDFLAG(IS_WIN)
TEST_F(HistoryBackendTest, MAYBE_Loaded) {
  ASSERT_TRUE(backend_.get());
  ASSERT_TRUE(loaded_);
}

TEST_F(HistoryBackendTest, DeleteAll) {
  ASSERT_TRUE(backend_.get());

  // Add two favicons, each with two bitmaps. Note that we add favicon2 before
  // adding favicon1. This is so that favicon1 one gets ID 2 autoassigned to
  // the database, which will change when the other one is deleted. This way
  // we can test that updating works properly.
  GURL favicon_url1("http://www.google.com/favicon.ico");
  GURL favicon_url2("http://news.google.com/favicon.ico");
  favicon_base::FaviconID favicon2 =
      favicon_db()->AddFavicon(favicon_url2, IconType::kFavicon);
  favicon_base::FaviconID favicon1 =
      favicon_db()->AddFavicon(favicon_url1, IconType::kFavicon);

  std::vector<unsigned char> data;
  data.push_back('a');
  EXPECT_TRUE(favicon_db()->AddFaviconBitmap(
      favicon1, new base::RefCountedBytes(data), FaviconBitmapType::ON_VISIT,
      base::Time::Now(), kSmallSize));
  data[0] = 'b';
  EXPECT_TRUE(favicon_db()->AddFaviconBitmap(
      favicon1, new base::RefCountedBytes(data), FaviconBitmapType::ON_VISIT,
      base::Time::Now(), kLargeSize));

  data[0] = 'c';
  EXPECT_TRUE(favicon_db()->AddFaviconBitmap(
      favicon2, new base::RefCountedBytes(data), FaviconBitmapType::ON_VISIT,
      base::Time::Now(), kSmallSize));
  data[0] = 'd';
  EXPECT_TRUE(favicon_db()->AddFaviconBitmap(
      favicon2, new base::RefCountedBytes(data), FaviconBitmapType::ON_VISIT,
      base::Time::Now(), kLargeSize));

  // First visit two URLs.
  URLRow row1(GURL("http://www.google.com/"));
  row1.set_visit_count(2);
  row1.set_typed_count(1);
  row1.set_last_visit(base::Time::Now());
  favicon_db()->AddIconMapping(row1.url(), favicon1);

  URLRow row2(GURL("http://news.google.com/"));
  row2.set_visit_count(1);
  row2.set_last_visit(base::Time::Now());
  favicon_db()->AddIconMapping(row2.url(), favicon2);

  URLRows rows;
  rows.push_back(row2);  // Reversed order for the same reason as favicons.
  rows.push_back(row1);
  backend_->AddPagesWithDetails(rows, SOURCE_BROWSED);

  URLID row1_id = backend_->db_->GetRowForURL(row1.url(), nullptr);
  URLID row2_id = backend_->db_->GetRowForURL(row2.url(), nullptr);

  // Get the two visits for the URLs we just added.
  VisitVector visits;
  backend_->db_->GetVisitsForURL(row1_id, &visits);
  ASSERT_EQ(1U, visits.size());

  visits.clear();
  backend_->db_->GetVisitsForURL(row2_id, &visits);
  ASSERT_EQ(1U, visits.size());

  // The in-memory backend should have been set and it should have gotten the
  // typed URL.
  ASSERT_TRUE(mem_backend_.get());
  URLRow outrow1;
  EXPECT_TRUE(mem_backend_->db_->GetRowForURL(row1.url(), nullptr));

  // Star row1.
  history_client_.AddBookmark(row1.url());

  // Now finally clear all history.
  ClearBroadcastedNotifications();
  backend_->DeleteAllHistory();

  // The first URL should be preserved but the time should be cleared.
  EXPECT_TRUE(backend_->db_->GetRowForURL(row1.url(), &outrow1));
  EXPECT_EQ(row1.url(), outrow1.url());
  EXPECT_EQ(0, outrow1.visit_count());
  EXPECT_EQ(0, outrow1.typed_count());
  EXPECT_TRUE(base::Time() == outrow1.last_visit());

  // The second row should be deleted.
  URLRow outrow2;
  EXPECT_FALSE(backend_->db_->GetRowForURL(row2.url(), &outrow2));

  // All visits should be deleted for both URLs.
  VisitVector all_visits;
  backend_->db_->GetAllVisitsInRange(base::Time(), base::Time(), 0,
                                     &all_visits);
  ASSERT_EQ(0U, all_visits.size());

  // We should have a favicon and favicon bitmaps for the first URL only. We
  // look them up by favicon URL since the IDs may have changed.
  favicon_base::FaviconID out_favicon1 =
      favicon_db()->GetFaviconIDForFaviconURL(favicon_url1, IconType::kFavicon);
  EXPECT_TRUE(out_favicon1);

  std::vector<FaviconBitmap> favicon_bitmaps;
  EXPECT_TRUE(favicon_db()->GetFaviconBitmaps(out_favicon1, &favicon_bitmaps));
  ASSERT_EQ(2u, favicon_bitmaps.size());

  FaviconBitmap favicon_bitmap1 = favicon_bitmaps[0];
  FaviconBitmap favicon_bitmap2 = favicon_bitmaps[1];

  // Favicon bitmaps do not need to be in particular order.
  if (favicon_bitmap1.pixel_size == kLargeSize) {
    FaviconBitmap tmp_favicon_bitmap = favicon_bitmap1;
    favicon_bitmap1 = favicon_bitmap2;
    favicon_bitmap2 = tmp_favicon_bitmap;
  }

  EXPECT_TRUE(BitmapDataEqual('a', favicon_bitmap1.bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmap1.pixel_size);

  EXPECT_TRUE(BitmapDataEqual('b', favicon_bitmap2.bitmap_data));
  EXPECT_EQ(kLargeSize, favicon_bitmap2.pixel_size);

  favicon_base::FaviconID out_favicon2 =
      favicon_db()->GetFaviconIDForFaviconURL(favicon_url2, IconType::kFavicon);
  EXPECT_FALSE(out_favicon2) << "Favicon not deleted";

  // The remaining URL should still reference the same favicon, even if its
  // ID has changed.
  std::vector<IconMapping> mappings;
  EXPECT_TRUE(favicon_db()->GetIconMappingsForPageURL(
      outrow1.url(), {IconType::kFavicon}, &mappings));
  EXPECT_EQ(1u, mappings.size());
  EXPECT_EQ(out_favicon1, mappings[0].icon_id);

  // The first URL should still be bookmarked.
  EXPECT_TRUE(history_client_.IsBookmarked(row1.url()));

  // Check that we fire the notification about all history having been deleted.
  ASSERT_EQ(1u, urls_deleted_notifications().size());
  EXPECT_TRUE(urls_deleted_notifications()[0].first);
  EXPECT_FALSE(urls_deleted_notifications()[0].second);
}

// Test that clearing all history does not delete bookmark favicons in the
// special case that the bookmark page URL is no longer present in the History
// database's urls table.
TEST_F(HistoryBackendTest, DeleteAllURLPreviouslyDeleted) {
  ASSERT_TRUE(backend_.get());

  GURL kPageURL("http://www.google.com");
  GURL kFaviconURL("http://www.google.com/favicon.ico");

  // Setup: Add visit for `kPageURL`.
  URLRow row(kPageURL);
  row.set_visit_count(2);
  row.set_typed_count(1);
  row.set_last_visit(base::Time::Now());
  backend_->AddPagesWithDetails(std::vector<URLRow>(1u, row), SOURCE_BROWSED);

  // Setup: Add favicon for `kPageURL`.
  std::vector<unsigned char> data;
  data.push_back('a');
  favicon_base::FaviconID favicon = favicon_db()->AddFavicon(
      kFaviconURL, IconType::kFavicon, new base::RefCountedBytes(data),
      FaviconBitmapType::ON_VISIT, base::Time::Now(), kSmallSize);
  favicon_db()->AddIconMapping(row.url(), favicon);

  history_client_.AddBookmark(kPageURL);

  // Test initial state.
  URLID row_id = backend_->db_->GetRowForURL(kPageURL, nullptr);
  ASSERT_NE(0, row_id);
  VisitVector visits;
  backend_->db_->GetVisitsForURL(row_id, &visits);
  ASSERT_EQ(1U, visits.size());

  std::vector<IconMapping> icon_mappings;
  ASSERT_TRUE(favicon_db()->GetIconMappingsForPageURL(
      kPageURL, {IconType::kFavicon}, &icon_mappings));
  ASSERT_EQ(1u, icon_mappings.size());

  // Delete information for `kPageURL`, then clear all browsing data.
  backend_->DeleteURL(kPageURL);
  backend_->DeleteAllHistory();

  // Test that the entry in the url table for the bookmark is gone but that the
  // favicon data for the bookmark is still there.
  ASSERT_EQ(0, backend_->db_->GetRowForURL(kPageURL, nullptr));

  icon_mappings.clear();
  EXPECT_TRUE(favicon_db()->GetIconMappingsForPageURL(
      kPageURL, {IconType::kFavicon}, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
}

// Checks that adding a visit, then calling DeleteAll, and then trying to add
// data for the visited page works.  This can happen when clearing the history
// immediately after visiting a page.
TEST_F(HistoryBackendTest, DeleteAllThenAddData) {
  ASSERT_TRUE(backend_.get());

  base::Time visit_time = base::Time::Now();
  GURL url("http://www.google.com/");
  HistoryAddPageArgs request(url, visit_time, 0, 0, GURL(), RedirectList(),
                             ui::PAGE_TRANSITION_KEYWORD_GENERATED, false,
                             SOURCE_BROWSED, false, true);
  backend_->AddPage(request);

  // Check that a row was added.
  URLRow outrow;
  EXPECT_TRUE(backend_->db_->GetRowForURL(url, &outrow));

  // Check that the visit was added.
  VisitVector all_visits;
  backend_->db_->GetAllVisitsInRange(base::Time(), base::Time(), 0,
                                     &all_visits);
  ASSERT_EQ(1U, all_visits.size());

  // Clear all history.
  backend_->DeleteAllHistory();

  // The row should be deleted.
  EXPECT_FALSE(backend_->db_->GetRowForURL(url, &outrow));

  // The visit should be deleted.
  backend_->db_->GetAllVisitsInRange(base::Time(), base::Time(), 0,
                                     &all_visits);
  ASSERT_EQ(0U, all_visits.size());

  // Try and set the title.
  backend_->SetPageTitle(url, u"Title");

  // The row should still be deleted.
  EXPECT_FALSE(backend_->db_->GetRowForURL(url, &outrow));

  // The visit should still be deleted.
  backend_->db_->GetAllVisitsInRange(base::Time(), base::Time(), 0,
                                     &all_visits);
  ASSERT_EQ(0U, all_visits.size());
}

TEST_F(HistoryBackendTest, URLsNoLongerBookmarked) {
  GURL favicon_url1("http://www.google.com/favicon.ico");
  GURL favicon_url2("http://news.google.com/favicon.ico");

  std::vector<unsigned char> data;
  data.push_back('1');
  favicon_base::FaviconID favicon1 = favicon_db()->AddFavicon(
      favicon_url1, IconType::kFavicon, new base::RefCountedBytes(data),
      FaviconBitmapType::ON_VISIT, base::Time::Now(), gfx::Size());

  data[0] = '2';
  favicon_base::FaviconID favicon2 = favicon_db()->AddFavicon(
      favicon_url2, IconType::kFavicon, new base::RefCountedBytes(data),
      FaviconBitmapType::ON_VISIT, base::Time::Now(), gfx::Size());

  // First visit two URLs.
  URLRow row1(GURL("http://www.google.com/"));
  row1.set_visit_count(2);
  row1.set_typed_count(1);
  row1.set_last_visit(base::Time::Now());
  EXPECT_TRUE(favicon_db()->AddIconMapping(row1.url(), favicon1));

  URLRow row2(GURL("http://news.google.com/"));
  row2.set_visit_count(1);
  row2.set_last_visit(base::Time::Now());
  EXPECT_TRUE(favicon_db()->AddIconMapping(row2.url(), favicon2));

  URLRows rows;
  rows.push_back(row2);  // Reversed order for the same reason as favicons.
  rows.push_back(row1);
  backend_->AddPagesWithDetails(rows, SOURCE_BROWSED);

  URLID row1_id = backend_->db_->GetRowForURL(row1.url(), nullptr);
  URLID row2_id = backend_->db_->GetRowForURL(row2.url(), nullptr);

  // Star the two URLs.
  history_client_.AddBookmark(row1.url());
  history_client_.AddBookmark(row2.url());

  // Delete url 2.
  backend_->expirer_.DeleteURL(row2.url(), base::Time::Max());
  EXPECT_FALSE(backend_->db_->GetRowForURL(row2.url(), nullptr));
  VisitVector visits;
  backend_->db_->GetVisitsForURL(row2_id, &visits);
  EXPECT_EQ(0U, visits.size());
  // The favicon should still be valid.
  EXPECT_EQ(favicon2, favicon_db()->GetFaviconIDForFaviconURL(
                          favicon_url2, IconType::kFavicon));

  // Unstar row2.
  history_client_.DelBookmark(row2.url());

  // Tell the backend it was unstarred. We have to explicitly do this as
  // BookmarkModel isn't wired up to the backend during testing.
  std::set<GURL> unstarred_urls;
  unstarred_urls.insert(row2.url());
  backend_->URLsNoLongerBookmarked(unstarred_urls);

  // The URL should still not exist.
  EXPECT_FALSE(backend_->db_->GetRowForURL(row2.url(), nullptr));
  // And the favicon should be deleted.
  EXPECT_EQ(0, favicon_db()->GetFaviconIDForFaviconURL(favicon_url2,
                                                       IconType::kFavicon));

  // Unstar row 1.
  history_client_.DelBookmark(row1.url());

  // Tell the backend it was unstarred. We have to explicitly do this as
  // BookmarkModel isn't wired up to the backend during testing.
  unstarred_urls.clear();
  unstarred_urls.insert(row1.url());
  backend_->URLsNoLongerBookmarked(unstarred_urls);

  // The URL should still exist (because there were visits).
  EXPECT_EQ(row1_id, backend_->db_->GetRowForURL(row1.url(), nullptr));

  // There should still be visits.
  visits.clear();
  backend_->db_->GetVisitsForURL(row1_id, &visits);
  EXPECT_EQ(1U, visits.size());

  // The favicon should still be valid.
  EXPECT_EQ(favicon1, favicon_db()->GetFaviconIDForFaviconURL(
                          favicon_url1, IconType::kFavicon));
}

// Tests a handful of assertions for a navigation with a type of
// KEYWORD_GENERATED.
TEST_F(HistoryBackendTest, KeywordGenerated) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://google.com");

  base::Time visit_time = base::Time::Now() - base::Days(1);
  HistoryAddPageArgs request(url, visit_time, 0, 0, GURL(), RedirectList(),
                             ui::PAGE_TRANSITION_KEYWORD_GENERATED, false,
                             SOURCE_BROWSED, false, true);
  backend_->AddPage(request);

  // A row should have been added for the url.
  URLRow row;
  URLID url_id = backend_->db()->GetRowForURL(url, &row);
  ASSERT_NE(0, url_id);

  // The typed count should be 1.
  ASSERT_EQ(1, row.typed_count());

  // KEYWORD_GENERATED urls should not be added to the segment db.
  std::string segment_name = VisitSegmentDatabase::ComputeSegmentName(url);
  EXPECT_EQ(0, backend_->db()->GetSegmentNamed(segment_name));

  // One visit should be added.
  VisitVector visits;
  EXPECT_TRUE(backend_->db()->GetVisitsForURL(url_id, &visits));
  EXPECT_EQ(1U, visits.size());

  // But no visible visits.
  visits.clear();
  QueryOptions query_options;
  query_options.max_count = 1;
  backend_->db()->GetVisibleVisitsInRange(query_options, &visits);
  EXPECT_TRUE(visits.empty());

  // Going back to the same entry should not increment the typed count.
  ui::PageTransition back_transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FORWARD_BACK);
  HistoryAddPageArgs back_request(url, visit_time, 0, 0, GURL(), RedirectList(),
                                  back_transition, false, SOURCE_BROWSED, false,
                                  true);
  backend_->AddPage(back_request);
  url_id = backend_->db()->GetRowForURL(url, &row);
  ASSERT_NE(0, url_id);
  ASSERT_EQ(1, row.typed_count());

  // Expire the visits.
  std::set<GURL> restrict_urls;
  backend_->expire_backend()->ExpireHistoryBetween(
      restrict_urls, visit_time, base::Time::Now(), /*user_initiated*/ true);

  // The visit should have been nuked.
  visits.clear();
  EXPECT_TRUE(backend_->db()->GetVisitsForURL(url_id, &visits));
  EXPECT_TRUE(visits.empty());

  // As well as the url.
  ASSERT_EQ(0, backend_->db()->GetRowForURL(url, &row));
}

TEST_F(HistoryBackendTest, OpenerWithRedirect) {
  ASSERT_TRUE(backend_.get());

  base::Time visit_time = base::Time::Now() - base::Days(1);
  GURL initial_url("http://google.com/c");
  GURL server_redirect_url("http://google.com/a");
  GURL client_redirect_url("http://google.com/b");

  ContextID context_id1 = 1;
  ContextID context_id2 = 2;

  // Add an initial page.
  int nav_entry_id = 2;
  HistoryAddPageArgs initial_request(initial_url, visit_time, context_id1,
                                     nav_entry_id, GURL(), RedirectList(),
                                     ui::PAGE_TRANSITION_TYPED, false,
                                     SOURCE_BROWSED, false, true);
  backend_->AddPage(initial_request);

  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(initial_url, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(1U, visits.size());
  VisitID initial_visit_id = visits[0].visit_id;

  // Simulate the initial URL opening a page that then redirects.
  HistoryAddPageArgs request(
      client_redirect_url, base::Time::Now() - base::Seconds(1), context_id2, 0,
      GURL(), /*redirects=*/{server_redirect_url, client_redirect_url},
      ui::PAGE_TRANSITION_TYPED, false, SOURCE_BROWSED, false, true,
      absl::nullopt, Opener(context_id1, nav_entry_id, initial_url));
  backend_->AddPage(request);

  visits.clear();
  backend_->db()->GetAllVisitsInRange(visit_time, base::Time::Now(), 5,
                                      &visits);
  // There should be 3 visits: initial visit, server redirect, and client
  // redirect.
  ASSERT_EQ(visits.size(), 3u);
  EXPECT_EQ(visits[1].opener_visit, initial_visit_id);
  // Opener should only be populated on first visit of chain.
  EXPECT_EQ(visits[2].opener_visit, 0);
}

TEST_F(HistoryBackendTest, ClientRedirect) {
  ASSERT_TRUE(backend_.get());

  int transition1;
  int transition2;

  // Initial transition to page A.
  GURL url_a("http://google.com/a");
  AddClientRedirect(GURL(), url_a, false, base::Time(),
                    &transition1, &transition2);
  EXPECT_TRUE(transition2 & ui::PAGE_TRANSITION_CHAIN_END);

  // User initiated redirect to page B.
  GURL url_b("http://google.com/b");
  AddClientRedirect(url_a, url_b, false, base::Time(),
                    &transition1, &transition2);
  EXPECT_TRUE(transition1 & ui::PAGE_TRANSITION_CHAIN_END);
  EXPECT_TRUE(transition2 & ui::PAGE_TRANSITION_CHAIN_END);

  // Non-user initiated redirect to page C.
  GURL url_c("http://google.com/c");
  AddClientRedirect(url_b, url_c, true, base::Time(),
                    &transition1, &transition2);
  EXPECT_FALSE(transition1 & ui::PAGE_TRANSITION_CHAIN_END);
  EXPECT_TRUE(transition2 & ui::PAGE_TRANSITION_CHAIN_END);
}

// Do not update original URL on form submission redirect
TEST_F(HistoryBackendTest, FormSubmitRedirect) {
  ASSERT_TRUE(backend_.get());
  const std::u16string page1_title = u"Form";
  const std::u16string page2_title = u"New Page";

  // User goes to form page.
  GURL url_a("http://www.google.com/a");
  HistoryAddPageArgs request(url_a, base::Time::Now(), 0, 0, GURL(),
                             RedirectList(), ui::PAGE_TRANSITION_TYPED, false,
                             SOURCE_BROWSED, false, true,
                             absl::optional<std::u16string>(page1_title));
  backend_->AddPage(request);

  // Check that URL was added.
  ASSERT_EQ(1, num_url_visited_notifications());
  const URLVisitedList& visited_url_list = url_visited_notifications();
  ASSERT_EQ(1u, visited_url_list.size());
  const URLRow& visited_url = visited_url_list[0].first;
  EXPECT_EQ(page1_title, visited_url.title());
  ClearBroadcastedNotifications();

  // User submits form and is redirected.
  int transition1;
  int transition2;
  GURL url_b("http://google.com/b");
  AddServerRedirect(url_a, url_b, false, base::Time::Now(), page2_title,
                    transition1, transition2);
  EXPECT_TRUE(transition1 & ui::PAGE_TRANSITION_CHAIN_START);
  EXPECT_TRUE(transition2 & ui::PAGE_TRANSITION_CHAIN_END);

  // Check that first URL did not change, but the second did.
  ASSERT_EQ(1, num_url_visited_notifications());
  const URLVisitedList& visited_url_list2 = url_visited_notifications();
  ASSERT_EQ(1u, visited_url_list2.size());
  const URLRow& visited_url2 = visited_url_list2[0].first;
  EXPECT_EQ(page2_title, visited_url2.title());
}

TEST_F(HistoryBackendTest, AddPagesWithDetails) {
  ASSERT_TRUE(backend_.get());

  // Import one non-typed URL, and two recent and one expired typed URLs.
  URLRow row1(GURL("https://news.google.com/"));
  row1.set_visit_count(1);
  row1.set_last_visit(base::Time::Now());
  URLRow row2(GURL("https://www.google.com/"));
  row2.set_typed_count(1);
  row2.set_last_visit(base::Time::Now());
  URLRow row3(GURL("https://mail.google.com/"));
  row3.set_visit_count(1);
  row3.set_typed_count(1);
  row3.set_last_visit(base::Time::Now() - base::Days(7 - 1));
  URLRow row4(GURL("https://maps.google.com/"));
  row4.set_visit_count(1);
  row4.set_typed_count(1);
  row4.set_last_visit(base::Time::Now() - base::Days(365 + 2));

  URLRows rows;
  rows.push_back(row1);
  rows.push_back(row2);
  rows.push_back(row3);
  rows.push_back(row4);
  backend_->AddPagesWithDetails(rows, SOURCE_BROWSED);

  // Verify that recent URLs have ended up in the main `db_`, while the already
  // expired URL has been ignored.
  URLRow stored_row1, stored_row2, stored_row3, stored_row4;
  EXPECT_NE(0, backend_->db_->GetRowForURL(row1.url(), &stored_row1));
  EXPECT_NE(0, backend_->db_->GetRowForURL(row2.url(), &stored_row2));
  EXPECT_NE(0, backend_->db_->GetRowForURL(row3.url(), &stored_row3));
  EXPECT_EQ(0, backend_->db_->GetRowForURL(row4.url(), &stored_row4));

  // Ensure that a notification was fired for both typed and non-typed URLs.
  // Further verify that the IDs in the notification are set to those that are
  // in effect in the main database. The InMemoryHistoryBackend relies on this
  // for caching.
  ASSERT_EQ(1, num_urls_modified_notifications());

  const URLRows& changed_urls = urls_modified_notifications()[0];
  EXPECT_EQ(3u, changed_urls.size());

  auto it_row1 =
      base::ranges::find_if(changed_urls, URLRow::URLRowHasURL(row1.url()));
  ASSERT_NE(changed_urls.end(), it_row1);
  EXPECT_EQ(stored_row1.id(), it_row1->id());

  auto it_row2 =
      base::ranges::find_if(changed_urls, URLRow::URLRowHasURL(row2.url()));
  ASSERT_NE(changed_urls.end(), it_row2);
  EXPECT_EQ(stored_row2.id(), it_row2->id());

  auto it_row3 =
      base::ranges::find_if(changed_urls, URLRow::URLRowHasURL(row3.url()));
  ASSERT_NE(changed_urls.end(), it_row3);
  EXPECT_EQ(stored_row3.id(), it_row3->id());
}

TEST_F(HistoryBackendTest, UpdateURLs) {
  ASSERT_TRUE(backend_.get());

  // Add three pages directly to the database.
  URLRow row1(GURL("https://news.google.com/"));
  row1.set_visit_count(1);
  row1.set_last_visit(base::Time::Now());
  URLRow row2(GURL("https://maps.google.com/"));
  row2.set_visit_count(2);
  row2.set_last_visit(base::Time::Now());
  URLRow row3(GURL("https://www.google.com/"));
  row3.set_visit_count(3);
  row3.set_last_visit(base::Time::Now());

  backend_->db_->AddURL(row1);
  backend_->db_->AddURL(row2);
  backend_->db_->AddURL(row3);

  // Now create changed versions of all URLRows by incrementing their visit
  // counts, and in the meantime, also delete the second row from the database.
  URLRow altered_row1, altered_row2, altered_row3;
  backend_->db_->GetRowForURL(row1.url(), &altered_row1);
  altered_row1.set_visit_count(42);
  backend_->db_->GetRowForURL(row2.url(), &altered_row2);
  altered_row2.set_visit_count(43);
  backend_->db_->GetRowForURL(row3.url(), &altered_row3);
  altered_row3.set_visit_count(44);

  backend_->db_->DeleteURLRow(altered_row2.id());

  // Now try to update all three rows at once. The change to the second URLRow
  // should be ignored, as it is no longer present in the DB.
  URLRows rows;
  rows.push_back(altered_row1);
  rows.push_back(altered_row2);
  rows.push_back(altered_row3);
  EXPECT_EQ(2u, backend_->UpdateURLs(rows));

  URLRow stored_row1, stored_row3;
  EXPECT_NE(0, backend_->db_->GetRowForURL(row1.url(), &stored_row1));
  EXPECT_NE(0, backend_->db_->GetRowForURL(row3.url(), &stored_row3));
  EXPECT_EQ(altered_row1.visit_count(), stored_row1.visit_count());
  EXPECT_EQ(altered_row3.visit_count(), stored_row3.visit_count());

  // Ensure that a notification was fired, and further verify that the IDs in
  // the notification are set to those that are in effect in the main database.
  // The InMemoryHistoryBackend relies on this for caching.
  ASSERT_EQ(1, num_urls_modified_notifications());

  const URLRows& changed_urls = urls_modified_notifications()[0];
  EXPECT_EQ(2u, changed_urls.size());

  auto it_row1 =
      base::ranges::find_if(changed_urls, URLRow::URLRowHasURL(row1.url()));
  ASSERT_NE(changed_urls.end(), it_row1);
  EXPECT_EQ(altered_row1.id(), it_row1->id());
  EXPECT_EQ(altered_row1.visit_count(), it_row1->visit_count());

  auto it_row3 =
      base::ranges::find_if(changed_urls, URLRow::URLRowHasURL(row3.url()));
  ASSERT_NE(changed_urls.end(), it_row3);
  EXPECT_EQ(altered_row3.id(), it_row3->id());
  EXPECT_EQ(altered_row3.visit_count(), it_row3->visit_count());
}

// This verifies that a notification is fired. In-depth testing of logic should
// be done in HistoryTest.SetTitle.
TEST_F(HistoryBackendTest, SetPageTitleFiresNotificationWithCorrectDetails) {
  const char16_t kTestUrlTitle[] = u"Google Search";

  ASSERT_TRUE(backend_.get());

  // Add two pages, then change the title of the second one.
  URLRow row1(GURL("https://news.google.com/"));
  row1.set_typed_count(1);
  row1.set_last_visit(base::Time::Now());
  URLRow row2(GURL("https://www.google.com/"));
  row2.set_visit_count(2);
  row2.set_last_visit(base::Time::Now());

  URLRows rows;
  rows.push_back(row1);
  rows.push_back(row2);
  backend_->AddPagesWithDetails(rows, SOURCE_BROWSED);

  ClearBroadcastedNotifications();
  backend_->SetPageTitle(row2.url(), kTestUrlTitle);

  // Ensure that a notification was fired, and further verify that the IDs in
  // the notification are set to those that are in effect in the main database.
  // The InMemoryHistoryBackend relies on this for caching.
  URLRow stored_row2;
  EXPECT_TRUE(backend_->GetURL(row2.url(), &stored_row2));
  ASSERT_EQ(1, num_urls_modified_notifications());

  const URLRows& changed_urls = urls_modified_notifications()[0];
  ASSERT_EQ(1u, changed_urls.size());
  EXPECT_EQ(kTestUrlTitle, changed_urls[0].title());
  EXPECT_EQ(stored_row2.id(), changed_urls[0].id());
}

// There's no importer on Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(HistoryBackendTest, ImportedFaviconsTest) {
  // Setup test data - two Urls in the history, one with favicon assigned and
  // one without.
  GURL favicon_url1("http://www.google.com/favicon.ico");
  std::vector<unsigned char> data;
  data.push_back('1');
  favicon_base::FaviconID favicon1 = favicon_db()->AddFavicon(
      favicon_url1, IconType::kFavicon,
      base::RefCountedBytes::TakeVector(&data), FaviconBitmapType::ON_VISIT,
      base::Time::Now(), gfx::Size());
  URLRow row1(GURL("http://www.google.com/"));
  row1.set_visit_count(1);
  row1.set_last_visit(base::Time::Now());
  EXPECT_TRUE(favicon_db()->AddIconMapping(row1.url(), favicon1));

  URLRow row2(GURL("http://news.google.com/"));
  row2.set_visit_count(1);
  row2.set_last_visit(base::Time::Now());
  URLRows rows;
  rows.push_back(row1);
  rows.push_back(row2);
  backend_->AddPagesWithDetails(rows, SOURCE_BROWSED);
  URLRow url_row1, url_row2;
  EXPECT_NE(0, backend_->db_->GetRowForURL(row1.url(), &url_row1));
  EXPECT_NE(0, backend_->db_->GetRowForURL(row2.url(), &url_row2));
  EXPECT_EQ(1u, NumIconMappingsForPageURL(row1.url(), IconType::kFavicon));
  EXPECT_EQ(0u, NumIconMappingsForPageURL(row2.url(), IconType::kFavicon));

  // Now provide one imported favicon for both URLs already in the registry.
  // The new favicon should only be used with the URL that doesn't already have
  // a favicon.
  favicon_base::FaviconUsageDataList favicons;
  favicon_base::FaviconUsageData favicon;
  favicon.favicon_url = GURL("http://news.google.com/favicon.ico");
  favicon.png_data.push_back('2');
  favicon.urls.insert(row1.url());
  favicon.urls.insert(row2.url());
  favicons.push_back(favicon);
  backend_->SetImportedFavicons(favicons);
  EXPECT_NE(0, backend_->db_->GetRowForURL(row1.url(), &url_row1));
  EXPECT_NE(0, backend_->db_->GetRowForURL(row2.url(), &url_row2));

  std::vector<IconMapping> mappings;
  EXPECT_TRUE(favicon_db()->GetIconMappingsForPageURL(
      row1.url(), {IconType::kFavicon}, &mappings));
  EXPECT_EQ(1u, mappings.size());
  EXPECT_EQ(favicon1, mappings[0].icon_id);
  EXPECT_EQ(favicon_url1, mappings[0].icon_url);

  mappings.clear();
  EXPECT_TRUE(favicon_db()->GetIconMappingsForPageURL(
      row2.url(), {IconType::kFavicon}, &mappings));
  EXPECT_EQ(1u, mappings.size());
  EXPECT_EQ(favicon.favicon_url, mappings[0].icon_url);

  // A URL should not be added to history (to store favicon), if
  // the URL is not bookmarked.
  GURL url3("http://mail.google.com");
  favicons.clear();
  favicon.favicon_url = GURL("http://mail.google.com/favicon.ico");
  favicon.png_data.push_back('3');
  favicon.urls.insert(url3);
  favicons.push_back(favicon);
  backend_->SetImportedFavicons(favicons);
  URLRow url_row3;
  EXPECT_EQ(0, backend_->db_->GetRowForURL(url3, &url_row3));

  // If the URL is bookmarked, it should get added to history with 0 visits.
  history_client_.AddBookmark(url3);
  backend_->SetImportedFavicons(favicons);
  EXPECT_NE(0, backend_->db_->GetRowForURL(url3, &url_row3));
  EXPECT_EQ(0, url_row3.visit_count());
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(HistoryBackendTest, StripUsernamePasswordTest) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://anyuser:anypass@www.google.com");
  GURL stripped_url("http://www.google.com");

  // Clear all history.
  backend_->DeleteAllHistory();

  // Visit the url with username, password.
  backend_->AddPageVisit(url, base::Time::Now(), 0, ui::PAGE_TRANSITION_TYPED,
                         false, SOURCE_BROWSED, true, false);

  // Fetch the row information about stripped url from history db.
  VisitVector visits;
  URLID row_id = backend_->db_->GetRowForURL(stripped_url, nullptr);
  backend_->db_->GetVisitsForURL(row_id, &visits);

  // Check if stripped url is stored in database.
  ASSERT_EQ(1U, visits.size());
}

TEST_F(HistoryBackendTest, AddPageVisitBackForward) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://www.google.com");

  // Clear all history.
  backend_->DeleteAllHistory();

  // Visit the url after typing it.
  backend_->AddPageVisit(url, base::Time::Now(), 0, ui::PAGE_TRANSITION_TYPED,
                         false, SOURCE_BROWSED, true, false);

  // Ensure both the typed count and visit count are 1.
  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  EXPECT_EQ(1, row.typed_count());
  EXPECT_EQ(1, row.visit_count());

  // Visit the url again via back/forward.
  backend_->AddPageVisit(
      url, base::Time::Now(), 0,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      false, SOURCE_BROWSED, false, false);

  // Ensure the typed count is still 1 but the visit count is 2.
  id = backend_->db()->GetRowForURL(url, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  EXPECT_EQ(1, row.typed_count());
  EXPECT_EQ(2, row.visit_count());
}

TEST_F(HistoryBackendTest, AddPageVisitRedirectBackForward) {
  ASSERT_TRUE(backend_.get());

  GURL url1("http://www.google.com");
  GURL url2("http://www.chromium.org");

  // Clear all history.
  backend_->DeleteAllHistory();

  // Visit a typed URL with a redirect.
  backend_->AddPageVisit(url1, base::Time::Now(), 0, ui::PAGE_TRANSITION_TYPED,
                         false, SOURCE_BROWSED, true, false);
  backend_->AddPageVisit(
      url2, base::Time::Now(), 0,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      false, SOURCE_BROWSED, false, false);

  // Ensure the redirected URL does not count as typed.
  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url2, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  EXPECT_EQ(0, row.typed_count());
  EXPECT_EQ(1, row.visit_count());

  // Visit the redirected url again via back/forward.
  backend_->AddPageVisit(
      url2, base::Time::Now(), 0,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FORWARD_BACK |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      false, SOURCE_BROWSED, false, false);

  // Ensure the typed count is still 1 but the visit count is 2.
  id = backend_->db()->GetRowForURL(url2, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  EXPECT_EQ(0, row.typed_count());
  EXPECT_EQ(2, row.visit_count());
}

TEST_F(HistoryBackendTest, AddPageVisitSource) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://www.google.com");

  // Clear all history.
  backend_->DeleteAllHistory();

  // Assume visiting the url from an extension.
  backend_->AddPageVisit(url, base::Time::Now(), 0, ui::PAGE_TRANSITION_TYPED,
                         false, SOURCE_EXTENSION, true, false);
  // Assume the url is imported from Firefox.
  backend_->AddPageVisit(url, base::Time::Now(), 0, ui::PAGE_TRANSITION_TYPED,
                         false, SOURCE_FIREFOX_IMPORTED, true, false);
  // Assume this url is also synced.
  backend_->AddPageVisit(url, base::Time::Now(), 0, ui::PAGE_TRANSITION_TYPED,
                         false, SOURCE_SYNCED, true, false);

  // Fetch the row information about the url from history db.
  VisitVector visits;
  URLID row_id = backend_->db_->GetRowForURL(url, nullptr);
  backend_->db_->GetVisitsForURL(row_id, &visits);

  // Check if all the visits to the url are stored in database.
  ASSERT_EQ(3U, visits.size());
  VisitSourceMap visit_sources;
  ASSERT_TRUE(backend_->GetVisitsSource(visits, &visit_sources));
  ASSERT_EQ(3U, visit_sources.size());
  int sources = 0;
  for (int i = 0; i < 3; i++) {
    switch (visit_sources[visits[i].visit_id]) {
      case SOURCE_EXTENSION:
        sources |= 0x1;
        break;
      case SOURCE_FIREFOX_IMPORTED:
        sources |= 0x2;
        break;
      case SOURCE_SYNCED:
        sources |= 0x4;
        break;
      default:
        break;
    }
  }
  EXPECT_EQ(0x7, sources);
}

TEST_F(HistoryBackendTest, AddPageVisitNotLastVisit) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://www.google.com");

  // Clear all history.
  backend_->DeleteAllHistory();

  // Create visit times
  base::Time recent_time = base::Time::Now();
  base::TimeDelta visit_age = base::Days(3);
  base::Time older_time = recent_time - visit_age;

  // Visit the url with recent time.
  backend_->AddPageVisit(url, recent_time, 0, ui::PAGE_TRANSITION_TYPED, false,
                         SOURCE_BROWSED, true, false);

  // Add to the url a visit with older time (could be syncing from another
  // client, etc.).
  backend_->AddPageVisit(url, older_time, 0, ui::PAGE_TRANSITION_TYPED, false,
                         SOURCE_SYNCED, true, false);

  // Fetch the row information about url from history db.
  VisitVector visits;
  URLRow row;
  URLID row_id = backend_->db_->GetRowForURL(url, &row);
  backend_->db_->GetVisitsForURL(row_id, &visits);

  // Last visit time should be the most recent time, not the most recently added
  // visit.
  ASSERT_EQ(2U, visits.size());
  ASSERT_EQ(recent_time, row.last_visit());
}

TEST_F(HistoryBackendTest, AddPageVisitFiresNotificationWithCorrectDetails) {
  ASSERT_TRUE(backend_.get());

  GURL url1("http://www.google.com");
  GURL url2("http://maps.google.com");

  // Clear all history.
  backend_->DeleteAllHistory();
  ClearBroadcastedNotifications();

  // Visit two distinct URLs, the second one twice.
  backend_->AddPageVisit(url1, base::Time::Now(), 0, ui::PAGE_TRANSITION_LINK,
                         false, SOURCE_BROWSED, false, false);
  for (int i = 0; i < 2; ++i) {
    backend_->AddPageVisit(url2, base::Time::Now(), 0,
                           ui::PAGE_TRANSITION_TYPED, false, SOURCE_BROWSED,
                           true, false);
  }

  URLRow stored_row1, stored_row2;
  EXPECT_NE(0, backend_->db_->GetRowForURL(url1, &stored_row1));
  EXPECT_NE(0, backend_->db_->GetRowForURL(url2, &stored_row2));

  // Expect that HistoryServiceObserver::OnURLVisited has been called 3 times,
  // and that each time the URLRows have the correct URLs and IDs set.
  ASSERT_EQ(3, num_url_visited_notifications());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      url_visited_notifications()[0].second.transition,
      ui::PAGE_TRANSITION_LINK));
  EXPECT_EQ(stored_row1.id(), url_visited_notifications()[0].first.id());
  EXPECT_EQ(stored_row1.url(), url_visited_notifications()[0].first.url());

  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      url_visited_notifications()[1].second.transition,
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(stored_row2.id(), url_visited_notifications()[1].first.id());
  EXPECT_EQ(stored_row2.url(), url_visited_notifications()[1].first.url());

  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      url_visited_notifications()[2].second.transition,
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(stored_row2.id(), url_visited_notifications()[2].first.id());
  EXPECT_EQ(stored_row2.url(), url_visited_notifications()[2].first.url());
}

TEST_F(HistoryBackendTest, AddPageArgsSource) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://testpageargs.com");

  // Assume this page is browsed by user.
  HistoryAddPageArgs request1(url, base::Time::Now(), 0, 0, GURL(),
                              RedirectList(),
                              ui::PAGE_TRANSITION_KEYWORD_GENERATED, false,
                              SOURCE_BROWSED, false, true);
  backend_->AddPage(request1);
  // Assume this page is synced.
  HistoryAddPageArgs request2(url, base::Time::Now(), 0, 0, GURL(),
                              RedirectList(), ui::PAGE_TRANSITION_LINK, false,
                              SOURCE_SYNCED, false, true);
  backend_->AddPage(request2);
  // Assume this page is browsed again.
  HistoryAddPageArgs request3(url, base::Time::Now(), 0, 0, GURL(),
                              RedirectList(), ui::PAGE_TRANSITION_TYPED, false,
                              SOURCE_BROWSED, false, true);
  backend_->AddPage(request3);

  // Three visits should be added with proper sources.
  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(3U, visits.size());
  VisitSourceMap visit_sources;
  ASSERT_TRUE(backend_->GetVisitsSource(visits, &visit_sources));
  ASSERT_EQ(1U, visit_sources.size());
  EXPECT_EQ(SOURCE_SYNCED, visit_sources.begin()->second);
}

TEST_F(HistoryBackendTest, AddContentModelAnnotationsWithNoEntryInVisitTable) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://pagewithvisit.com");
  ContextID context_id = 1;
  int nav_entry_id = 1;

  HistoryAddPageArgs request(url, base::Time::Now(), context_id, nav_entry_id,
                             GURL(), RedirectList(), ui::PAGE_TRANSITION_TYPED,
                             false, SOURCE_BROWSED, false, true);
  backend_->AddPage(request);

  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(1U, visits.size());
  VisitID visit_id = visits[0].visit_id;

  // Delete the visit.
  backend_->DeleteURL(url);

  // Try adding the model_annotations. It should be a no-op as there's no
  // matching entry in the visits table.
  VisitContentModelAnnotations model_annotations = {
      0.5f,
      {{/*id=*/"1", /*weight=*/1}, {/*id=*/"2", /*weight=*/1}},
      123,
      {{/*id=*/"entity1", /*weight=*/1}, {/*id=*/"entity2", /*weight=*/1}}};
  backend_->AddContentModelAnnotationsForVisit(visit_id, model_annotations);

  // The content_annotations table should have no entries.
  VisitContentAnnotations got_content_annotations;
  ASSERT_FALSE(backend_->db()->GetContentAnnotationsForVisit(
      visit_id, &got_content_annotations));
}

TEST_F(HistoryBackendTest, AddRelatedSearchesWithNoEntryInVisitTable) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://pagewithvisit.com");
  ContextID context_id = 1;
  int nav_entry_id = 1;

  HistoryAddPageArgs request(url, base::Time::Now(), context_id, nav_entry_id,
                             GURL(), RedirectList(), ui::PAGE_TRANSITION_TYPED,
                             false, SOURCE_BROWSED, false, true);
  backend_->AddPage(request);

  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(1U, visits.size());
  VisitID visit_id = visits[0].visit_id;

  // Delete the visit.
  backend_->DeleteURL(url);

  // Try adding the related searches. It should be a no-op as there's no
  // matching entry in the visits table.
  backend_->AddRelatedSearchesForVisit(
      visit_id, {"related searches", "búsquedas relacionadas"});

  // The content_annotations table should have no entries.
  VisitContentAnnotations got_content_annotations;
  ASSERT_FALSE(backend_->db()->GetContentAnnotationsForVisit(
      visit_id, &got_content_annotations));
}

TEST_F(HistoryBackendTest, AddSearchMetadataWithNoEntryInVisitTable) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://pagewithvisit.com?q=search");
  ContextID context_id = 1;
  int nav_entry_id = 1;

  HistoryAddPageArgs request(url, base::Time::Now(), context_id, nav_entry_id,
                             GURL(), RedirectList(), ui::PAGE_TRANSITION_TYPED,
                             false, SOURCE_BROWSED, false, true);
  backend_->AddPage(request);

  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(1U, visits.size());
  VisitID visit_id = visits[0].visit_id;

  // Delete the visit.
  backend_->DeleteURL(url);

  // Try adding the search metadata. It should be a no-op as there's no
  // matching entry in the visits table.
  backend_->AddSearchMetadataForVisit(
      visit_id, GURL("http://pagewithvisit.com?q=search"), u"search");

  // The content_annotations table should have no entries.
  VisitContentAnnotations got_content_annotations;
  ASSERT_FALSE(backend_->db()->GetContentAnnotationsForVisit(
      visit_id, &got_content_annotations));
}

TEST_F(HistoryBackendTest, SetBrowsingTopicsAllowed) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://test-set-floc-allowed.com");
  ContextID context_id = 1;
  int nav_entry_id = 1;

  HistoryAddPageArgs request(url, base::Time::Now(), context_id, nav_entry_id,
                             GURL(), RedirectList(), ui::PAGE_TRANSITION_TYPED,
                             false, SOURCE_BROWSED, false, true);
  backend_->AddPage(request);

  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(1U, visits.size());
  VisitID visit_id = visits[0].visit_id;

  backend_->SetBrowsingTopicsAllowed(context_id, nav_entry_id, url);

  VisitContentAnnotations got_content_annotations;
  ASSERT_TRUE(backend_->db()->GetContentAnnotationsForVisit(
      visit_id, &got_content_annotations));

  EXPECT_EQ(VisitContentAnnotationFlag::kBrowsingTopicsEligible,
            got_content_annotations.annotation_flags);
  EXPECT_EQ(-1, got_content_annotations.model_annotations.visibility_score);
  EXPECT_TRUE(got_content_annotations.model_annotations.categories.empty());
  EXPECT_EQ(
      -1, got_content_annotations.model_annotations.page_topics_model_version);

  QueryOptions options;
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  QueryResults results = backend_->QueryHistory(/*text_query=*/{}, options);

  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(VisitContentAnnotationFlag::kBrowsingTopicsEligible,
            results[0].content_annotations().annotation_flags);
  EXPECT_EQ(
      -1, results[0].content_annotations().model_annotations.visibility_score);
  EXPECT_TRUE(
      results[0].content_annotations().model_annotations.categories.empty());
  EXPECT_EQ(-1, results[0]
                    .content_annotations()
                    .model_annotations.page_topics_model_version);
}

TEST_F(HistoryBackendTest, AddContentModelAnnotations) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://pagewithvisit.com");
  ContextID context_id = 1;
  int nav_entry_id = 1;

  HistoryAddPageArgs request(url, base::Time::Now(), context_id, nav_entry_id,
                             GURL(), RedirectList(), ui::PAGE_TRANSITION_TYPED,
                             false, SOURCE_BROWSED, false, true);
  backend_->AddPage(request);

  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(1U, visits.size());
  VisitID visit_id = visits[0].visit_id;

  VisitContentModelAnnotations model_annotations_without_entities = {
      0.5f, {{/*id=*/"1", /*weight=*/1}, {/*id=*/"2", /*weight=*/1}}, 123, {}};
  backend_->AddContentModelAnnotationsForVisit(
      visit_id, model_annotations_without_entities);
  VisitContentModelAnnotations model_annotations_only_entities = {
      -1.0f,
      {},
      -1,
      {{/*id=*/"entity1", /*weight=*/1}, {/*id=*/"entity2", /*weight=*/1}}};
  backend_->AddContentModelAnnotationsForVisit(visit_id,
                                               model_annotations_only_entities);

  VisitContentAnnotations got_content_annotations;
  ASSERT_TRUE(backend_->db()->GetContentAnnotationsForVisit(
      visit_id, &got_content_annotations));

  // Model annotations should be merged from both calls.
  EXPECT_EQ(VisitContentAnnotationFlag::kNone,
            got_content_annotations.annotation_flags);
  EXPECT_EQ(0.5f, got_content_annotations.model_annotations.visibility_score);
  EXPECT_THAT(
      got_content_annotations.model_annotations.categories,
      ElementsAre(
          VisitContentModelAnnotations::Category(/*id=*/"1", /*weight=*/1),
          VisitContentModelAnnotations::Category(/*id=*/"2", /*weight=*/1)));
  EXPECT_EQ(
      123, got_content_annotations.model_annotations.page_topics_model_version);
  EXPECT_THAT(got_content_annotations.model_annotations.entities,
              ElementsAre(VisitContentModelAnnotations::Category(
                              /*id=*/"entity1", /*weight=*/1),
                          VisitContentModelAnnotations::Category(
                              /*id=*/"entity2", /*weight=*/1)));

  QueryOptions options;
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  QueryResults results = backend_->QueryHistory(/*text_query=*/{}, options);

  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(VisitContentAnnotationFlag::kNone,
            results[0].content_annotations().annotation_flags);
  EXPECT_EQ(
      0.5f,
      results[0].content_annotations().model_annotations.visibility_score);
  EXPECT_THAT(
      results[0].content_annotations().model_annotations.categories,
      ElementsAre(
          VisitContentModelAnnotations::Category(/*id=*/"1", /*weight=*/1),
          VisitContentModelAnnotations::Category(/*id=*/"2", /*weight=*/1)));
  EXPECT_EQ(123, results[0]
                     .content_annotations()
                     .model_annotations.page_topics_model_version);
  EXPECT_THAT(results[0].content_annotations().model_annotations.entities,
              ElementsAre(VisitContentModelAnnotations::Category(
                              /*id=*/"entity1", /*weight=*/1),
                          VisitContentModelAnnotations::Category(
                              /*id=*/"entity2", /*weight=*/1)));

  // Now, delete the URL. Content Annotations should be deleted.
  backend_->DeleteURL(url);
  ASSERT_FALSE(backend_->db()->GetContentAnnotationsForVisit(
      visit_id, &got_content_annotations));
}

TEST_F(HistoryBackendTest, AddRelatedSearches) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://pagewithvisit.com");
  ContextID context_id = 1;
  int nav_entry_id = 1;

  HistoryAddPageArgs request(url, base::Time::Now(), context_id, nav_entry_id,
                             GURL(), RedirectList(), ui::PAGE_TRANSITION_TYPED,
                             false, SOURCE_BROWSED, false, true);
  backend_->AddPage(request);

  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(1U, visits.size());
  VisitID visit_id = visits[0].visit_id;

  backend_->AddRelatedSearchesForVisit(
      visit_id, {"related searches", "búsquedas relacionadas"});

  VisitContentAnnotations got_content_annotations;
  ASSERT_TRUE(backend_->db()->GetContentAnnotationsForVisit(
      visit_id, &got_content_annotations));

  EXPECT_EQ(VisitContentAnnotationFlag::kNone,
            got_content_annotations.annotation_flags);
  EXPECT_EQ(-1.0f, got_content_annotations.model_annotations.visibility_score);
  ASSERT_TRUE(got_content_annotations.model_annotations.categories.empty());
  EXPECT_EQ(
      -1, got_content_annotations.model_annotations.page_topics_model_version);
  ASSERT_TRUE(got_content_annotations.model_annotations.entities.empty());
  EXPECT_THAT(got_content_annotations.related_searches,
              ElementsAre("related searches", "búsquedas relacionadas"));

  QueryOptions options;
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  QueryResults results = backend_->QueryHistory(/*text_query=*/{}, options);

  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(VisitContentAnnotationFlag::kNone,
            results[0].content_annotations().annotation_flags);
  EXPECT_EQ(VisitContentAnnotationFlag::kNone,
            got_content_annotations.annotation_flags);
  EXPECT_EQ(-1.0f, got_content_annotations.model_annotations.visibility_score);
  ASSERT_TRUE(got_content_annotations.model_annotations.categories.empty());
  EXPECT_EQ(
      -1, got_content_annotations.model_annotations.page_topics_model_version);
  ASSERT_TRUE(got_content_annotations.model_annotations.entities.empty());
  EXPECT_THAT(got_content_annotations.related_searches,
              ElementsAre("related searches", "búsquedas relacionadas"));

  // Now, delete the URL. Content Annotations should be deleted.
  backend_->DeleteURL(url);
  ASSERT_FALSE(backend_->db()->GetContentAnnotationsForVisit(
      visit_id, &got_content_annotations));
}

TEST_F(HistoryBackendTest, AddSearchMetadata) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://pagewithvisit.com?q=search#garbage");
  ContextID context_id = 1;
  int nav_entry_id = 1;

  HistoryAddPageArgs request(url, base::Time::Now(), context_id, nav_entry_id,
                             GURL(), RedirectList(), ui::PAGE_TRANSITION_TYPED,
                             false, SOURCE_BROWSED, false, true);
  backend_->AddPage(request);

  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(1U, visits.size());
  VisitID visit_id = visits[0].visit_id;

  backend_->AddSearchMetadataForVisit(
      visit_id, GURL("http://pagewithvisit.com?q=search"), u"search");

  VisitContentAnnotations got_content_annotations;
  ASSERT_TRUE(backend_->db()->GetContentAnnotationsForVisit(
      visit_id, &got_content_annotations));

  EXPECT_EQ(VisitContentAnnotationFlag::kNone,
            got_content_annotations.annotation_flags);
  EXPECT_EQ(-1.0f, got_content_annotations.model_annotations.visibility_score);
  ASSERT_TRUE(got_content_annotations.model_annotations.categories.empty());
  EXPECT_EQ(
      -1, got_content_annotations.model_annotations.page_topics_model_version);
  ASSERT_TRUE(got_content_annotations.model_annotations.entities.empty());
  ASSERT_TRUE(got_content_annotations.related_searches.empty());
  EXPECT_EQ(got_content_annotations.search_normalized_url,
            GURL("http://pagewithvisit.com?q=search"));
  EXPECT_EQ(got_content_annotations.search_terms, u"search");

  QueryOptions options;
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  QueryResults results = backend_->QueryHistory(/*text_query=*/{}, options);

  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(VisitContentAnnotationFlag::kNone,
            results[0].content_annotations().annotation_flags);
  EXPECT_EQ(VisitContentAnnotationFlag::kNone,
            got_content_annotations.annotation_flags);
  EXPECT_EQ(-1.0f, got_content_annotations.model_annotations.visibility_score);
  ASSERT_TRUE(got_content_annotations.model_annotations.categories.empty());
  EXPECT_EQ(
      -1, got_content_annotations.model_annotations.page_topics_model_version);
  ASSERT_TRUE(got_content_annotations.model_annotations.entities.empty());
  EXPECT_TRUE(got_content_annotations.related_searches.empty());
  EXPECT_EQ(got_content_annotations.search_normalized_url,
            GURL("http://pagewithvisit.com?q=search"));
  EXPECT_EQ(got_content_annotations.search_terms, u"search");

  // Now, delete the URL. Content Annotations should be deleted.
  backend_->DeleteURL(url);
  ASSERT_FALSE(backend_->db()->GetContentAnnotationsForVisit(
      visit_id, &got_content_annotations));
}

TEST_F(HistoryBackendTest, AddPageMetadata) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://pagewithvisit.com");
  ContextID context_id = 1;
  int nav_entry_id = 1;

  HistoryAddPageArgs request(url, base::Time::Now(), context_id, nav_entry_id,
                             GURL(), RedirectList(), ui::PAGE_TRANSITION_TYPED,
                             false, SOURCE_BROWSED, false, true);
  backend_->AddPage(request);

  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(1U, visits.size());
  VisitID visit_id = visits[0].visit_id;

  backend_->AddPageMetadataForVisit(visit_id, "alternative title");

  VisitContentAnnotations got_content_annotations;
  ASSERT_TRUE(backend_->db()->GetContentAnnotationsForVisit(
      visit_id, &got_content_annotations));

  EXPECT_EQ(VisitContentAnnotationFlag::kNone,
            got_content_annotations.annotation_flags);
  EXPECT_EQ(-1.0f, got_content_annotations.model_annotations.visibility_score);
  ASSERT_TRUE(got_content_annotations.model_annotations.categories.empty());
  EXPECT_EQ(
      -1, got_content_annotations.model_annotations.page_topics_model_version);
  ASSERT_TRUE(got_content_annotations.model_annotations.entities.empty());
  ASSERT_TRUE(got_content_annotations.related_searches.empty());
  ASSERT_TRUE(got_content_annotations.search_normalized_url.is_empty());
  ASSERT_TRUE(got_content_annotations.search_terms.empty());
  EXPECT_EQ(got_content_annotations.alternative_title, "alternative title");

  QueryOptions options;
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  QueryResults results = backend_->QueryHistory(/*text_query=*/{}, options);

  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(VisitContentAnnotationFlag::kNone,
            results[0].content_annotations().annotation_flags);
  EXPECT_EQ(VisitContentAnnotationFlag::kNone,
            got_content_annotations.annotation_flags);
  EXPECT_EQ(-1.0f, got_content_annotations.model_annotations.visibility_score);
  ASSERT_TRUE(got_content_annotations.model_annotations.categories.empty());
  EXPECT_EQ(
      -1, got_content_annotations.model_annotations.page_topics_model_version);
  ASSERT_TRUE(got_content_annotations.model_annotations.entities.empty());
  ASSERT_TRUE(got_content_annotations.related_searches.empty());
  EXPECT_EQ(got_content_annotations.alternative_title, "alternative title");

  // Now, delete the URL. Content Annotations should be deleted.
  backend_->DeleteURL(url);
  ASSERT_FALSE(backend_->db()->GetContentAnnotationsForVisit(
      visit_id, &got_content_annotations));
}

TEST_F(HistoryBackendTest, MixedContentAnnotationsRequestTypes) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://pagewithvisit.com");
  ContextID context_id = 1;
  int nav_entry_id = 1;

  HistoryAddPageArgs request(url, base::Time::Now(), context_id, nav_entry_id,
                             GURL(), RedirectList(), ui::PAGE_TRANSITION_TYPED,
                             false, SOURCE_BROWSED, false, true);
  backend_->AddPage(request);

  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(1U, visits.size());
  VisitID visit_id = visits[0].visit_id;

  backend_->SetBrowsingTopicsAllowed(context_id, nav_entry_id, url);

  VisitContentModelAnnotations model_annotations = {
      0.5f,
      {{/*id=*/"1", /*weight=*/1}, {/*id=*/"2", /*weight=*/1}},
      123,
      {{/*id=*/"entity1", /*weight=*/1}, {/*id=*/"entity2", /*weight=*/1}}};
  backend_->AddContentModelAnnotationsForVisit(visit_id, model_annotations);

  VisitContentAnnotations got_content_annotations;
  ASSERT_TRUE(backend_->db()->GetContentAnnotationsForVisit(
      visit_id, &got_content_annotations));

  EXPECT_EQ(VisitContentAnnotationFlag::kBrowsingTopicsEligible,
            got_content_annotations.annotation_flags);
  EXPECT_EQ(0.5f, got_content_annotations.model_annotations.visibility_score);
  EXPECT_THAT(
      got_content_annotations.model_annotations.categories,
      ElementsAre(
          VisitContentModelAnnotations::Category(/*id=*/"1", /*weight=*/1),
          VisitContentModelAnnotations::Category(/*id=*/"2", /*weight=*/1)));
  EXPECT_EQ(
      123, got_content_annotations.model_annotations.page_topics_model_version);
  EXPECT_THAT(got_content_annotations.model_annotations.entities,
              ElementsAre(VisitContentModelAnnotations::Category(
                              /*id=*/"entity1", /*weight=*/1),
                          VisitContentModelAnnotations::Category(
                              /*id=*/"entity2", /*weight=*/1)));

  QueryOptions options;
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  QueryResults results = backend_->QueryHistory(/*text_query=*/{}, options);

  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(VisitContentAnnotationFlag::kBrowsingTopicsEligible,
            results[0].content_annotations().annotation_flags);
  EXPECT_EQ(
      0.5f,
      results[0].content_annotations().model_annotations.visibility_score);
  EXPECT_THAT(
      results[0].content_annotations().model_annotations.categories,
      ElementsAre(
          VisitContentModelAnnotations::Category(/*id=*/"1", /*weight=*/1),
          VisitContentModelAnnotations::Category(/*id=*/"2", /*weight=*/1)));
  EXPECT_EQ(123, results[0]
                     .content_annotations()
                     .model_annotations.page_topics_model_version);
  EXPECT_THAT(got_content_annotations.model_annotations.entities,
              ElementsAre(VisitContentModelAnnotations::Category(
                              /*id=*/"entity1", /*weight=*/1),
                          VisitContentModelAnnotations::Category(
                              /*id=*/"entity2", /*weight=*/1)));
}

TEST_F(HistoryBackendTest, AddVisitsSource) {
  ASSERT_TRUE(backend_.get());

  GURL url1("http://www.cnn.com");
  std::vector<VisitInfo> visits1, visits2;
  visits1.emplace_back(base::Time::Now() - base::Days(5),
                       ui::PAGE_TRANSITION_LINK);
  visits1.emplace_back(base::Time::Now() - base::Days(1),
                       ui::PAGE_TRANSITION_LINK);
  visits1.emplace_back(base::Time::Now(), ui::PAGE_TRANSITION_LINK);

  GURL url2("http://www.example.com");
  visits2.emplace_back(base::Time::Now() - base::Days(10),
                       ui::PAGE_TRANSITION_LINK);
  visits2.emplace_back(base::Time::Now(), ui::PAGE_TRANSITION_LINK);

  // Clear all history.
  backend_->DeleteAllHistory();

  // Add the visits.
  backend_->AddVisits(url1, visits1, SOURCE_IE_IMPORTED);
  backend_->AddVisits(url2, visits2, SOURCE_SYNCED);

  // Verify the visits were added with their sources.
  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url1, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(3U, visits.size());
  VisitSourceMap visit_sources;
  ASSERT_TRUE(backend_->GetVisitsSource(visits, &visit_sources));
  ASSERT_EQ(3U, visit_sources.size());
  for (int i = 0; i < 3; i++)
    EXPECT_EQ(SOURCE_IE_IMPORTED, visit_sources[visits[i].visit_id]);
  id = backend_->db()->GetRowForURL(url2, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(2U, visits.size());
  ASSERT_TRUE(backend_->GetVisitsSource(visits, &visit_sources));
  ASSERT_EQ(2U, visit_sources.size());
  for (int i = 0; i < 2; i++)
    EXPECT_EQ(SOURCE_SYNCED, visit_sources[visits[i].visit_id]);
}

TEST_F(HistoryBackendTest, GetMostRecentVisits) {
  ASSERT_TRUE(backend_.get());

  GURL url1("http://www.cnn.com");
  std::vector<VisitInfo> visits1;
  visits1.emplace_back(base::Time::Now() - base::Days(5),
                       ui::PAGE_TRANSITION_LINK);
  visits1.emplace_back(base::Time::Now() - base::Days(1),
                       ui::PAGE_TRANSITION_LINK);
  visits1.emplace_back(base::Time::Now(), ui::PAGE_TRANSITION_LINK);

  // Clear all history.
  backend_->DeleteAllHistory();

  // Add the visits.
  backend_->AddVisits(url1, visits1, SOURCE_IE_IMPORTED);

  // Verify the visits were added with their sources.
  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url1, &row);
  ASSERT_TRUE(backend_->db()->GetMostRecentVisitsForURL(id, 1, &visits));
  ASSERT_EQ(1U, visits.size());
  EXPECT_EQ(visits1[2].first, visits[0].visit_time);
}

TEST_F(HistoryBackendTest, RemoveVisitsTransitions) {
  ASSERT_TRUE(backend_.get());

  // Clear all history.
  backend_->DeleteAllHistory();

  GURL url1("http://www.cnn.com");
  VisitInfo typed_visit(base::Time::Now() - base::Days(6),
                        ui::PAGE_TRANSITION_TYPED);
  VisitInfo reload_visit(base::Time::Now() - base::Days(5),
                         ui::PAGE_TRANSITION_RELOAD);
  VisitInfo link_visit(base::Time::Now() - base::Days(4),
                       ui::PAGE_TRANSITION_LINK);
  std::vector<VisitInfo> visits_to_add;
  visits_to_add.push_back(typed_visit);
  visits_to_add.push_back(reload_visit);
  visits_to_add.push_back(link_visit);

  // Add the visits.
  backend_->AddVisits(url1, visits_to_add, SOURCE_SYNCED);

  // Verify that the various counts are what we expect.
  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url1, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(3U, visits.size());
  ASSERT_EQ(1, row.typed_count());
  ASSERT_EQ(2, row.visit_count());

  // Now, delete the typed visit and verify that typed_count is updated.
  ASSERT_TRUE(backend_->RemoveVisits(VisitVector(1, visits[0])));
  id = backend_->db()->GetRowForURL(url1, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(2U, visits.size());
  ASSERT_EQ(0, row.typed_count());
  ASSERT_EQ(1, row.visit_count());

  // Delete the reload visit now and verify that none of the counts have
  // changed.
  ASSERT_TRUE(backend_->RemoveVisits(VisitVector(1, visits[0])));
  id = backend_->db()->GetRowForURL(url1, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(1U, visits.size());
  ASSERT_EQ(0, row.typed_count());
  ASSERT_EQ(1, row.visit_count());

  // Delete the last visit and verify that we delete the URL.
  ASSERT_TRUE(backend_->RemoveVisits(VisitVector(1, visits[0])));
  ASSERT_EQ(0, backend_->db()->GetRowForURL(url1, &row));
}

TEST_F(HistoryBackendTest, RemoveVisitsSource) {
  ASSERT_TRUE(backend_.get());

  GURL url1("http://www.cnn.com");
  std::vector<VisitInfo> visits1, visits2;
  visits1.emplace_back(base::Time::Now() - base::Days(5),
                       ui::PAGE_TRANSITION_LINK);
  visits1.emplace_back(base::Time::Now(), ui::PAGE_TRANSITION_LINK);

  GURL url2("http://www.example.com");
  visits2.emplace_back(base::Time::Now() - base::Days(10),
                       ui::PAGE_TRANSITION_LINK);
  visits2.emplace_back(base::Time::Now(), ui::PAGE_TRANSITION_LINK);

  // Clear all history.
  backend_->DeleteAllHistory();

  // Add the visits.
  backend_->AddVisits(url1, visits1, SOURCE_IE_IMPORTED);
  backend_->AddVisits(url2, visits2, SOURCE_SYNCED);

  // Verify the visits of url1 were added.
  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url1, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(2U, visits.size());
  // Remove these visits.
  ASSERT_TRUE(backend_->RemoveVisits(visits));

  // Now check only url2's source in visit_source table.
  VisitSourceMap visit_sources;
  ASSERT_TRUE(backend_->GetVisitsSource(visits, &visit_sources));
  ASSERT_EQ(0U, visit_sources.size());
  id = backend_->db()->GetRowForURL(url2, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(2U, visits.size());
  ASSERT_TRUE(backend_->GetVisitsSource(visits, &visit_sources));
  ASSERT_EQ(2U, visit_sources.size());
  for (int i = 0; i < 2; i++)
    EXPECT_EQ(SOURCE_SYNCED, visit_sources[visits[i].visit_id]);
}

// Test for migration of adding visit_source table.
TEST_F(HistoryBackendTest, MigrationVisitSource) {
  ASSERT_TRUE(backend_.get());
  backend_->Closing();
  backend_ = nullptr;

  base::FilePath old_history_path;
  ASSERT_TRUE(GetTestDataHistoryDir(&old_history_path));
  old_history_path = old_history_path.AppendASCII("HistoryNoSource");

  // Copy history database file to current directory so that it will be deleted
  // in Teardown.
  base::FilePath new_history_path(test_dir());
  base::DeletePathRecursively(new_history_path);
  base::CreateDirectory(new_history_path);
  base::FilePath new_history_file = new_history_path.Append(kHistoryFilename);
  ASSERT_TRUE(base::CopyFile(old_history_path, new_history_file));

  backend_ = base::MakeRefCounted<TestHistoryBackend>(
      std::make_unique<HistoryBackendTestDelegate>(this),
      history_client_.CreateBackendClient(),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  backend_->Init(false, TestHistoryDatabaseParamsForPath(new_history_path));
  backend_->Closing();
  backend_ = nullptr;

  // Now the database should already be migrated.
  // Check version first.
  int cur_version = HistoryDatabase::GetCurrentVersion();
  sql::Database db;
  ASSERT_TRUE(db.Open(new_history_file));
  sql::Statement s(
      db.GetUniqueStatement("SELECT value FROM meta WHERE key='version'"));
  ASSERT_TRUE(s.Step());
  int file_version = s.ColumnInt(0);
  EXPECT_EQ(cur_version, file_version);

  // Check visit_source table is created and empty.
  s.Assign(db.GetUniqueStatement(
      "SELECT name FROM sqlite_schema WHERE name='visit_source'"));
  ASSERT_TRUE(s.Step());
  s.Assign(db.GetUniqueStatement("SELECT * FROM visit_source LIMIT 10"));
  EXPECT_FALSE(s.Step());
}

// Test that `recent_redirects_` stores the full redirect chain in case of
// client redirects. In this case, a server-side redirect is followed by a
// client-side one.
TEST_F(HistoryBackendTest, RecentRedirectsForClientRedirects) {
  GURL server_redirect_url("http://google.com/a");
  GURL client_redirect_url("http://google.com/b");
  GURL landing_url("http://google.com/c");
  GURL clicked_url("http://google.com/d");

  // Page A is browsed by user and server redirects to B.
  HistoryAddPageArgs request(
      client_redirect_url, base::Time::Now(), 0, 0, GURL(),
      /*redirects=*/{server_redirect_url, client_redirect_url},
      ui::PAGE_TRANSITION_TYPED, false, SOURCE_BROWSED, false, true);
  backend_->AddPage(request);

  // Client redirect to page C (non-user initiated).
  AddClientRedirect(client_redirect_url, landing_url, /*did_replace=*/true,
                    base::Time(), /*transition1=*/nullptr,
                    /*transition2=*/nullptr);

  EXPECT_THAT(
      backend_->recent_redirects_.Get(landing_url)->second,
      ElementsAre(server_redirect_url, client_redirect_url, landing_url));

  // Navigation to page D (user initiated).
  AddClientRedirect(landing_url, clicked_url, /*did_replace=*/false,
                    base::Time(), /*transition1=*/nullptr,
                    /*transition2=*/nullptr);

  EXPECT_THAT(backend_->recent_redirects_.Get(clicked_url)->second,
              ElementsAre(clicked_url));
}

// Test that adding a favicon for a new icon URL:
// - Sends a notification that the favicon for the page URL has changed.
// - Does not send a notification that the favicon for the icon URL has changed
//   as there are no other page URLs which use the icon URL.
TEST_F(HistoryBackendTest, FaviconChangedNotificationNewFavicon) {
  GURL page_url1("http://www.google.com/a");
  GURL icon_url1("http://www.google.com/favicon1.ico");
  GURL page_url2("http://www.google.com/b");
  GURL icon_url2("http://www.google.com/favicon2.ico");

  // SetFavicons()
  {
    std::vector<SkBitmap> bitmaps;
    bitmaps.push_back(CreateBitmap(SK_ColorBLUE, kSmallEdgeSize));
    backend_->SetFavicons({page_url1}, IconType::kFavicon, icon_url1, bitmaps);
    ASSERT_EQ(1u, favicon_changed_notifications_page_urls().size());
    EXPECT_EQ(page_url1, favicon_changed_notifications_page_urls()[0]);
    EXPECT_EQ(1u, favicon_changed_notifications_icon_urls().size());
    ClearBroadcastedNotifications();
  }

  // MergeFavicon()
  {
    std::vector<unsigned char> data;
    data.push_back('a');
    scoped_refptr<base::RefCountedBytes> bitmap_data(
        new base::RefCountedBytes(data));
    backend_->MergeFavicon(page_url2, icon_url2, IconType::kFavicon,
                           bitmap_data, kSmallSize);
    ASSERT_EQ(1u, favicon_changed_notifications_page_urls().size());
    EXPECT_EQ(page_url2, favicon_changed_notifications_page_urls()[0]);
    EXPECT_EQ(1u, favicon_changed_notifications_icon_urls().size());
  }
}

// Test that changing the favicon bitmap data for an icon URL:
// - Does not send a notification that the favicon for the page URL has changed.
// - Sends a notification that the favicon for the icon URL has changed (Several
//   page URLs may be mapped to the icon URL).
TEST_F(HistoryBackendTest, FaviconChangedNotificationBitmapDataChanged) {
  GURL page_url("http://www.google.com");
  GURL icon_url("http://www.google.com/favicon.ico");

  // Setup
  {
    std::vector<SkBitmap> bitmaps;
    bitmaps.push_back(CreateBitmap(SK_ColorBLUE, kSmallEdgeSize));
    backend_->SetFavicons({page_url}, IconType::kFavicon, icon_url, bitmaps);
    ClearBroadcastedNotifications();
  }

  // SetFavicons()
  {
    std::vector<SkBitmap> bitmaps;
    bitmaps.push_back(CreateBitmap(SK_ColorWHITE, kSmallEdgeSize));
    backend_->SetFavicons({page_url}, IconType::kFavicon, icon_url, bitmaps);
    EXPECT_EQ(0u, favicon_changed_notifications_page_urls().size());
    ASSERT_EQ(1u, favicon_changed_notifications_icon_urls().size());
    EXPECT_EQ(icon_url, favicon_changed_notifications_icon_urls()[0]);
    ClearBroadcastedNotifications();
  }

  // MergeFavicon()
  {
    std::vector<unsigned char> data;
    data.push_back('a');
    scoped_refptr<base::RefCountedBytes> bitmap_data(
        new base::RefCountedBytes(data));
    backend_->MergeFavicon(page_url, icon_url, IconType::kFavicon, bitmap_data,
                           kSmallSize);
    EXPECT_EQ(0u, favicon_changed_notifications_page_urls().size());
    ASSERT_EQ(1u, favicon_changed_notifications_icon_urls().size());
    EXPECT_EQ(icon_url, favicon_changed_notifications_icon_urls()[0]);
  }
}

// Test that changing the page URL -> icon URL mapping:
// - Sends a notification that the favicon for the page URL has changed.
// - Does not send a notification that the favicon for the icon URL has changed.
TEST_F(HistoryBackendTest, FaviconChangedNotificationIconMappingChanged) {
  GURL page_url1("http://www.google.com/a");
  GURL page_url2("http://www.google.com/b");
  GURL page_url3("http://www.google.com/c");
  GURL page_url4("http://www.google.com/d");
  GURL icon_url1("http://www.google.com/favicon1.ico");
  GURL icon_url2("http://www.google.com/favicon2.ico");

  SkBitmap bitmap(CreateBitmap(SK_ColorBLUE, kSmallEdgeSize));
  std::vector<SkBitmap> bitmaps;
  bitmaps.push_back(bitmap);
  std::vector<unsigned char> png_bytes;
  ASSERT_TRUE(gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &png_bytes));

  // Setup
  {
    backend_->SetFavicons({page_url1}, IconType::kFavicon, icon_url1, bitmaps);
    backend_->SetFavicons({page_url2}, IconType::kFavicon, icon_url2, bitmaps);

    // Map `page_url3` to `icon_url1` so that the test does not delete the
    // favicon at `icon_url1`.
    std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results =
        backend_->UpdateFaviconMappingsAndFetch({page_url3}, icon_url1,
                                                IconType::kFavicon,
                                                GetEdgeSizesSmallAndLarge());
    ClearBroadcastedNotifications();
  }

  // SetFavicons()
  backend_->SetFavicons({page_url1}, IconType::kFavicon, icon_url2, bitmaps);
  EXPECT_THAT(favicon_changed_notifications_page_urls(),
              ElementsAre(page_url1));
  EXPECT_EQ(0u, favicon_changed_notifications_icon_urls().size());
  ClearBroadcastedNotifications();

  // MergeFavicon()
  backend_->MergeFavicon(page_url1, icon_url1, IconType::kFavicon,
                         new base::RefCountedBytes(png_bytes), kSmallSize);
  EXPECT_THAT(favicon_changed_notifications_page_urls(),
              ElementsAre(page_url1));
  EXPECT_EQ(0u, favicon_changed_notifications_icon_urls().size());
  ClearBroadcastedNotifications();

  // UpdateFaviconMappingsAndFetch()
  {
    std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results =
        backend_->UpdateFaviconMappingsAndFetch({page_url1}, icon_url2,
                                                IconType::kFavicon,
                                                GetEdgeSizesSmallAndLarge());
    EXPECT_THAT(favicon_changed_notifications_page_urls(),
                ElementsAre(page_url1));
    EXPECT_EQ(0u, favicon_changed_notifications_icon_urls().size());
  }
}

// Test that changing the page URL -> icon URL mapping for multiple page URLs
// sends notifications that the favicon for each page URL has changed.
TEST_F(HistoryBackendTest,
       FaviconChangedNotificationIconMappingChangedForMultiplePages) {
  GURL page_url1("http://www.google.com/a");
  GURL page_url2("http://www.google.com/b");
  GURL page_url3("http://www.google.com/c");
  GURL page_url4("http://www.google.com/d");
  GURL icon_url("http://www.google.com/favicon.ico");

  SkBitmap bitmap(CreateBitmap(SK_ColorBLUE, kSmallEdgeSize));
  std::vector<unsigned char> png_bytes;
  ASSERT_TRUE(gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &png_bytes));

  // Setup
  {
    std::vector<SkBitmap> bitmaps;
    bitmaps.push_back(bitmap);
    backend_->SetFavicons({page_url4}, IconType::kFavicon, icon_url, bitmaps);
    ClearBroadcastedNotifications();
  }

  // UpdateFaviconMappingsAndFetch() for two page URLs.
  {
    std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results =
        backend_->UpdateFaviconMappingsAndFetch({page_url1, page_url2},
                                                icon_url, IconType::kFavicon,
                                                GetEdgeSizesSmallAndLarge());
    EXPECT_THAT(favicon_changed_notifications_page_urls(),
                ElementsAre(page_url1, page_url2));
    ClearBroadcastedNotifications();
  }

  // UpdateFaviconMappingsAndFetch() for two page URLs, but only one needs an
  // update.
  {
    std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results =
        backend_->UpdateFaviconMappingsAndFetch({page_url3, page_url4},
                                                icon_url, IconType::kFavicon,
                                                GetEdgeSizesSmallAndLarge());
    EXPECT_THAT(favicon_changed_notifications_page_urls(),
                ElementsAre(page_url3));
  }
}

// Test that changing both:
// - The page URL -> icon URL mapping
// - The favicon's bitmap data
// sends notifications that the favicon data for both the page URL and the icon
// URL have changed.
TEST_F(HistoryBackendTest,
       FaviconChangedNotificationIconMappingAndBitmapDataChanged) {
  GURL page_url1("http://www.google.com/a");
  GURL page_url2("http://www.google.com/b");
  GURL page_url3("http://www.google.com/c");
  GURL icon_url1("http://www.google.com/favicon1.ico");
  GURL icon_url2("http://www.google.com/favicon2.ico");

  // Setup
  {
    std::vector<SkBitmap> bitmaps;
    bitmaps.push_back(CreateBitmap(SK_ColorBLUE, kSmallEdgeSize));
    backend_->SetFavicons({page_url1}, IconType::kFavicon, icon_url1, bitmaps);
    backend_->SetFavicons({page_url2}, IconType::kFavicon, icon_url2, bitmaps);

    // Map `page_url3` to `icon_url1` so that the test does not delete the
    // favicon at `icon_url1`.
    std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results =
        backend_->UpdateFaviconMappingsAndFetch({page_url3}, icon_url1,
                                                IconType::kFavicon,
                                                GetEdgeSizesSmallAndLarge());
    ClearBroadcastedNotifications();
  }

  // SetFavicons()
  {
    std::vector<SkBitmap> bitmaps;
    bitmaps.push_back(CreateBitmap(SK_ColorWHITE, kSmallEdgeSize));
    backend_->SetFavicons({page_url1}, IconType::kFavicon, icon_url2, bitmaps);
    ASSERT_EQ(1u, favicon_changed_notifications_page_urls().size());
    EXPECT_EQ(page_url1, favicon_changed_notifications_page_urls()[0]);
    ASSERT_EQ(1u, favicon_changed_notifications_icon_urls().size());
    EXPECT_EQ(icon_url2, favicon_changed_notifications_icon_urls()[0]);
    ClearBroadcastedNotifications();
  }

  // MergeFavicon()
  {
    std::vector<unsigned char> data;
    data.push_back('a');
    scoped_refptr<base::RefCountedBytes> bitmap_data(
        new base::RefCountedBytes(data));
    backend_->MergeFavicon(page_url1, icon_url1, IconType::kFavicon,
                           bitmap_data, kSmallSize);
    ASSERT_EQ(1u, favicon_changed_notifications_page_urls().size());
    EXPECT_EQ(page_url1, favicon_changed_notifications_page_urls()[0]);
    ASSERT_EQ(1u, favicon_changed_notifications_icon_urls().size());
    EXPECT_EQ(icon_url1, favicon_changed_notifications_icon_urls()[0]);
  }
}

// Test that if MergeFavicon() copies favicon bitmaps from one favicon to
// another that a notification is sent that the favicon at the destination
// icon URL has changed.
TEST_F(HistoryBackendTest, FaviconChangedNotificationsMergeCopy) {
  GURL page_url1("http://www.google.com/a");
  GURL icon_url1("http://www.google.com/favicon1.ico");
  GURL page_url2("http://www.google.com/b");
  GURL icon_url2("http://www.google.com/favicon2.ico");
  std::vector<unsigned char> png_bytes1;
  png_bytes1.push_back('a');
  std::vector<unsigned char> png_bytes2;
  png_bytes2.push_back('b');

  // Setup
  backend_->MergeFavicon(page_url1, icon_url1, IconType::kFavicon,
                         new base::RefCountedBytes(png_bytes1), kSmallSize);
  backend_->MergeFavicon(page_url2, icon_url2, IconType::kFavicon,
                         new base::RefCountedBytes(png_bytes2), kSmallSize);
  backend_->MergeFavicon(page_url2, icon_url2, IconType::kFavicon,
                         new base::RefCountedBytes(png_bytes2), kLargeSize);
  ClearBroadcastedNotifications();

  // Calling MergeFavicon() with `page_url2`, `icon_url1`, `png_bytes1` and
  // `kSmallSize` should cause the large favicon bitmap from `icon_url2` to
  // be copied to `icon_url1`.
  backend_->MergeFavicon(page_url2, icon_url1, IconType::kFavicon,
                         new base::RefCountedBytes(png_bytes1), kSmallSize);

  ASSERT_EQ(1u, favicon_changed_notifications_page_urls().size());
  EXPECT_EQ(page_url2, favicon_changed_notifications_page_urls()[0]);

  // A favicon bitmap was copied to the favicon at `icon_url1`. A notification
  // that the favicon at `icon_url1` has changed should be sent.
  ASSERT_EQ(1u, favicon_changed_notifications_icon_urls().size());
  EXPECT_EQ(icon_url1, favicon_changed_notifications_icon_urls()[0]);
}

// Test that no notifications are broadcast if calling SetFavicons() /
// MergeFavicon() / UpdateFaviconMappingsAndFetch() did not alter the Favicon
// database data (with the exception of the "last updated time").
TEST_F(HistoryBackendTest, NoFaviconChangedNotifications) {
  GURL page_url("http://www.google.com");
  GURL icon_url("http://www.google.com/favicon.ico");

  SkBitmap bitmap(CreateBitmap(SK_ColorBLUE, kSmallEdgeSize));
  std::vector<SkBitmap> bitmaps;
  bitmaps.push_back(bitmap);
  std::vector<unsigned char> png_bytes;
  ASSERT_TRUE(gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &png_bytes));

  // Setup
  backend_->SetFavicons({page_url}, IconType::kFavicon, icon_url, bitmaps);
  ClearBroadcastedNotifications();

  // SetFavicons()
  backend_->SetFavicons({page_url}, IconType::kFavicon, icon_url, bitmaps);

  // MergeFavicon()
  backend_->MergeFavicon(page_url, icon_url, IconType::kFavicon,
                         new base::RefCountedBytes(png_bytes), kSmallSize);

  // UpdateFaviconMappingsAndFetch()
  {
    std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results =
        backend_->UpdateFaviconMappingsAndFetch({page_url}, icon_url,
                                                IconType::kFavicon,
                                                GetEdgeSizesSmallAndLarge());
  }

  EXPECT_EQ(0u, favicon_changed_notifications_page_urls().size());
  EXPECT_EQ(0u, favicon_changed_notifications_icon_urls().size());
}


// Test that CloneFaviconMappingsForPages() propagates favicon mappings to the
// provided pages and their redirects.
TEST_F(HistoryBackendTest, CloneFaviconMappingsForPages) {
  const GURL landing_page_url1("http://www.google.com/landing");
  const GURL landing_page_url2("http://www.google.ca/landing");
  const GURL redirecting_page_url1("http://www.google.com/redirect");
  const GURL redirecting_page_url2("http://www.google.ca/redirect");
  const GURL icon_url("http://www.google.com/icon.png");

  // Setup
  {
    // A mapping exists for `landing_page_url1`.
    std::vector<favicon_base::FaviconRawBitmapData> favicon_bitmap_data;
    backend_->SetFavicons({landing_page_url1}, IconType::kFavicon, icon_url,
                          {CreateBitmap(SK_ColorBLUE, kSmallEdgeSize)});

    // Init recent_redirects_.
    backend_->recent_redirects_.Put(
        landing_page_url1,
        RedirectList{redirecting_page_url1, landing_page_url1});
    backend_->recent_redirects_.Put(
        landing_page_url2,
        RedirectList{redirecting_page_url2, landing_page_url2});
    ClearBroadcastedNotifications();
  }

  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results_out;
  backend_->CloneFaviconMappingsForPages(
      landing_page_url1, {IconType::kFavicon},
      {landing_page_url1, landing_page_url2});

  EXPECT_THAT(favicon_changed_notifications_page_urls(),
              UnorderedElementsAre(redirecting_page_url1, landing_page_url2,
                                   redirecting_page_url2));

  EXPECT_EQ(1U, GetIconMappingsForPageURL(redirecting_page_url1).size());
  EXPECT_EQ(1U, GetIconMappingsForPageURL(landing_page_url2).size());
  EXPECT_EQ(1U, GetIconMappingsForPageURL(redirecting_page_url2).size());
}

// Check that UpdateFaviconMappingsAndFetch() call back to the UI when there is
// no valid favicon database.
TEST_F(HistoryBackendTest, UpdateFaviconMappingsAndFetchNoDB) {
  // Make the favicon database invalid.
  backend_->favicon_backend_.reset();

  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results =
      backend_->UpdateFaviconMappingsAndFetch(
          {GURL()}, GURL(), IconType::kFavicon, GetEdgeSizesSmallAndLarge());

  EXPECT_TRUE(bitmap_results.empty());
}

TEST_F(HistoryBackendTest, GetCountsAndLastVisitForOrigins) {
  base::Time now = base::Time::Now();
  base::Time tomorrow = now + base::Days(1);
  base::Time yesterday = now - base::Days(1);
  base::Time last_week = now - base::Days(7);

  backend_->AddPageVisit(GURL("http://cnn.com/intl"), yesterday, 0,
                         ui::PAGE_TRANSITION_LINK, false, SOURCE_BROWSED, false,
                         false);
  backend_->AddPageVisit(GURL("http://cnn.com/us"), last_week, 0,
                         ui::PAGE_TRANSITION_LINK, false, SOURCE_BROWSED, false,
                         false);
  backend_->AddPageVisit(GURL("http://cnn.com/ny"), now, 0,
                         ui::PAGE_TRANSITION_LINK, false, SOURCE_BROWSED, false,
                         false);
  backend_->AddPageVisit(GURL("https://cnn.com/intl"), yesterday, 0,
                         ui::PAGE_TRANSITION_LINK, false, SOURCE_BROWSED, false,
                         false);
  backend_->AddPageVisit(GURL("http://cnn.com:8080/path"), yesterday, 0,
                         ui::PAGE_TRANSITION_LINK, false, SOURCE_BROWSED, false,
                         false);
  backend_->AddPageVisit(GURL("http://dogtopia.com/pups?q=poods"), now, 0,
                         ui::PAGE_TRANSITION_LINK, false, SOURCE_BROWSED, false,
                         false);

  std::set<GURL> origins;
  origins.insert(GURL("http://cnn.com/"));
  EXPECT_THAT(backend_->GetCountsAndLastVisitForOrigins(origins),
              ElementsAre(std::make_pair(GURL("http://cnn.com/"),
                                         std::make_pair(3, now))));

  origins.insert(GURL("http://dogtopia.com/"));
  origins.insert(GURL("http://cnn.com:8080/"));
  origins.insert(GURL("https://cnn.com/"));
  origins.insert(GURL("http://notpresent.com/"));
  backend_->AddPageVisit(GURL("http://cnn.com/"), tomorrow, 0,
                         ui::PAGE_TRANSITION_LINK, false, SOURCE_BROWSED, false,
                         false);

  EXPECT_THAT(
      backend_->GetCountsAndLastVisitForOrigins(origins),
      ElementsAre(
          std::make_pair(GURL("http://cnn.com/"), std::make_pair(4, tomorrow)),
          std::make_pair(GURL("http://cnn.com:8080/"),
                         std::make_pair(1, yesterday)),
          std::make_pair(GURL("http://dogtopia.com/"), std::make_pair(1, now)),
          std::make_pair(GURL("http://notpresent.com/"),
                         std::make_pair(0, base::Time())),
          std::make_pair(GURL("https://cnn.com/"),
                         std::make_pair(1, yesterday))));
}

TEST_F(HistoryBackendTest, UpdateVisitDuration) {
  // This unit test will test adding and deleting visit details information.
  ASSERT_TRUE(backend_.get());

  GURL url1("http://www.cnn.com");
  std::vector<VisitInfo> visit_info1, visit_info2;
  base::Time start_ts = base::Time::Now() - base::Days(5);
  base::Time end_ts = start_ts + base::Days(2);
  visit_info1.emplace_back(start_ts, ui::PAGE_TRANSITION_LINK);

  GURL url2("http://www.example.com");
  visit_info2.emplace_back(base::Time::Now() - base::Days(10),
                           ui::PAGE_TRANSITION_LINK);

  // Clear all history.
  backend_->DeleteAllHistory();

  // Add the visits.
  backend_->AddVisits(url1, visit_info1, SOURCE_BROWSED);
  backend_->AddVisits(url2, visit_info2, SOURCE_BROWSED);

  // Verify the entries for both visits were added in visit_details.
  VisitVector visits1, visits2;
  URLRow row;
  URLID url_id1 = backend_->db()->GetRowForURL(url1, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(url_id1, &visits1));
  ASSERT_EQ(1U, visits1.size());
  EXPECT_EQ(0, visits1[0].visit_duration.ToInternalValue());

  URLID url_id2 = backend_->db()->GetRowForURL(url2, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(url_id2, &visits2));
  ASSERT_EQ(1U, visits2.size());
  EXPECT_EQ(0, visits2[0].visit_duration.ToInternalValue());

  // Update the visit to cnn.com.
  backend_->UpdateVisitDuration(visits1[0].visit_id, end_ts);

  // Check the duration for visiting cnn.com was correctly updated.
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(url_id1, &visits1));
  ASSERT_EQ(1U, visits1.size());
  base::TimeDelta expected_duration = end_ts - start_ts;
  EXPECT_EQ(expected_duration.ToInternalValue(),
            visits1[0].visit_duration.ToInternalValue());

  // Remove the visit to cnn.com.
  ASSERT_TRUE(backend_->RemoveVisits(visits1));
}

// Test for migration of adding visit_duration column.
TEST_F(HistoryBackendTest, MigrationVisitDuration) {
  ASSERT_TRUE(backend_.get());
  backend_->Closing();
  backend_ = nullptr;

  base::FilePath old_history_path, old_history;
  ASSERT_TRUE(GetTestDataHistoryDir(&old_history_path));
  old_history = old_history_path.AppendASCII("HistoryNoDuration");

  // Copy history database file to current directory so that it will be deleted
  // in Teardown.
  base::FilePath new_history_path(test_dir());
  base::DeletePathRecursively(new_history_path);
  base::CreateDirectory(new_history_path);
  base::FilePath new_history_file = new_history_path.Append(kHistoryFilename);
  ASSERT_TRUE(base::CopyFile(old_history, new_history_file));

  backend_ = base::MakeRefCounted<TestHistoryBackend>(
      std::make_unique<HistoryBackendTestDelegate>(this),
      history_client_.CreateBackendClient(),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  backend_->Init(false, TestHistoryDatabaseParamsForPath(new_history_path));
  backend_->Closing();
  backend_ = nullptr;

  // Now the history database should already be migrated.

  // Check version in history database first.
  int cur_version = HistoryDatabase::GetCurrentVersion();
  sql::Database db;
  ASSERT_TRUE(db.Open(new_history_file));
  sql::Statement s(db.GetUniqueStatement(
      "SELECT value FROM meta WHERE key = 'version'"));
  ASSERT_TRUE(s.Step());
  int file_version = s.ColumnInt(0);
  EXPECT_EQ(cur_version, file_version);

  // Check visit_duration column in visits table is created and set to 0.
  s.Assign(db.GetUniqueStatement(
      "SELECT visit_duration FROM visits LIMIT 1"));
  ASSERT_TRUE(s.Step());
  EXPECT_EQ(0, s.ColumnInt(0));
}

TEST_F(HistoryBackendTest, AddPageNoVisitForBookmark) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://www.google.com");
  std::u16string title(u"Bookmark title");
  backend_->AddPageNoVisitForBookmark(url, title);

  URLRow row;
  backend_->GetURL(url, &row);
  EXPECT_EQ(url, row.url());
  EXPECT_EQ(title, row.title());
  EXPECT_EQ(0, row.visit_count());

  backend_->DeleteURL(url);
  backend_->AddPageNoVisitForBookmark(url, std::u16string());
  backend_->GetURL(url, &row);
  EXPECT_EQ(url, row.url());
  EXPECT_EQ(base::UTF8ToUTF16(url.spec()), row.title());
  EXPECT_EQ(0, row.visit_count());
}

TEST_F(HistoryBackendTest, ExpireHistoryForTimes) {
  ASSERT_TRUE(backend_.get());

  HistoryAddPageArgs args[10];
  for (size_t i = 0; i < std::size(args); ++i) {
    args[i].url =
        GURL("http://example" + std::string((i % 2 == 0 ? ".com" : ".net")));
    args[i].time = base::Time::FromInternalValue(i);
    backend_->AddPage(args[i]);
  }
  EXPECT_EQ(base::Time(), backend_->GetFirstRecordedTimeForTest());

  URLRow row;
  for (auto& arg : args)
    EXPECT_TRUE(backend_->GetURL(arg.url, &row));

  std::set<base::Time> times;
  times.insert(args[5].time);
  // Invalid time (outside range), should have no effect.
  times.insert(base::Time::FromInternalValue(10));
  backend_->ExpireHistoryForTimes(times, base::Time::FromInternalValue(2),
                                  base::Time::FromInternalValue(8));

  EXPECT_EQ(base::Time::FromInternalValue(0),
            backend_->GetFirstRecordedTimeForTest());

  // Visits to http://example.com are untouched.
  VisitVector visit_vector;
  EXPECT_TRUE(backend_->GetVisitsForURL(
      backend_->db_->GetRowForURL(GURL("http://example.com"), nullptr),
      &visit_vector));
  ASSERT_EQ(5u, visit_vector.size());
  EXPECT_EQ(base::Time::FromInternalValue(0), visit_vector[0].visit_time);
  EXPECT_EQ(base::Time::FromInternalValue(2), visit_vector[1].visit_time);
  EXPECT_EQ(base::Time::FromInternalValue(4), visit_vector[2].visit_time);
  EXPECT_EQ(base::Time::FromInternalValue(6), visit_vector[3].visit_time);
  EXPECT_EQ(base::Time::FromInternalValue(8), visit_vector[4].visit_time);

  // Visits to http://example.net between [2,8] are removed.
  visit_vector.clear();
  EXPECT_TRUE(backend_->GetVisitsForURL(
      backend_->db_->GetRowForURL(GURL("http://example.net"), nullptr),
      &visit_vector));
  ASSERT_EQ(2u, visit_vector.size());
  EXPECT_EQ(base::Time::FromInternalValue(1), visit_vector[0].visit_time);
  EXPECT_EQ(base::Time::FromInternalValue(9), visit_vector[1].visit_time);

  EXPECT_EQ(base::Time::FromInternalValue(0),
            backend_->GetFirstRecordedTimeForTest());
}

TEST_F(HistoryBackendTest, ExpireHistory) {
  ASSERT_TRUE(backend_.get());
  // Since history operations are dependent on the local timezone, make all
  // entries relative to a fixed, local reference time.
  base::Time reference_time =
      base::Time::UnixEpoch().LocalMidnight() + base::Hours(12);

  // Insert 4 entries into the database.
  HistoryAddPageArgs args[4];
  for (size_t i = 0; i < std::size(args); ++i) {
    args[i].url = GURL("http://example" + base::NumberToString(i) + ".com");
    args[i].time = reference_time + base::Days(i);
    backend_->AddPage(args[i]);
  }

  URLRow url_rows[4];
  for (unsigned int i = 0; i < std::size(args); ++i)
    ASSERT_TRUE(backend_->GetURL(args[i].url, &url_rows[i]));

  std::vector<ExpireHistoryArgs> expire_list;
  VisitVector visits;

  // Passing an empty map should be a no-op.
  backend_->ExpireHistory(expire_list);
  backend_->db()->GetAllVisitsInRange(base::Time(), base::Time(), 0, &visits);
  EXPECT_EQ(4U, visits.size());

  // Trying to delete an unknown URL with the time of the first visit should
  // also be a no-op.
  expire_list.resize(expire_list.size() + 1);
  expire_list[0].SetTimeRangeForOneDay(args[0].time);
  expire_list[0].urls.insert(GURL("http://google.does-not-exist"));
  backend_->ExpireHistory(expire_list);
  backend_->db()->GetAllVisitsInRange(base::Time(), base::Time(), 0, &visits);
  EXPECT_EQ(4U, visits.size());

  // Now add the first URL with the same time -- it should get deleted.
  expire_list.back().urls.insert(url_rows[0].url());
  backend_->ExpireHistory(expire_list);

  backend_->db()->GetAllVisitsInRange(base::Time(), base::Time(), 0, &visits);
  ASSERT_EQ(3U, visits.size());
  EXPECT_EQ(visits[0].url_id, url_rows[1].id());
  EXPECT_EQ(visits[1].url_id, url_rows[2].id());
  EXPECT_EQ(visits[2].url_id, url_rows[3].id());

  // The first recorded time should also get updated.
  EXPECT_EQ(backend_->GetFirstRecordedTimeForTest(), args[1].time);

  // Now delete the rest of the visits in one call.
  for (unsigned int i = 1; i < std::size(args); ++i) {
    expire_list.resize(expire_list.size() + 1);
    expire_list[i].SetTimeRangeForOneDay(args[i].time);
    expire_list[i].urls.insert(args[i].url);
  }
  backend_->ExpireHistory(expire_list);

  backend_->db()->GetAllVisitsInRange(base::Time(), base::Time(), 0, &visits);
  ASSERT_EQ(0U, visits.size());
}

TEST_F(HistoryBackendTest, DeleteMatchingUrlsForKeyword) {
  // Set up urls and keyword_search_terms
  GURL url1("https://www.bing.com/?q=bar");
  URLRow url_info1(url1);
  url_info1.set_visit_count(0);
  url_info1.set_typed_count(0);
  url_info1.set_last_visit(base::Time());
  url_info1.set_hidden(false);
  const URLID url1_id = backend_->db()->AddURL(url_info1);
  EXPECT_NE(0, url1_id);

  KeywordID keyword_id = 1;
  std::u16string keyword = u"bar";
  ASSERT_TRUE(backend_->db()->SetKeywordSearchTermsForURL(
      url1_id, keyword_id, keyword));

  GURL url2("https://www.google.com/?q=bar");
  URLRow url_info2(url2);
  url_info2.set_visit_count(0);
  url_info2.set_typed_count(0);
  url_info2.set_last_visit(base::Time());
  url_info2.set_hidden(false);
  const URLID url2_id = backend_->db()->AddURL(url_info2);
  EXPECT_NE(0, url2_id);

  KeywordID keyword_id2 = 2;
  ASSERT_TRUE(backend_->db()->SetKeywordSearchTermsForURL(
      url2_id, keyword_id2, keyword));

  // Add another visit to the same URL
  URLRow url_info3(url2);
  url_info3.set_visit_count(0);
  url_info3.set_typed_count(0);
  url_info3.set_last_visit(base::Time());
  url_info3.set_hidden(false);
  const URLID url3_id = backend_->db()->AddURL(url_info3);
  EXPECT_NE(0, url3_id);
  ASSERT_TRUE(backend_->db()->SetKeywordSearchTermsForURL(
      url3_id, keyword_id2, keyword));

  // Test that deletion works correctly
  backend_->DeleteMatchingURLsForKeyword(keyword_id2, keyword);

  // Test that rows 2 and 3 are deleted, while 1 is intact
  URLRow row;
  EXPECT_TRUE(backend_->db()->GetURLRow(url1_id, &row));
  EXPECT_EQ(url1.spec(), row.url().spec());
  EXPECT_FALSE(backend_->db()->GetURLRow(url2_id, &row));
  EXPECT_FALSE(backend_->db()->GetURLRow(url3_id, &row));

  // Test that corresponding keyword search terms are deleted for rows 2 & 3,
  // but not for row 1
  EXPECT_TRUE(backend_->db()->GetKeywordSearchTermRow(url1_id, nullptr));
  EXPECT_FALSE(backend_->db()->GetKeywordSearchTermRow(url2_id, nullptr));
  EXPECT_FALSE(backend_->db()->GetKeywordSearchTermRow(url3_id, nullptr));
}

// Test DeleteFTSIndexDatabases deletes expected files.
TEST_F(HistoryBackendTest, DeleteFTSIndexDatabases) {
  ASSERT_TRUE(backend_.get());

  base::FilePath history_path(test_dir());
  base::FilePath db1(history_path.AppendASCII("History Index 2013-05"));
  base::FilePath db1_journal(db1.InsertBeforeExtensionASCII("-journal"));
  base::FilePath db1_wal(db1.InsertBeforeExtensionASCII("-wal"));
  base::FilePath db2_symlink(history_path.AppendASCII("History Index 2013-06"));
  base::FilePath db2_actual(history_path.AppendASCII("Underlying DB"));

  // Setup dummy index database files.
  const char* data = "Dummy";
  const size_t data_len = 5;
  ASSERT_EQ(static_cast<int>(data_len), base::WriteFile(db1, data, data_len));
  ASSERT_EQ(static_cast<int>(data_len),
            base::WriteFile(db1_journal, data, data_len));
  ASSERT_EQ(static_cast<int>(data_len),
            base::WriteFile(db1_wal, data, data_len));
  ASSERT_EQ(static_cast<int>(data_len),
            base::WriteFile(db2_actual, data, data_len));
#if BUILDFLAG(IS_POSIX)
  EXPECT_TRUE(base::CreateSymbolicLink(db2_actual, db2_symlink));
#endif

  // Delete all DTS index databases.
  backend_->DeleteFTSIndexDatabases();
  EXPECT_FALSE(base::PathExists(db1));
  EXPECT_FALSE(base::PathExists(db1_wal));
  EXPECT_FALSE(base::PathExists(db1_journal));
  EXPECT_FALSE(base::PathExists(db2_symlink));
  EXPECT_TRUE(base::PathExists(db2_actual));  // Symlinks shouldn't be followed.
}

// Tests that calling DatabaseErrorCallback doesn't cause crash. (Regression
// test for https://crbug.com/796138)
TEST_F(HistoryBackendTest, DatabaseError) {
  base::HistogramTester histogram_tester;

  backend_->SetTypedURLSyncBridgeForTest(nullptr);
  backend_->DatabaseErrorCallback(SQLITE_CANTOPEN, nullptr);
  // Run loop to let any posted callbacks run before TearDown().
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "History.DatabaseSqliteError",
      static_cast<int>(sql::SqliteLoggedResultCode::kCantOpen), 1);
}

// Tests that calling DatabaseErrorCallback results in killing the database and
// notifying the TypedURLSyncBridge at the same time so that no further
// notification from the backend can lead to the bridge. (Regression test for
// https://crbug.com/853395)
TEST_F(HistoryBackendTest, DatabaseErrorSynchronouslyKillAndNotifyBridge) {
  // Notify the backend that a database error occurred.
  backend_->DatabaseErrorCallback(SQLITE_CORRUPT, nullptr);
  // In-between (before the posted task finishes), we can again delete all
  // history.
  backend_->ExpireHistoryBetween(/*restrict_urls=*/std::set<GURL>(),
                                 /*begin_time=*/base::Time(),
                                 /*end_time=*/base::Time::Max(),
                                 /*user_initiated*/ true);

  // Run loop to let the posted task to kill the DB run.
  base::RunLoop().RunUntilIdle();
  // After DB is destroyed, we can again try to delete all history (with no
  // effect but it should not crash).
  backend_->ExpireHistoryBetween(/*restrict_urls=*/std::set<GURL>(),
                                 /*begin_time=*/base::Time(),
                                 /*end_time=*/base::Time::Max(),
                                 /*user_initiated*/ true);
}

// Tests that a typed navigation which results in a redirect from HTTP to HTTPS
// will cause the HTTPS URL to accrue the typed count, and the HTTP URL to not.
TEST_F(HistoryBackendTest, RedirectScoring) {
  // Non-typed navigations should not increase the count for either.
  const char* redirect1[] = {"http://foo1.com/page1.html",
                             "https://foo1.com/page1.html", nullptr};
  AddRedirectChainWithTransitionAndTime(redirect1, 0, ui::PAGE_TRANSITION_LINK,
                                        base::Time::Now());
  URLRow url_row;
  ASSERT_TRUE(backend_->GetURL(GURL("http://foo1.com/page1.html"), &url_row));
  EXPECT_EQ(0, url_row.typed_count());
  ASSERT_TRUE(backend_->GetURL(GURL("https://foo1.com/page1.html"), &url_row));
  EXPECT_EQ(0, url_row.typed_count());

  // Typed navigation with a redirect from HTTP to HTTPS should count for the
  // HTTPS URL.
  AddRedirectChainWithTransitionAndTime(redirect1, 1, ui::PAGE_TRANSITION_TYPED,
                                        base::Time::Now());
  ASSERT_TRUE(backend_->GetURL(GURL("http://foo1.com/page1.html"), &url_row));
  EXPECT_EQ(0, url_row.typed_count());
  ASSERT_TRUE(backend_->GetURL(GURL("https://foo1.com/page1.html"), &url_row));
  EXPECT_EQ(1, url_row.typed_count());

  // The HTTPS URL should accrue the typed count, even if it adds a trivial
  // subdomain.
  const char* redirect2[] = {"http://foo2.com", "https://www.foo2.com",
                             nullptr};
  AddRedirectChainWithTransitionAndTime(redirect2, 2, ui::PAGE_TRANSITION_TYPED,
                                        base::Time::Now());
  ASSERT_TRUE(backend_->GetURL(GURL("http://foo2.com"), &url_row));
  EXPECT_EQ(0, url_row.typed_count());
  ASSERT_TRUE(backend_->GetURL(GURL("https://www.foo2.com"), &url_row));
  EXPECT_EQ(1, url_row.typed_count());

  // The HTTPS URL should accrue the typed count, even if it removes a trivial
  // subdomain.
  const char* redirect3[] = {"http://www.foo3.com", "https://foo3.com",
                             nullptr};
  AddRedirectChainWithTransitionAndTime(redirect3, 3, ui::PAGE_TRANSITION_TYPED,
                                        base::Time::Now());
  ASSERT_TRUE(backend_->GetURL(GURL("http://www.foo3.com"), &url_row));
  EXPECT_EQ(0, url_row.typed_count());
  ASSERT_TRUE(backend_->GetURL(GURL("https://foo3.com"), &url_row));
  EXPECT_EQ(1, url_row.typed_count());

  // A typed navigation redirecting to a different URL (not simply HTTP to HTTPS
  // with trivial subdomain changes) should have the first URL accrue the typed
  // count, not the second.
  const char* redirect4[] = {"http://foo4.com", "https://foo4.com/page1.html",
                             nullptr};
  AddRedirectChainWithTransitionAndTime(redirect4, 4, ui::PAGE_TRANSITION_TYPED,
                                        base::Time::Now());
  ASSERT_TRUE(backend_->GetURL(GURL("http://foo4.com"), &url_row));
  EXPECT_EQ(1, url_row.typed_count());
  ASSERT_TRUE(backend_->GetURL(GURL("https://foo4.com/page1.html"), &url_row));
  EXPECT_EQ(0, url_row.typed_count());

  const char* redirect5[] = {"http://bar.com", "https://baz.com", nullptr};
  AddRedirectChainWithTransitionAndTime(redirect5, 5, ui::PAGE_TRANSITION_TYPED,
                                        base::Time::Now());
  ASSERT_TRUE(backend_->GetURL(GURL("http://bar.com"), &url_row));
  EXPECT_EQ(1, url_row.typed_count());
  ASSERT_TRUE(backend_->GetURL(GURL("https://baz.com"), &url_row));
  EXPECT_EQ(0, url_row.typed_count());

  // A typed navigation redirecting from HTTPS to HTTP should have the first URL
  // accrue the typed count, not the second.
  const char* redirect6[] = {"https://foo6.com", "http://foo6.com", nullptr};
  AddRedirectChainWithTransitionAndTime(redirect6, 6, ui::PAGE_TRANSITION_TYPED,
                                        base::Time::Now());
  ASSERT_TRUE(backend_->GetURL(GURL("https://foo6.com"), &url_row));
  EXPECT_EQ(1, url_row.typed_count());
  ASSERT_TRUE(backend_->GetURL(GURL("http://foo6.com"), &url_row));
  EXPECT_EQ(0, url_row.typed_count());

  // A long redirect chain where the first redirect is HTTP to HTTPS should
  // count for the second URL (not the first or later URLs).
  const char* redirect7[] = {"http://foo7.com", "https://foo7.com",
                             "https://foo7.com/page1.html", nullptr};
  AddRedirectChainWithTransitionAndTime(redirect7, 7, ui::PAGE_TRANSITION_TYPED,
                                        base::Time::Now());
  ASSERT_TRUE(backend_->GetURL(GURL("http://foo7.com"), &url_row));
  EXPECT_EQ(0, url_row.typed_count());
  ASSERT_TRUE(backend_->GetURL(GURL("https://foo7.com"), &url_row));
  EXPECT_EQ(1, url_row.typed_count());
  ASSERT_TRUE(backend_->GetURL(GURL("https://foo7.com/page1.html"), &url_row));
  EXPECT_EQ(0, url_row.typed_count());

  // A typed navigation redirecting from HTTP to HTTPS but using non-standard
  // port numbers should have the HTTPS URL accrue the typed count.
  const char* redirect8[] = {"http://foo8.com:1234", "https://foo8.com:9876",
                             nullptr};
  AddRedirectChainWithTransitionAndTime(redirect8, 8, ui::PAGE_TRANSITION_TYPED,
                                        base::Time::Now());
  ASSERT_TRUE(backend_->GetURL(GURL("http://foo8.com:1234"), &url_row));
  EXPECT_EQ(0, url_row.typed_count());
  ASSERT_TRUE(backend_->GetURL(GURL("https://foo8.com:9876"), &url_row));
  EXPECT_EQ(1, url_row.typed_count());
}

TEST_F(HistoryBackendTest, RedirectWithQualifiers) {
  // Create a redirect chain with 3 entries, with a page transition that
  // includes a qualifier.
  const ui::PageTransition page_transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  const char* redirects[] = {"https://foo.com/page1.html",
                             "https://foo.com/page2.html",
                             "https://foo.com/page3.html", nullptr};
  AddRedirectChainWithTransitionAndTime(redirects, 0, page_transition,
                                        base::Time::Now());

  URLRow url1;
  ASSERT_TRUE(backend_->GetURL(GURL("https://foo.com/page1.html"), &url1));
  URLRow url2;
  ASSERT_TRUE(backend_->GetURL(GURL("https://foo.com/page2.html"), &url2));
  URLRow url3;
  ASSERT_TRUE(backend_->GetURL(GURL("https://foo.com/page3.html"), &url3));

  // Grab the resulting visits.
  VisitVector visits1;
  backend_->GetVisitsForURL(url1.id(), &visits1);
  ASSERT_EQ(visits1.size(), 1u);
  VisitVector visits2;
  backend_->GetVisitsForURL(url2.id(), &visits2);
  ASSERT_EQ(visits2.size(), 1u);
  VisitVector visits3;
  backend_->GetVisitsForURL(url3.id(), &visits3);
  ASSERT_EQ(visits3.size(), 1u);

  // The page transition, including the qualifier, should have been preserved
  // across all the visits. Additionally, the appropriate redirect qualifiers
  // should have been set.
  EXPECT_TRUE(PageTransitionTypeIncludingQualifiersIs(
      visits1[0].transition,
      ui::PageTransitionFromInt(page_transition |
                                ui::PAGE_TRANSITION_CHAIN_START)));
  EXPECT_TRUE(PageTransitionTypeIncludingQualifiersIs(
      visits2[0].transition,
      ui::PageTransitionFromInt(page_transition |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT)));
  EXPECT_TRUE(PageTransitionTypeIncludingQualifiersIs(
      visits3[0].transition,
      ui::PageTransitionFromInt(page_transition |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT |
                                ui::PAGE_TRANSITION_CHAIN_END)));
}

// Tests that a typed navigation will accrue the typed count even when a client
// redirect from HTTP to HTTPS occurs.
TEST_F(HistoryBackendTest, ClientRedirectScoring) {
  const GURL typed_url("http://foo.com");
  const GURL redirected_url("https://foo.com");

  // Initial typed page visit, with no server redirects.
  HistoryAddPageArgs request(typed_url, base::Time::Now(), 0, 0, GURL(), {},
                             ui::PAGE_TRANSITION_TYPED, false, SOURCE_BROWSED,
                             false, true);
  backend_->AddPage(request);

  // Client redirect to HTTPS (non-user initiated).
  AddClientRedirect(typed_url, redirected_url, /*did_replace=*/true,
                    base::Time::Now(), /*transition1=*/nullptr,
                    /*transition2=*/nullptr);
  URLRow url_row;
  ASSERT_TRUE(backend_->GetURL(typed_url, &url_row));
  EXPECT_EQ(1, url_row.typed_count());
  ASSERT_TRUE(backend_->GetURL(redirected_url, &url_row));
  EXPECT_EQ(0, url_row.typed_count());
}

// Common implementation for the two tests below, given that the only difference
// between them is the type of the notification sent out.
void InMemoryHistoryBackendTest::TestAddingAndChangingURLRows(
    const SimulateNotificationCallback& callback) {
  const char16_t kTestTypedURLAlternativeTitle[] = u"Google Search Again";
  const char16_t kTestNonTypedURLAlternativeTitle[] = u"Google News Again";

  // Notify the in-memory database that a typed and non-typed URLRow (which were
  // never before seen by the cache) have been modified.
  URLRow row1(CreateTestTypedURL());
  URLRow row2(CreateTestNonTypedURL());
  callback.Run(&row1, &row2, nullptr);

  // The in-memory database should only pick up the typed URL, and should ignore
  // the non-typed one. The typed URL should retain the ID that was present in
  // the notification.
  URLRow cached_row1, cached_row2;
  EXPECT_NE(0, mem_backend_->db()->GetRowForURL(row1.url(), &cached_row1));
  EXPECT_EQ(0, mem_backend_->db()->GetRowForURL(row2.url(), &cached_row2));
  EXPECT_EQ(row1.id(), cached_row1.id());

  // Try changing attributes (other than typed_count) for existing URLRows.
  row1.set_title(kTestTypedURLAlternativeTitle);
  row2.set_title(kTestNonTypedURLAlternativeTitle);
  callback.Run(&row1, &row2, nullptr);

  // URLRows that are cached by the in-memory database should be updated.
  EXPECT_NE(0, mem_backend_->db()->GetRowForURL(row1.url(), &cached_row1));
  EXPECT_EQ(0, mem_backend_->db()->GetRowForURL(row2.url(), &cached_row2));
  EXPECT_EQ(kTestTypedURLAlternativeTitle, cached_row1.title());

  // Now decrease the typed count for the typed URLRow, and increase it for the
  // previously non-typed URLRow.
  row1.set_typed_count(0);
  row2.set_typed_count(2);
  callback.Run(&row1, &row2, nullptr);

  // The in-memory database should stop caching the first URLRow, and start
  // caching the second URLRow.
  EXPECT_EQ(0, mem_backend_->db()->GetRowForURL(row1.url(), &cached_row1));
  EXPECT_NE(0, mem_backend_->db()->GetRowForURL(row2.url(), &cached_row2));
  EXPECT_EQ(row2.id(), cached_row2.id());
  EXPECT_EQ(kTestNonTypedURLAlternativeTitle, cached_row2.title());
}

TEST_F(InMemoryHistoryBackendTest, OnURLsModified) {
  TestAddingAndChangingURLRows(base::BindRepeating(
      &SimulateNotificationURLsModified, base::Unretained(mem_backend_.get())));
}

TEST_F(InMemoryHistoryBackendTest, OnURLsVisisted) {
  TestAddingAndChangingURLRows(base::BindRepeating(
      &SimulateNotificationURLVisited, base::Unretained(mem_backend_.get())));
}

TEST_F(InMemoryHistoryBackendTest, OnURLsDeletedPiecewise) {
  // Add two typed and one non-typed URLRow to the in-memory database.
  URLRow row1(CreateTestTypedURL());
  URLRow row2(CreateAnotherTestTypedURL());
  URLRow row3(CreateTestNonTypedURL());
  SimulateNotificationURLsModified(mem_backend_.get(), &row1, &row2, &row3);

  // Notify the in-memory database that the second typed URL and the non-typed
  // URL has been deleted.
  SimulateNotificationURLsDeleted(&row2, &row3);

  // Expect that the first typed URL remains intact, the second typed URL is
  // correctly removed, and the non-typed URL does not magically appear.
  URLRow cached_row1;
  EXPECT_NE(0, mem_backend_->db()->GetRowForURL(row1.url(), &cached_row1));
  EXPECT_EQ(0, mem_backend_->db()->GetRowForURL(row2.url(), nullptr));
  EXPECT_EQ(0, mem_backend_->db()->GetRowForURL(row3.url(), nullptr));
  EXPECT_EQ(row1.id(), cached_row1.id());
}

TEST_F(InMemoryHistoryBackendTest, OnURLsDeletedEnMasse) {
  // Add two typed and one non-typed URLRow to the in-memory database.
  URLRow row1(CreateTestTypedURL());
  URLRow row2(CreateAnotherTestTypedURL());
  URLRow row3(CreateTestNonTypedURL());
  SimulateNotificationURLsModified(mem_backend_.get(), &row1, &row2, &row3);

  // Now notify the in-memory database that all history has been deleted.
  mem_backend_->OnURLsDeleted(nullptr, DeletionInfo::ForAllHistory());

  // Expect that everything goes away.
  EXPECT_EQ(0, mem_backend_->db()->GetRowForURL(row1.url(), nullptr));
  EXPECT_EQ(0, mem_backend_->db()->GetRowForURL(row2.url(), nullptr));
  EXPECT_EQ(0, mem_backend_->db()->GetRowForURL(row3.url(), nullptr));
}

void InMemoryHistoryBackendTest::PopulateTestURLsAndSearchTerms(
    URLRow* row1,
    URLRow* row2,
    const std::u16string& term1,
    const std::u16string& term2) {
  // Add a typed and a non-typed URLRow to the in-memory database. This time,
  // though, do it through the history backend...
  URLRows rows;
  rows.push_back(*row1);
  rows.push_back(*row2);
  backend_->AddPagesWithDetails(rows, SOURCE_BROWSED);
  backend_->db()->GetRowForURL(row1->url(), row1);  // Get effective IDs from
  backend_->db()->GetRowForURL(row2->url(), row2);  // the database.

  // ... so that we can also use that for adding the search terms. This way, we
  // not only test that the notifications involved are handled correctly, but
  // also that they are fired correctly (in the history backend).
  backend_->SetKeywordSearchTermsForURL(row1->url(), kTestKeywordId, term1);
  backend_->SetKeywordSearchTermsForURL(row2->url(), kTestKeywordId, term2);
}

TEST_F(InMemoryHistoryBackendTest, SetKeywordSearchTerms) {
  URLRow row1(CreateTestTypedURL());
  URLRow row2(CreateTestNonTypedURL());
  std::u16string term1(kTestSearchTerm1);
  std::u16string term2(kTestSearchTerm2);
  PopulateTestURLsAndSearchTerms(&row1, &row2, term1, term2);

  // Both URLs now have associated search terms, so the in-memory database
  // should cache both of them, regardless whether they have been typed or not.
  URLRow cached_row1, cached_row2;
  EXPECT_NE(0, mem_backend_->db()->GetRowForURL(row1.url(), &cached_row1));
  EXPECT_NE(0, mem_backend_->db()->GetRowForURL(row2.url(), &cached_row2));
  EXPECT_EQ(row1.id(), cached_row1.id());
  EXPECT_EQ(row2.id(), cached_row2.id());

  // Verify that lookups will actually return both search terms; and also check
  // at the low level that the rows are there.
  EXPECT_EQ(1u, GetNumberOfMatchingSearchTerms(kTestKeywordId, term1));
  EXPECT_EQ(1u, GetNumberOfMatchingSearchTerms(kTestKeywordId, term2));
  EXPECT_TRUE(mem_backend_->db()->GetKeywordSearchTermRow(row1.id(), nullptr));
  EXPECT_TRUE(mem_backend_->db()->GetKeywordSearchTermRow(row2.id(), nullptr));
}

TEST_F(InMemoryHistoryBackendTest, DeleteKeywordSearchTerms) {
  URLRow row1(CreateTestTypedURL());
  URLRow row2(CreateTestNonTypedURL());
  std::u16string term1(kTestSearchTerm1);
  std::u16string term2(kTestSearchTerm2);
  PopulateTestURLsAndSearchTerms(&row1, &row2, term1, term2);

  // Delete both search terms. This should be reflected in the in-memory DB.
  backend_->DeleteKeywordSearchTermForURL(row1.url());
  backend_->DeleteKeywordSearchTermForURL(row2.url());

  // The typed URL should remain intact.
  // Note: we do not need to guarantee anything about the non-typed URL.
  URLRow cached_row1;
  EXPECT_NE(0, mem_backend_->db()->GetRowForURL(row1.url(), &cached_row1));
  EXPECT_EQ(row1.id(), cached_row1.id());

  // Verify that the search terms are no longer returned as results, and also
  // check at the low level that they are gone for good.
  EXPECT_EQ(0u, GetNumberOfMatchingSearchTerms(kTestKeywordId, term1));
  EXPECT_EQ(0u, GetNumberOfMatchingSearchTerms(kTestKeywordId, term2));
  EXPECT_FALSE(mem_backend_->db()->GetKeywordSearchTermRow(row1.id(), nullptr));
  EXPECT_FALSE(mem_backend_->db()->GetKeywordSearchTermRow(row2.id(), nullptr));
}

TEST_F(InMemoryHistoryBackendTest, DeleteAllSearchTermsForKeyword) {
  URLRow row1(CreateTestTypedURL());
  URLRow row2(CreateTestNonTypedURL());
  std::u16string term1(kTestSearchTerm1);
  std::u16string term2(kTestSearchTerm2);
  PopulateTestURLsAndSearchTerms(&row1, &row2, term1, term2);

  // Delete all corresponding search terms from the in-memory database.
  KeywordID id = kTestKeywordId;
  mem_backend_->DeleteAllSearchTermsForKeyword(id);

  // The typed URL should remain intact.
  // Note: we do not need to guarantee anything about the non-typed URL.
  URLRow cached_row1;
  EXPECT_NE(0, mem_backend_->db()->GetRowForURL(row1.url(), &cached_row1));
  EXPECT_EQ(row1.id(), cached_row1.id());

  // Verify that the search terms are no longer returned as results, and also
  // check at the low level that they are gone for good.
  EXPECT_EQ(0u, GetNumberOfMatchingSearchTerms(kTestKeywordId, term1));
  EXPECT_EQ(0u, GetNumberOfMatchingSearchTerms(kTestKeywordId, term2));
  EXPECT_FALSE(mem_backend_->db()->GetKeywordSearchTermRow(row1.id(), nullptr));
  EXPECT_FALSE(mem_backend_->db()->GetKeywordSearchTermRow(row2.id(), nullptr));
}

TEST_F(InMemoryHistoryBackendTest, OnURLsDeletedWithSearchTerms) {
  URLRow row1(CreateTestTypedURL());
  URLRow row2(CreateTestNonTypedURL());
  std::u16string term1(kTestSearchTerm1);
  std::u16string term2(kTestSearchTerm2);
  PopulateTestURLsAndSearchTerms(&row1, &row2, term1, term2);

  // Notify the in-memory database that the second typed URL has been deleted.
  SimulateNotificationURLsDeleted(&row2);

  // Verify that the second term is no longer returned as result, and also check
  // at the low level that it is gone for good. The term corresponding to the
  // first URLRow should not be affected.
  EXPECT_EQ(1u, GetNumberOfMatchingSearchTerms(kTestKeywordId, term1));
  EXPECT_EQ(0u, GetNumberOfMatchingSearchTerms(kTestKeywordId, term2));
  EXPECT_TRUE(mem_backend_->db()->GetKeywordSearchTermRow(row1.id(), nullptr));
  EXPECT_FALSE(mem_backend_->db()->GetKeywordSearchTermRow(row2.id(), nullptr));
}

TEST_F(HistoryBackendTest, QueryMostVisitedURLs) {
  ASSERT_TRUE(backend_.get());

  // Pairs from page transitions to consider_for_ntp_most_visited.
  std::vector<std::pair<ui::PageTransition, bool>> pages;
  pages.emplace_back(ui::PAGE_TRANSITION_AUTO_BOOKMARK, true);   // good.
  pages.emplace_back(ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);  // bad.
  pages.emplace_back(ui::PAGE_TRANSITION_LINK, true);            // bad.
  pages.emplace_back(ui::PAGE_TRANSITION_TYPED, false);          // bad.
  pages.emplace_back(ui::PAGE_TRANSITION_TYPED, true);           // good.

  for (size_t i = 0; i < pages.size(); ++i) {
    HistoryAddPageArgs args;
    args.url = GURL("http://example" + base::NumberToString(i + 1) + ".com");
    args.time = base::Time::Now() - base::Days(i + 1);
    args.transition = pages[i].first;
    args.consider_for_ntp_most_visited = pages[i].second;
    backend_->AddPage(args);
  }

  MostVisitedURLList most_visited = backend_->QueryMostVisitedURLs(100);

  const std::u16string kSomeTitle;  // Ignored by equality operator.
  EXPECT_THAT(
      most_visited,
      ElementsAre(MostVisitedURL(GURL("http://example1.com"), kSomeTitle),
                  MostVisitedURL(GURL("http://example5.com"), kSomeTitle)));
}

TEST_F(HistoryBackendTest, QueryMostRepeatedQueriesForKeyword) {
  ASSERT_TRUE(backend_.get());

  // Choose the local midnight of today last week as the baseline for the time.
  base::Time base_time = base::Time::Now().LocalMidnight() - base::Days(7);
  const size_t result_count = 3;

  const KeywordID first_keyword_id = 1;
  for (size_t i = 0; i < result_count * 2; ++i) {
    HistoryAddPageArgs args;
    const std::u16string term = u"First" + base::NumberToString16(i + 1);
    args.url = GURL(u"https://www.google.com/search?q=" + term);
    args.time = base_time + base::Days(i + 1);
    args.transition = ui::PAGE_TRANSITION_TYPED;
    backend_->AddPage(args);
    backend_->SetKeywordSearchTermsForURL(args.url, first_keyword_id, term);
  }

  const KeywordID second_keyword_id = 2;
  for (size_t i = 0; i < result_count * 2; ++i) {
    HistoryAddPageArgs args;
    const std::u16string term = u"Second" + base::NumberToString16(i + 1);
    args.url = GURL(u"https://www.example.com/search?q=" + term);
    args.time = base_time + base::Days(i + 1);
    args.transition = ui::PAGE_TRANSITION_TYPED;
    backend_->AddPage(args);
    backend_->SetKeywordSearchTermsForURL(args.url, second_keyword_id, term);
  }

  {
    base::HistogramTester histogram_tester;
    KeywordSearchTermVisitList queries =
        backend_->QueryMostRepeatedQueriesForKeyword(first_keyword_id,
                                                     result_count);
    ASSERT_EQ(result_count, queries.size());
    EXPECT_EQ(u"first6", queries[0]->normalized_term);
    EXPECT_EQ(u"first5", queries[1]->normalized_term);
    EXPECT_EQ(u"first4", queries[2]->normalized_term);

    histogram_tester.ExpectTotalCount("History.QueryMostRepeatedQueriesTime",
                                      1);
    histogram_tester.ExpectTotalCount("History.QueryMostRepeatedQueriesCount",
                                      1);
    histogram_tester.ExpectUniqueSample("History.QueryMostRepeatedQueriesCount",
                                        result_count * 2, 1);
  }
}

TEST(FormatUrlForRedirectComparisonTest, TestUrlFormatting) {
  // Tests that the formatter removes HTTPS scheme, port, username/password,
  // and trivial "www." subdomain. Domain and path are left unchanged.
  GURL url1("https://foo:bar@www.baz.com:4443/path1.html");
  EXPECT_EQ(u"baz.com/path1.html", FormatUrlForRedirectComparison(url1));

  // Tests that the formatter removes the HTTP scheme.
  GURL url2("http://www.baz.com");
  EXPECT_EQ(u"baz.com/", FormatUrlForRedirectComparison(url2));

  // Tests that the formatter only removes the first subdomain.
  GURL url3("http://www.www.baz.com/");
  EXPECT_EQ(u"www.baz.com/", FormatUrlForRedirectComparison(url3));
}

TEST_F(HistoryBackendTest, ExpireVisitDeletes) {
  ASSERT_TRUE(backend_);

  GURL url("http://www.google.com/");
  const ContextID context_id = 0x1;
  const int navigation_entry_id = 2;
  HistoryAddPageArgs request(
      url, base::Time::Now(), context_id, navigation_entry_id, GURL(), {},
      ui::PAGE_TRANSITION_TYPED, false, SOURCE_BROWSED, false, true);
  backend_->AddPage(request);
  URLRow url_row;
  ASSERT_TRUE(backend_->GetURL(url, &url_row));

  VisitVector visits;
  ASSERT_TRUE(backend_->GetVisitsForURL(
      backend_->db_->GetRowForURL(url, nullptr), &visits));
  ASSERT_EQ(1u, visits.size());

  const VisitID visit_id = visits[0].visit_id;
  EXPECT_EQ(visit_id, backend_->visit_tracker().GetLastVisit(
                          context_id, navigation_entry_id, url));

  backend_->RemoveVisits(visits);
  EXPECT_EQ(0, backend_->visit_tracker().GetLastVisit(
                   context_id, navigation_entry_id, url));
}

TEST_F(HistoryBackendTest, AddPageWithContextAnnotations) {
  // Add a page including context annotations.
  base::Time visit_time = base::Time::Now();
  GURL url("https://www.google.com/");
  VisitContextAnnotations::OnVisitFields context_annotations;
  context_annotations.browser_type =
      VisitContextAnnotations::BrowserType::kTabbed;
  context_annotations.window_id = SessionID::FromSerializedValue(2);
  context_annotations.tab_id = SessionID::FromSerializedValue(3);
  context_annotations.task_id = 4;
  context_annotations.root_task_id = 5;
  context_annotations.parent_task_id = 6;
  context_annotations.response_code = 200;
  HistoryAddPageArgs request(
      url, visit_time, /*context_id=*/0,
      /*nav_entry_id=*/0, /*referrer=*/GURL(), RedirectList(),
      ui::PAGE_TRANSITION_TYPED, /*hidden=*/false, SOURCE_BROWSED,
      /*did_replace_entry=*/false, /*consider_for_ntp_most_visited=*/true,
      /*title=*/absl::nullopt, /*opener=*/absl::nullopt,
      /*bookmark_id=*/absl::nullopt, context_annotations);
  backend_->AddPage(request);

  // Read the visit back from the DB and make sure the annotations are there.
  history::QueryOptions query_options;
  query_options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  std::vector<AnnotatedVisit> annotated_visits =
      backend_->GetAnnotatedVisits(query_options);
  ASSERT_EQ(annotated_visits.size(), 1u);

  EXPECT_EQ(context_annotations,
            annotated_visits[0].context_annotations.on_visit);
}

TEST_F(HistoryBackendTest, AnnotatedVisits) {
  auto last_visit_time = base::Time::Now();
  const auto add_url_and_visit = [&](std::string url) {
    // Each visit should have a unique `visit_time` to avoid deduping visits to
    // the same URL. The exact times don't matter, but we use increasing values
    // to make the test cases easy to reason about.
    last_visit_time += base::Milliseconds(1);
    return backend_->AddPageVisit(
        GURL(url), last_visit_time, /*referring_visit=*/0,
        // Must set this so that the visit is considered 'visible'.
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_CHAIN_START |
                                  ui::PAGE_TRANSITION_CHAIN_END),
        /*hidden=*/false, SOURCE_BROWSED, /*should_increment_typed_count=*/true,
        /*opener_visit=*/0);
  };

  const auto delete_url = [&](URLID id) { backend_->db_->DeleteURLRow(id); };
  const auto delete_visit = [&](VisitID id) {
    VisitRow row;
    backend_->db_->GetRowForVisit(id, &row);
    backend_->db_->DeleteVisit(row);
  };

  // For test purposes, keep all the duplicates.
  history::QueryOptions query_options;
  query_options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;

  // Happy path; annotated visits with associated URL & visits.
  ASSERT_EQ(add_url_and_visit("http://1.com/"),
            (std::pair<URLID, VisitID>{1, 1}));
  ASSERT_EQ(add_url_and_visit("http://2.com/"),
            (std::pair<URLID, VisitID>{2, 2}));
  ASSERT_EQ(add_url_and_visit("http://1.com/"),
            (std::pair<URLID, VisitID>{1, 3}));
  backend_->AddContextAnnotationsForVisit(1, MakeContextAnnotations(true));
  backend_->AddContextAnnotationsForVisit(3, MakeContextAnnotations(false));
  backend_->AddContextAnnotationsForVisit(2, MakeContextAnnotations(true));
  EXPECT_EQ(backend_->GetAnnotatedVisits(query_options).size(), 3u);

  // Annotated visits should have a visit IDs.
  EXPECT_DCHECK_DEATH(
      backend_->AddContextAnnotationsForVisit(0, MakeContextAnnotations(true)));
  EXPECT_EQ(backend_->GetAnnotatedVisits(query_options).size(), 3u);

  // `GetAnnotatedVisits()` should still succeed to fetch visits that lack
  // annotations. They just won't have annotations attached.
  EXPECT_EQ(add_url_and_visit("http://3.com/"),
            (std::pair<URLID, VisitID>{3, 4}));
  EXPECT_EQ(backend_->GetAnnotatedVisits(query_options).size(), 4u);

  // Annotations associated with a removed visit should not be added.
  EXPECT_EQ(add_url_and_visit("http://4.com/"),
            (std::pair<URLID, VisitID>{4, 5}));
  delete_visit(5);
  backend_->AddContextAnnotationsForVisit(5, MakeContextAnnotations(true));
  EXPECT_EQ(backend_->GetAnnotatedVisits(query_options).size(), 4u);

  // Verify only the correct annotated visits are retrieved ordered recent
  // visits first.
  auto annotated_visits = backend_->GetAnnotatedVisits(query_options);
  ASSERT_EQ(annotated_visits.size(), 4u);
  EXPECT_EQ(annotated_visits[0].url_row.id(), 3);
  EXPECT_EQ(annotated_visits[0].url_row.url(), "http://3.com/");
  EXPECT_EQ(annotated_visits[0].visit_row.visit_id, 4);
  EXPECT_EQ(annotated_visits[0].visit_row.url_id, 3);
  EXPECT_EQ(annotated_visits[0].context_annotations.omnibox_url_copied, false);
  EXPECT_EQ(annotated_visits[0].referring_visit_of_redirect_chain_start, 0);
  EXPECT_EQ(annotated_visits[1].url_row.id(), 1);
  EXPECT_EQ(annotated_visits[1].url_row.url(), "http://1.com/");
  EXPECT_EQ(annotated_visits[1].visit_row.visit_id, 3);
  EXPECT_EQ(annotated_visits[1].visit_row.url_id, 1);
  EXPECT_EQ(annotated_visits[1].context_annotations.omnibox_url_copied, false);
  EXPECT_EQ(annotated_visits[1].referring_visit_of_redirect_chain_start, 0);
  EXPECT_EQ(annotated_visits[2].url_row.id(), 2);
  EXPECT_EQ(annotated_visits[2].url_row.url(), "http://2.com/");
  EXPECT_EQ(annotated_visits[2].visit_row.visit_id, 2);
  EXPECT_EQ(annotated_visits[2].visit_row.url_id, 2);
  EXPECT_EQ(annotated_visits[2].context_annotations.omnibox_url_copied, true);
  EXPECT_EQ(annotated_visits[2].referring_visit_of_redirect_chain_start, 0);
  EXPECT_EQ(annotated_visits[3].url_row.id(), 1);
  EXPECT_EQ(annotated_visits[3].url_row.url(), "http://1.com/");
  EXPECT_EQ(annotated_visits[3].visit_row.visit_id, 1);
  EXPECT_EQ(annotated_visits[3].visit_row.url_id, 1);
  EXPECT_EQ(annotated_visits[3].context_annotations.omnibox_url_copied, true);
  EXPECT_EQ(annotated_visits[3].referring_visit_of_redirect_chain_start, 0);

  delete_url(2);
  delete_url(3);
  delete_visit(3);
  // Annotated visits should be unfetchable if their associated URL or visit is
  // removed.
  annotated_visits = backend_->GetAnnotatedVisits(query_options);
  ASSERT_EQ(annotated_visits.size(), 1u);
  EXPECT_EQ(annotated_visits[0].url_row.id(), 1);
  EXPECT_EQ(annotated_visits[0].url_row.url(), "http://1.com/");
  EXPECT_EQ(annotated_visits[0].visit_row.visit_id, 1);
  EXPECT_EQ(annotated_visits[0].visit_row.url_id, 1);
  EXPECT_EQ(annotated_visits[0].context_annotations.omnibox_url_copied, true);
}

TEST_F(HistoryBackendTest, PreservesAllContextAnnotationsFields) {
  auto [url_id, visit_id] = backend_->AddPageVisit(
      GURL("https://url.com"), base::Time::Now(), /*referring_visit=*/0,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_CHAIN_START |
                                ui::PAGE_TRANSITION_CHAIN_END),
      /*hidden=*/false, SOURCE_BROWSED, /*should_increment_typed_count=*/true,
      /*opener_visit=*/0);

  // Add context annotations with non-default values for all fields.
  VisitContextAnnotations annotations_in;
  annotations_in.on_visit.browser_type =
      VisitContextAnnotations::BrowserType::kTabbed;
  annotations_in.on_visit.window_id = SessionID::FromSerializedValue(2);
  annotations_in.on_visit.tab_id = SessionID::FromSerializedValue(3);
  annotations_in.on_visit.task_id = 4;
  annotations_in.on_visit.root_task_id = 5;
  annotations_in.on_visit.parent_task_id = 6;
  annotations_in.on_visit.response_code = 200;
  annotations_in.omnibox_url_copied = true;
  annotations_in.is_existing_part_of_tab_group = true;
  annotations_in.is_placed_in_tab_group = true;
  annotations_in.is_existing_bookmark = true;
  annotations_in.is_new_bookmark = true;
  annotations_in.is_ntp_custom_link = true;
  annotations_in.duration_since_last_visit = base::Seconds(7);
  annotations_in.page_end_reason = 8;
  annotations_in.duration_since_last_visit = base::Seconds(9);

  backend_->AddContextAnnotationsForVisit(visit_id, annotations_in);

  // Verify that we can read all the fields back from the DB.
  history::QueryOptions query_options;
  query_options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  std::vector<AnnotatedVisit> annotated_visits =
      backend_->GetAnnotatedVisits(query_options);
  ASSERT_EQ(annotated_visits.size(), 1u);

  VisitContextAnnotations annotations_out =
      annotated_visits[0].context_annotations;
  EXPECT_EQ(annotations_in, annotations_out);

  // Now update the on-close fields.
  VisitContextAnnotations annotations_update;
  annotations_update.omnibox_url_copied = false;
  annotations_update.is_existing_part_of_tab_group = false;
  annotations_update.is_placed_in_tab_group = false;
  annotations_update.is_existing_bookmark = false;
  annotations_update.is_new_bookmark = false;
  annotations_update.is_ntp_custom_link = false;
  annotations_update.duration_since_last_visit = base::Seconds(11);
  annotations_update.page_end_reason = 12;
  annotations_update.duration_since_last_visit = base::Seconds(13);
  backend_->SetOnCloseContextAnnotationsForVisit(visit_id, annotations_update);

  // Make sure the update applied: All the on-close fields should've been
  // updated, but all the on-visit fields should have kept their values.
  VisitContextAnnotations annotations_expected = annotations_update;
  annotations_expected.on_visit = annotations_in.on_visit;

  annotated_visits = backend_->GetAnnotatedVisits(query_options);
  ASSERT_EQ(annotated_visits.size(), 1u);

  annotations_out = annotated_visits[0].context_annotations;
  EXPECT_EQ(annotations_expected, annotations_out);
}

TEST_F(HistoryBackendTest, FindMostRecentClusteredTime) {
  // Should return `Min()` when there are no clusters
  EXPECT_EQ(backend_->FindMostRecentClusteredTime(), base::Time::Min());

  // Add 1 cluster with multiple visits.
  AddAnnotatedVisit(50);
  AddAnnotatedVisit(20);
  AddAnnotatedVisit(60);
  backend_->ReplaceClusters({}, CreateClusters({{1, 2, 3}}));

  // Should return the max time across all visits in the cluster.
  EXPECT_EQ(backend_->FindMostRecentClusteredTime(), GetRelativeTime(60));

  // Add another cluster.
  AddAnnotatedVisit(10);
  backend_->ReplaceClusters({}, CreateClusters({{4}}));

  // Should return the max time across all clusters.
  EXPECT_EQ(backend_->FindMostRecentClusteredTime(), GetRelativeTime(60));

  // Add another cluster.
  AddAnnotatedVisit(100);
  backend_->ReplaceClusters({}, CreateClusters({{5}}));

  // Should return the max time across all clusters.
  EXPECT_EQ(backend_->FindMostRecentClusteredTime(), GetRelativeTime(100));
}

TEST_F(HistoryBackendTest, ReplaceClusters) {
  {
    SCOPED_TRACE("Add clusters");
    AddAnnotatedVisit(0);
    AddAnnotatedVisit(1);

    backend_->ReplaceClusters({}, CreateClusters({{1, 2}, {1, 2}, {}, {1}}));
    VerifyClusters(backend_->GetMostRecentClusters(base::Time::Min(),
                                                   base::Time::Max(), 10, 1000),
                   {
                       {1, {2, 1}},
                       // Shouldn't check duplicates clusters.
                       {2, {2, 1}},
                       // Shouldn't return empty clusters.
                       // The empty cluster shouldn't increment `cluster_id`.
                       {3, {1}},
                   });
  }

  {
    SCOPED_TRACE("Replace clusters");
    AddAnnotatedVisit(2);
    AddAnnotatedVisit(3);

    backend_->ReplaceClusters({2, 4}, CreateClusters({{1, 3}, {4}}));
    VerifyClusters(backend_->GetMostRecentClusters(base::Time::Min(),
                                                   base::Time::Max(), 10, 1000),
                   {
                       {5, {4}},
                       {4, {3, 1}},
                       {1, {2, 1}},
                       {3, {1}},
                   });
  }
}

TEST_F(HistoryBackendTest, GetMostRecentClusters) {
  // Setup some visits and clusters.
  AddAnnotatedVisit(1);
  AddAnnotatedVisit(2);
  AddAnnotatedVisit(3);
  AddAnnotatedVisit(4);
  AddAnnotatedVisit(5);
  AddAnnotatedVisit(6);
  AddAnnotatedVisit(7);
  AddAnnotatedVisit(8);
  AddAnnotatedVisit(9);
  AddAnnotatedVisit(10);
  AddCluster({3, 4});
  AddCluster({5, 6, 9});
  AddCluster({10});

  {
    // Verify returns clusters with a visit >= min_time. Verify returns complete
    // clusters, including visits < min_time.
    SCOPED_TRACE("time: [9, 20), max_clusters: 10, max_visits: 100");
    VerifyClusters(backend_->GetMostRecentClusters(
                       GetRelativeTime(9), GetRelativeTime(20), 10, 100),
                   {{3, {10}}, {2, {9, 6, 5}}});
  }
  {
    // Verify doesn't return clusters with a visit > max_time.
    SCOPED_TRACE("time: [9, 20), max_clusters: 10, max_visits: 100");
    VerifyClusters(backend_->GetMostRecentClusters(GetRelativeTime(4),
                                                   GetRelativeTime(8), 10, 100),
                   {{1, {4, 3}}});
  }
  {
    // Verify `max_clusters`.
    SCOPED_TRACE("time: [0, 20), max_clusters: 1, max_visits: 100");
    VerifyClusters(backend_->GetMostRecentClusters(GetRelativeTime(0),
                                                   GetRelativeTime(20), 1, 100),
                   {{3, {10}}});
  }
  {
    // Verify `max_visits`.
    SCOPED_TRACE("time: [0, 20), max_clusters: 10, max_visits: 1");
    VerifyClusters(backend_->GetMostRecentClusters(GetRelativeTime(0),
                                                   GetRelativeTime(20), 10, 1),
                   {{3, {10}}});
  }
  {
    // Verify doesn't return clusters with invalid visits.
    SCOPED_TRACE(
        "time: [0, 20), max_clusters: 1, max_visits: 100, after url 10 "
        "deleted.");
    backend_->db()->DeleteURLRow(10);
    VerifyClusters(backend_->GetMostRecentClusters(GetRelativeTime(0),
                                                   GetRelativeTime(20), 1, 100),
                   {});
  }
  {
    // Verify doesn't deleted visits don't interfere.
    SCOPED_TRACE(
        "time: [0, 20), max_clusters: 1, max_visits: 100, after visit 10 "
        "deleted.");
    backend_->db()->DeleteAnnotationsForVisit(10);
    VerifyClusters(backend_->GetMostRecentClusters(GetRelativeTime(0),
                                                   GetRelativeTime(20), 1, 100),
                   {{2, {9, 6, 5}}});
  }
}

TEST_F(HistoryBackendTest, AddClusters_GetCluster) {
  AddAnnotatedVisit(0);
  AddAnnotatedVisit(1);

  ClusterVisit visit_1;
  visit_1.annotated_visit.visit_row.visit_id = 1;
  // URLs and times should be ignored, they'll be retrieved from the 'urls' and
  // 'visits' DBs respectively.
  visit_1.duplicate_visits.push_back(
      {2, GURL{"https://duplicate_visit.com"}, GetRelativeTime(5)});
  // A non-existent duplicate visit shouldn't be returned;
  visit_1.duplicate_visits.push_back(
      {20, GURL{"https://duplicate_visit.com"}, GetRelativeTime(5)});
  // Verify the cluster visits are being flushed out.
  visit_1.url_for_display = u"url_for_display";
  ClusterVisit visit_2;
  visit_2.annotated_visit.visit_row.visit_id = 2;
  // A cluster visit without a corresponding annotated visit shouldn't be
  // returned.
  ClusterVisit visit_3;
  visit_3.annotated_visit.visit_row.visit_id = 3;

  ClusterKeywordData keyword_data_1 = {
      ClusterKeywordData::ClusterKeywordType::kEntityAlias,
      .4,
      {"entity1", "entity2"}};
  ClusterKeywordData keyword_data_2 = {
      ClusterKeywordData::ClusterKeywordType::kEntityCategory, .6, {}};

  backend_->db_->AddClusters(
      {{0,
        {visit_1, visit_2, visit_3},
        {{u"keyword1", keyword_data_1}, {u"keyword2", keyword_data_2}},
        false,
        u"label"}});

  auto cluster = backend_->GetCluster(1, true);
  VerifyCluster(cluster, {1, {2, 1}});
  EXPECT_EQ(cluster.cluster_id, 1);
  EXPECT_EQ(cluster.label, u"label");
  EXPECT_EQ(cluster.visits[1].url_for_display, u"url_for_display");
  // Verify keywords
  EXPECT_EQ(cluster.keyword_to_data_map.size(), 2u);
  EXPECT_EQ(cluster.keyword_to_data_map[u"keyword1"].type,
            ClusterKeywordData::ClusterKeywordType::kEntityAlias);
  EXPECT_EQ(cluster.keyword_to_data_map[u"keyword1"].score, .4f);
  // Only the 1st keyword entity should be preserved.
  EXPECT_THAT(cluster.keyword_to_data_map[u"keyword1"].entity_collections,
              UnorderedElementsAre("entity1"));
  EXPECT_EQ(cluster.keyword_to_data_map[u"keyword2"].type,
            ClusterKeywordData::ClusterKeywordType::kEntityCategory);
  EXPECT_EQ(cluster.keyword_to_data_map[u"keyword2"].score, .6f);
  EXPECT_TRUE(
      cluster.keyword_to_data_map[u"keyword2"].entity_collections.empty());
  // Verify duplicate visits.
  EXPECT_TRUE(cluster.visits[0].duplicate_visits.empty());
  ASSERT_EQ(cluster.visits[1].duplicate_visits.size(), 1u);
  EXPECT_EQ(cluster.visits[1].duplicate_visits[0].visit_id, 2);
  EXPECT_EQ(
      cluster.visits[1].duplicate_visits[0].url.spec(),
      "https://google.com/1");  // The URL generated by `AddAnnotatedVisit()`.
  EXPECT_EQ(cluster.visits[1].duplicate_visits[0].visit_time,
            GetRelativeTime(1));

  // Verify keywords and duplicates are not returned, but other info is, when
  // the `include_keywords_and_duplicates` param is false.
  cluster = backend_->GetCluster(1, false);
  VerifyCluster(cluster, {1, {2, 1}});
  EXPECT_EQ(cluster.cluster_id, 1);
  EXPECT_EQ(cluster.label, u"label");
  EXPECT_EQ(cluster.visits[1].url_for_display, u"url_for_display");
  EXPECT_TRUE(cluster.keyword_to_data_map.empty());
  EXPECT_TRUE(cluster.visits[0].duplicate_visits.empty());
  EXPECT_TRUE(cluster.visits[1].duplicate_visits.empty());

  // Verify non-existent clusters aren't returned.
  VerifyCluster(backend_->GetCluster(2, true), {0});

  // Verify clusters without valid visits aren't returned. `visit_3` does not
  // exist.
  backend_->db_->AddClusters({{0, {visit_3}, {}, false, u"label"}});
  VerifyCluster(backend_->GetCluster(2, true), {0});
}

TEST_F(HistoryBackendTest, ReserveNextClusterId_GetCluster) {
  int64_t cluster_id = backend_->ReserveNextClusterId();

  // We call from the DB instead of from the backend since the DB does
  // additional checking around visit count.
  auto cluster = backend_->db_->GetCluster(cluster_id);
  EXPECT_EQ(cluster.cluster_id, cluster_id);
  EXPECT_TRUE(cluster.should_show_on_prominent_ui_surfaces);
}

TEST_F(HistoryBackendTest, ReserveNextClusterId_AddVisitsToCluster_GetCluster) {
  int64_t cluster_id = backend_->ReserveNextClusterId();

  AddAnnotatedVisit(1);
  AddAnnotatedVisit(2);
  ClusterVisit visit_1;
  visit_1.annotated_visit.visit_row.visit_id = 1;
  // Verify the cluster visits are being flushed out.
  visit_1.url_for_display = u"url_for_display";
  ClusterVisit visit_2;
  visit_2.annotated_visit.visit_row.visit_id = 2;
  backend_->AddVisitsToCluster(cluster_id, {visit_1, visit_2});

  VerifyCluster(backend_->GetCluster(cluster_id, false), {cluster_id, {2, 1}});
}

TEST_F(HistoryBackendTest, GetRedirectChainStart) {
  auto last_visit_time = base::Time::Now();
  const auto add_visit = [&](std::string url, VisitID referring_visit,
                             VisitID opener_visit, bool is_redirect) {
    // Each visit should have a unique `visit_time` to avoid deduping visits so
    // the same URL. The exact times don't matter, but we use increasing values
    // to make the test cases easy to reason about.
    last_visit_time += base::Milliseconds(1);
    // Use `ui::PAGE_TRANSITION_CHAIN_END` to make the visits user visible and
    // included in the `GetAnnotatedVisits()` response, even though they're not
    // actually representing chain end transitions.
    ui::PageTransition transition = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_CHAIN_END |
        (is_redirect ? ui::PageTransition::PAGE_TRANSITION_IS_REDIRECT_MASK
                     : ui::PageTransition::PAGE_TRANSITION_CHAIN_START));
    auto ids = backend_->AddPageVisit(GURL(url), last_visit_time,
                                      referring_visit, transition, false,
                                      SOURCE_BROWSED, false, opener_visit);
    backend_->AddContextAnnotationsForVisit(ids.second,
                                            VisitContextAnnotations());
  };

  // Navigate to 'google.com'.
  add_visit("google.com", 0, 0, false);
  // It redirects to 'https://www.google.com'.
  add_visit("https://www.google.com", 1, 0, true);
  // Perform a search.
  add_visit("https://www.google.com/query=wiki", 2, 0, false);
  // Navigate to 'https://www.google.com' in a new tab.
  add_visit("https://www.google.com", 0, 0, false);
  // Perform a search
  add_visit("https://www.google.com/query=wiki2", 4, 0, false);
  // Follow a search result link.
  add_visit("https://www.wiki2.org", 5, 0, false);
  // It redirects.
  add_visit("https://www.wiki2.org/home", 6, 0, true);
  // Follow a search result in the first tab.
  add_visit("https://www.wiki.org", 3, 0, false);
  // Open a search result link in a new tab.
  add_visit("https://www.wiki2.org", 0, 6, false);
  // It redirects.
  add_visit("https://www.wiki2.org/home", 9, 0, true);

  // The redirect/referral chain now look like this:
  // 1 ->> 2 -> 3 -> 8
  // 4 -> 5 -> 6 ->> 7
  // where '->' represents a referral, and '->>' represents a redirect.

  struct Expectation {
    VisitID referring_visit;
    VisitID opener_visit;
    VisitID first_redirect;
    VisitID referring_visit_of_redirect_chain_start;
    VisitID opener_visit_of_redirect_chain_start;
  };

  std::vector<Expectation> expectations = {
      {0, 0, 1, 0, 0}, {1, 0, 1, 0, 0}, {2, 0, 3, 2, 0}, {0, 0, 4, 0, 0},
      {4, 0, 5, 4, 0}, {5, 0, 6, 5, 0}, {6, 0, 6, 5, 0}, {3, 0, 8, 3, 0},
      {0, 6, 9, 0, 6}, {9, 0, 9, 0, 6},
  };

  QueryOptions queryOptions;
  queryOptions.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  queryOptions.visit_order = QueryOptions::OLDEST_FIRST;
  auto annotated_visits = backend_->GetAnnotatedVisits(queryOptions);
  ASSERT_EQ(annotated_visits.size(), expectations.size());
  for (size_t i = 0; i < expectations.size(); ++i) {
    VisitID visit_id = i + 1;
    const auto& expectation = expectations[i];
    VisitRow visit;
    backend_->db_->GetRowForVisit(visit_id, &visit);
    EXPECT_EQ(visit.referring_visit, expectation.referring_visit)
        << "visit id: " << visit_id;
    EXPECT_EQ(visit.opener_visit, expectation.opener_visit)
        << "visit id: " << visit_id;

    // Verify `GetRedirectChainStart()`.
    auto first_redirect = backend_->GetRedirectChainStart(visit);
    EXPECT_EQ(first_redirect.visit_id, expectation.first_redirect)
        << "visit id: " << visit_id;

    // Verify `GetAnnotatedVisits()`.
    const auto& annotated_visit = annotated_visits[i];
    EXPECT_EQ(annotated_visit.visit_row.visit_id, visit_id)
        << "visit id: " << visit_id;
    EXPECT_EQ(annotated_visit.referring_visit_of_redirect_chain_start,
              expectation.referring_visit_of_redirect_chain_start)
        << "visit id: " << visit_id;
    EXPECT_EQ(annotated_visit.opener_visit_of_redirect_chain_start,
              expectation.opener_visit_of_redirect_chain_start)
        << "visit id: " << visit_id;
  }
}

TEST_F(HistoryBackendTest, GetRedirectChain) {
  const auto add_visit_chain = [&](std::vector<std::string> urls,
                                   base::Time visit_time,
                                   VisitID referring_visit) {
    std::vector<VisitID> ids;
    for (size_t i = 0; i < urls.size(); i++) {
      int transition = ui::PAGE_TRANSITION_TYPED;
      if (i == 0) {
        transition |= ui::PAGE_TRANSITION_CHAIN_START;
      }
      if (i == urls.size() - 1) {
        transition |= ui::PAGE_TRANSITION_CHAIN_END;
      } else {
        transition |= ui::PAGE_TRANSITION_SERVER_REDIRECT;
      }
      auto url_and_visit_id =
          backend_->AddPageVisit(GURL(urls[i]), visit_time, referring_visit,
                                 ui::PageTransitionFromInt(transition), false,
                                 SOURCE_BROWSED, false, 0);
      ids.push_back(url_and_visit_id.second);

      referring_visit = url_and_visit_id.second;
    }

    return ids;
  };

  base::Time time1 = base::Time::Now();
  base::Time time2 = time1 + base::Minutes(1);
  base::Time time3 = time2 + base::Minutes(2);

  // Create visits: A single visit (no redirects), and a 2-entry redirect chain
  // which further refers to another 3-entry redirect chain.
  std::vector<VisitID> chain1_ids =
      add_visit_chain({"https://url.com"}, time1, 0);
  std::vector<VisitID> chain2_ids =
      add_visit_chain({"https://chain2a.com", "https://chain2b.com"}, time2, 0);
  std::vector<VisitID> chain3_ids = add_visit_chain(
      {"https://chain3a.com", "https://chain3b.com", "https://chain3c.com"},
      time3, chain2_ids.back());

  ASSERT_EQ(chain1_ids.size(), 1u);
  ASSERT_EQ(chain2_ids.size(), 2u);
  ASSERT_EQ(chain3_ids.size(), 3u);

  // Querying the redirect chain for the individual visit should just return
  // that one visit.
  VisitRow visit1;
  backend_->db_->GetRowForVisit(chain1_ids.back(), &visit1);
  VisitVector chain1 = backend_->GetRedirectChain(visit1);
  ASSERT_EQ(chain1.size(), 1u);
  EXPECT_EQ(chain1[0].visit_id, chain1_ids[0]);

  // Querying the chains should return the full chains, but only as linked by
  // redirects (not by referrals).
  VisitRow chain2end;
  backend_->db_->GetRowForVisit(chain2_ids.back(), &chain2end);
  VisitVector chain2 = backend_->GetRedirectChain(chain2end);
  ASSERT_EQ(chain2.size(), 2u);
  EXPECT_EQ(chain2[0].visit_id, chain2_ids[0]);
  EXPECT_EQ(chain2[1].visit_id, chain2_ids[1]);

  VisitRow chain3end;
  backend_->db_->GetRowForVisit(chain3_ids.back(), &chain3end);
  VisitVector chain3 = backend_->GetRedirectChain(chain3end);
  ASSERT_EQ(chain3.size(), 3u);
  EXPECT_EQ(chain3[0].visit_id, chain3_ids[0]);
  EXPECT_EQ(chain3[1].visit_id, chain3_ids[1]);
  EXPECT_EQ(chain3[2].visit_id, chain3_ids[2]);
}

TEST_F(HistoryBackendTest, AddSyncedVisitAddsOnlyValidURLs) {
  const ui::PageTransition kLink = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CHAIN_START |
      ui::PAGE_TRANSITION_CHAIN_END);

  // Note: Per AddSyncedVisit() preconditions (DCHECKs), the passed visit MUST
  // have visit_time and originator_cache_guid, but MUST NOT have visit_id or
  // url_id.

  // First, try to add some visits with unwanted URLs. These should *not* get
  // added to the DB.
  // Note that in this test, all valid URLs except "chrome://" ones are
  // considered valid; see HistoryBackendTestDelegate::CanAddURL.
  VisitRow foreign_visit;
  foreign_visit.visit_time = base::Time::Now();
  foreign_visit.transition = kLink;
  foreign_visit.originator_cache_guid = "originator";
  EXPECT_EQ(kInvalidVisitID,
            backend_->AddSyncedVisit(GURL("chrome://settings"), u"Settings",
                                     /*hidden=*/false, foreign_visit,
                                     absl::nullopt, absl::nullopt));
  EXPECT_EQ(kInvalidVisitID,
            backend_->AddSyncedVisit(GURL("Not a URL at all"), u"Title",
                                     /*hidden=*/false, foreign_visit,
                                     absl::nullopt, absl::nullopt));

  // A regular old URL should get added successfully.
  VisitID added_id = backend_->AddSyncedVisit(
      GURL("https://some.url"), u"Title", /*hidden=*/false, foreign_visit,
      absl::nullopt, absl::nullopt);
  EXPECT_NE(added_id, kInvalidVisitID);
  VisitRow added_visit;
  EXPECT_TRUE(backend_->GetVisitByID(added_id, &added_visit));
  EXPECT_EQ(foreign_visit.visit_time, added_visit.visit_time);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      foreign_visit.transition, added_visit.transition));
  EXPECT_EQ(foreign_visit.originator_cache_guid,
            added_visit.originator_cache_guid);
}

TEST_F(HistoryBackendTest, DeleteAllForeignVisitsDoesNotDeleteLocalVisits) {
  const ui::PageTransition kLink = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CHAIN_START |
      ui::PAGE_TRANSITION_CHAIN_END);

  const base::Time initial_time = base::Time::Now();

  // Setup: Add some visits, both local and foreign.

  VisitID local_visit_id1 =
      backend_
          ->AddPageVisit(GURL("https://local1.url"), base::Time::Now(),
                         /*referring_visit=*/kInvalidVisitID, kLink,
                         /*hidden=*/false, SOURCE_BROWSED,
                         /*should_increment_typed_count=*/false,
                         /*opener_visit=*/kInvalidVisitID)
          .second;

  task_environment_.FastForwardBy(base::Seconds(1));

  VisitRow foreign_visit1;
  foreign_visit1.visit_time = base::Time::Now();
  foreign_visit1.transition = kLink;
  foreign_visit1.originator_cache_guid = "originator";
  VisitID foreign_visit_id1 = backend_->AddSyncedVisit(
      GURL("https://remote1.url"), u"Title 1", /*hidden=*/false, foreign_visit1,
      absl::nullopt, absl::nullopt);

  task_environment_.FastForwardBy(base::Seconds(1));

  VisitID local_visit_id2 =
      backend_
          ->AddPageVisit(GURL("https://local2.url"), base::Time::Now(),
                         /*referring_visit=*/kInvalidVisitID, kLink,
                         /*hidden=*/false, SOURCE_BROWSED,
                         /*should_increment_typed_count=*/false,
                         /*opener_visit=*/kInvalidVisitID)
          .second;

  task_environment_.FastForwardBy(base::Seconds(1));

  VisitRow foreign_visit2;
  foreign_visit2.visit_time = base::Time::Now();
  foreign_visit2.transition = kLink;
  foreign_visit2.originator_cache_guid = "originator";
  VisitID foreign_visit_id2 = backend_->AddSyncedVisit(
      GURL("https://remote2.url"), u"Title 2", /*hidden=*/true, foreign_visit2,
      absl::nullopt, absl::nullopt);

  task_environment_.FastForwardBy(base::Seconds(1));

  // Setup finished - verify that the visits are there.
  {
    VisitVector visits;
    backend_->db()->GetAllVisitsInRange(initial_time, base::Time::Now(),
                                        /*max_results=*/5, &visits);
    ASSERT_THAT(visits, UnorderedElementsAre(HasVisitID(local_visit_id1),
                                             HasVisitID(local_visit_id2),
                                             HasVisitID(foreign_visit_id1),
                                             HasVisitID(foreign_visit_id2)));
  }

  // Main test body: Instruct backend to delete foreign visits.
  backend_->DeleteAllForeignVisits();
  // The deletions happens asynchronously, so wait for it to complete.
  task_environment_.RunUntilIdle();

  // Make sure the foreign visits (and only those) got deleted.
  {
    VisitVector visits;
    backend_->db()->GetAllVisitsInRange(initial_time, base::Time::Now(),
                                        /*max_results=*/5, &visits);
    ASSERT_THAT(visits, UnorderedElementsAre(HasVisitID(local_visit_id1),
                                             HasVisitID(local_visit_id2)));
  }
}

TEST_F(HistoryBackendTest, DeleteAllForeignVisitsWorksInBatches) {
  const ui::PageTransition kLink = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CHAIN_START |
      ui::PAGE_TRANSITION_CHAIN_END);

  const int visits_per_batch =
      HistoryBackend::GetForeignVisitsToDeletePerBatchForTest();
  const int total_visits = visits_per_batch + 5;

  const base::Time initial_time = base::Time::Now();

  // Setup: Add enough foreign visits that they'll need more than one batch to
  // delete.
  for (int i = 0; i < visits_per_batch + 5; ++i) {
    VisitRow foreign_visit;
    foreign_visit.visit_time = base::Time::Now();
    foreign_visit.transition = kLink;
    foreign_visit.originator_cache_guid = "originator";
    backend_->AddSyncedVisit(GURL("https://remote.url"), /*title=*/u"",
                             /*hidden=*/false, foreign_visit, absl::nullopt,
                             absl::nullopt);

    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Setup finished - verify that the visits are there.
  {
    VisitVector visits;
    backend_->db()->GetAllVisitsInRange(initial_time, base::Time::Now(),
                                        /*max_results=*/total_visits + 1,
                                        &visits);
    ASSERT_EQ(static_cast<int>(visits.size()), total_visits);
  }

  // Instruct the backend to delete foreign visits.
  backend_->DeleteAllForeignVisits();

  // Wait for the deletions to happen.
  task_environment_.RunUntilIdle();

  // Make sure that all the foreign visits got deleted.
  {
    VisitVector visits;
    backend_->db()->GetAllVisitsInRange(initial_time, base::Time::Now(),
                                        /*max_results=*/total_visits + 1,
                                        &visits);
    EXPECT_TRUE(visits.empty());
  }
}

TEST_F(HistoryBackendTest, DeleteAllForeignVisitsDoesNotDeleteFutureVisits) {
  const ui::PageTransition kLink = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CHAIN_START |
      ui::PAGE_TRANSITION_CHAIN_END);

  const base::Time initial_time = base::Time::Now();

  // Setup: Add some foreign visits.
  for (int i = 0; i < 5; ++i) {
    VisitRow foreign_visit;
    foreign_visit.visit_time = base::Time::Now();
    foreign_visit.transition = kLink;
    foreign_visit.originator_cache_guid = "originator";
    backend_->AddSyncedVisit(GURL("https://remote.url"), /*title=*/u"",
                             /*hidden=*/false, foreign_visit, absl::nullopt,
                             absl::nullopt);

    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Setup finished - verify that the visits are there.
  {
    VisitVector visits;
    backend_->db()->GetAllVisitsInRange(initial_time, base::Time::Now(),
                                        /*max_results=*/10, &visits);
    ASSERT_EQ(visits.size(), 5u);
  }

  // Instruct the backend to delete foreign visits.
  backend_->DeleteAllForeignVisits();

  // Before the actual (async) deletion happens, add some more foreign visits.
  // These should *not* be affected by the previous DeleteAllForeignVisits()
  // call!
  std::vector<VisitID> new_foreign_visit_ids;
  for (int i = 0; i < 5; ++i) {
    VisitRow foreign_visit;
    foreign_visit.visit_time = base::Time::Now();
    foreign_visit.transition = kLink;
    foreign_visit.originator_cache_guid = "originator";
    new_foreign_visit_ids.push_back(backend_->AddSyncedVisit(
        GURL("https://remote.url"), /*title=*/u"",
        /*hidden=*/false, foreign_visit, absl::nullopt, absl::nullopt));

    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Wait for the scheduled deletions to happen.
  task_environment_.RunUntilIdle();

  // Make sure that (only) the visits added after the DeleteAllForeignVisits()
  // call remain.
  {
    VisitVector visits;
    backend_->db()->GetAllVisitsInRange(initial_time, base::Time::Now(),
                                        /*max_results=*/10, &visits);
    std::vector<VisitID> remaining_visit_ids;
    for (const VisitRow& visit : visits) {
      remaining_visit_ids.push_back(visit.visit_id);
    }
    EXPECT_THAT(remaining_visit_ids,
                UnorderedElementsAreArray(new_foreign_visit_ids));
  }
}

}  // namespace history
