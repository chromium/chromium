// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "components/visitedlink/browser/partitioned_visitedlink_writer.h"
#include "components/visitedlink/browser/visitedlink_delegate.h"
#include "components/visitedlink/browser/visitedlink_event_listener.h"
#include "components/visitedlink/browser/visitedlink_writer.h"
#include "components/visitedlink/common/visitedlink.mojom.h"
#include "components/visitedlink/common/visitedlink_common.h"
#include "components/visitedlink/core/visited_link.h"
#include "components/visitedlink/renderer/visitedlink_reader.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::MockRenderProcessHost;
using content::RenderViewHostTester;

namespace content {
class SiteInstance;
}

namespace visitedlink {

namespace {

typedef std::vector<GURL> URLs;

// a nice long URL that we can append numbers to to get new URLs
const char kTestPrefix[] =
    "http://www.google.com/products/foo/index.html?id=45028640526508376&seq=";
constexpr int kTestCount = 1000;

// Returns a test URL for index |i|
GURL TestURL(int i) {
  return GURL(base::StringPrintf("%s%d", kTestPrefix, i));
}

std::vector<VisitedLinkReader*> g_readers;

class TestVisitedLinkDelegate : public VisitedLinkDelegate {
 public:
  void RebuildTable(const scoped_refptr<URLEnumerator>& enumerator) override;

  void BuildVisitedLinkTable(
      const scoped_refptr<VisitedLinkEnumerator>& enumerator) override;

  void AddURLForRebuild(const GURL& url);

  void AddVisitedLinkForRebuild(const VisitedLink& link);

 private:
  URLs rebuild_urls_;
  std::vector<VisitedLink> rebuild_links_;
  base::WeakPtrFactory<TestVisitedLinkDelegate> weak_factory_{this};
};

void TestVisitedLinkDelegate::RebuildTable(
    const scoped_refptr<URLEnumerator>& enumerator) {
  for (URLs::const_iterator itr = rebuild_urls_.begin();
       itr != rebuild_urls_.end(); ++itr)
    enumerator->OnURL(*itr);
  enumerator->OnComplete(true);
}

void TestVisitedLinkDelegate::BuildVisitedLinkTable(
    const scoped_refptr<VisitedLinkEnumerator>& enumerator) {
  for (auto it = rebuild_links_.begin(); it != rebuild_links_.end(); it++) {
    enumerator->OnVisitedLink(it->link_url, it->top_level_site,
                              it->frame_origin);
  }
  enumerator->OnVisitedLinkComplete(true);
}

void TestVisitedLinkDelegate::AddURLForRebuild(const GURL& url) {
  rebuild_urls_.push_back(url);
}

void TestVisitedLinkDelegate::AddVisitedLinkForRebuild(
    const VisitedLink& link) {
  rebuild_links_.push_back(link);
}

class TestURLIterator : public VisitedLinkWriter::URLIterator {
 public:
  explicit TestURLIterator(const URLs& urls);

  const GURL& NextURL() override;
  bool HasNextURL() const override;

 private:
  URLs::const_iterator iterator_;
  URLs::const_iterator end_;
};

TestURLIterator::TestURLIterator(const URLs& urls)
    : iterator_(urls.begin()), end_(urls.end()) {}

const GURL& TestURLIterator::NextURL() {
  return *(iterator_++);
}

bool TestURLIterator::HasNextURL() const {
  return iterator_ != end_;
}

class TestVisitedLinkIterator
    : public PartitionedVisitedLinkWriter::VisitedLinkIterator {
 public:
  explicit TestVisitedLinkIterator(const std::vector<VisitedLink>& links);

  const VisitedLink& NextVisitedLink() override;
  bool HasNextVisitedLink() const override;

 private:
  std::vector<VisitedLink>::const_iterator iterator_;
  std::vector<VisitedLink>::const_iterator end_;
};

TestVisitedLinkIterator::TestVisitedLinkIterator(
    const std::vector<VisitedLink>& links)
    : iterator_(links.begin()), end_(links.end()) {}

const VisitedLink& TestVisitedLinkIterator::NextVisitedLink() {
  return *(iterator_++);
}

bool TestVisitedLinkIterator::HasNextVisitedLink() const {
  return iterator_ != end_;
}

}  // namespace

class TrackingVisitedLinkEventListener
    : public PartitionedVisitedLinkWriter::Listener,
      public VisitedLinkWriter::Listener {
 public:
  TrackingVisitedLinkEventListener()
      : reset_count_(0),
        completely_reset_count_(0),
        add_count_(0),
        salts_update_count_(0) {}

  void NewTable(base::ReadOnlySharedMemoryRegion* table_region) override {
    if (table_region->IsValid()) {
      for (std::vector<VisitedLinkReader>::size_type i = 0;
           i < g_readers.size(); i++) {
        g_readers[i]->UpdateVisitedLinks(table_region->Duplicate());
      }
    }
  }
  void Add(VisitedLinkCommon::Fingerprint) override { add_count_++; }
  void Reset(bool invalidate_hashes) override {
    if (invalidate_hashes)
      completely_reset_count_++;
    else
      reset_count_++;
  }
  void UpdateOriginSalts() override { salts_update_count_++; }

  void SetUp() {
    reset_count_ = 0;
    add_count_ = 0;
    salts_update_count_ = 0;
  }

  int reset_count() const { return reset_count_; }
  int completely_reset_count() { return completely_reset_count_; }
  int add_count() const { return add_count_; }
  int salts_update_count() const { return salts_update_count_; }

 private:
  int reset_count_;
  int completely_reset_count_;
  int add_count_;
  int salts_update_count_;
};

class VisitedLinkTest : public testing::Test {
 protected:
  // Initializes the visited link objects. Pass in the size that you want a
  // freshly created table to be. 0 means use the default.
  //
  // |suppress_rebuild| is set when we're not testing rebuilding, see
  // the VisitedLinkWriter constructor.
  //
  // |wait_for_io_complete| wait for result of async loading.
  bool InitVisited(int initial_size,
                   bool suppress_rebuild,
                   bool wait_for_io_complete) {
    // Initialize the visited link system.
    writer_ = std::make_unique<VisitedLinkWriter>(
        new TrackingVisitedLinkEventListener(), &delegate_, true,
        suppress_rebuild, visited_file_, initial_size);
    bool result = writer_->Init();
    if (result && wait_for_io_complete) {
      // Wait for all pending file I/O to be completed.
      content::RunAllTasksUntilIdle();
    }
    return result;
  }

  // May be called multiple times (some tests will do this to clear things,
  // and TearDown will do this to make sure eveything is shiny before quitting.
  void ClearDB() {
    writer_.reset(nullptr);

    // Wait for all pending file I/O to be completed.
    content::RunAllTasksUntilIdle();
  }

  // Loads the database from disk and makes sure that the same URLs are present
  // as were generated by TestIO_Create(). This also checks the URLs with a
  // reader to make sure it reads the data properly.
  void Reload() {
    // Clean up after our caller, who may have left the database open.
    ClearDB();

    ASSERT_TRUE(InitVisited(0, true, true));

    writer_->DebugValidate();

    // check that the table has the proper number of entries
    int used_count = writer_->GetUsedCount();
    ASSERT_EQ(used_count, kTestCount);

    // Create a reader database.
    VisitedLinkReader reader;
    reader.UpdateVisitedLinks(
        writer_->mapped_table_memory().region.Duplicate());
    g_readers.push_back(&reader);

    bool found;
    for (int i = 0; i < kTestCount; i++) {
      GURL cur = TestURL(i);
      found = writer_->IsVisited(cur);
      EXPECT_TRUE(found) << "URL " << i << "not found in writer.";

      found = reader.IsVisited(cur);
      EXPECT_TRUE(found) << "URL " << i << "not found in reader.";
    }

    // test some random URL so we know that it returns false sometimes too
    found = writer_->IsVisited(GURL("http://unfound.site/"));
    ASSERT_FALSE(found);
    found = reader.IsVisited(GURL("http://unfound.site/"));
    ASSERT_FALSE(found);

    writer_->DebugValidate();

    g_readers.clear();
  }

  // testing::Test
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    history_dir_ = temp_dir_.GetPath().AppendASCII("VisitedLinkTest");
    ASSERT_TRUE(base::CreateDirectory(history_dir_));

    visited_file_ = history_dir_.Append(FILE_PATH_LITERAL("VisitedLinks"));
  }

  void TearDown() override {
    ClearDB();
    ASSERT_TRUE(temp_dir_.Delete());
  }

  base::ScopedTempDir temp_dir_;

  // Filenames for the services;
  base::FilePath history_dir_;
  base::FilePath visited_file_;

  std::unique_ptr<VisitedLinkWriter> writer_;
  TestVisitedLinkDelegate delegate_;
  content::BrowserTaskEnvironment task_environment_;
};

// This test creates and reads some databases to make sure the data is
// preserved throughout those operations.
TEST_F(VisitedLinkTest, DatabaseIO) {
  ASSERT_TRUE(InitVisited(0, true, true));

  for (int i = 0; i < kTestCount; i++)
    writer_->AddURL(TestURL(i));

  // Test that the database was written properly
  Reload();
}

// This test fills a database using AddURLs() and verifies that it can be read
// back correctly.
TEST_F(VisitedLinkTest, DatabaseIOAddURLs) {
  ASSERT_TRUE(InitVisited(0, true, true));

  constexpr int kHalfCount = kTestCount / 2;

  // Add urls in pairs of two. Simulates calls to AddURLs() after navigations
  // with redirects.
  for (int i = 0; i < (kHalfCount - 1); i += 2)
    writer_->AddURLs({TestURL(i), TestURL(i + 1)});

  // Add a big vector of URLs, exceeding kBulkOperationThreshold.
  std::vector<GURL> urls;
  static_assert(kHalfCount > VisitedLinkWriter::kBulkOperationThreshold,
                "kBulkOperationThreshold not exceeded");
  for (int i = kHalfCount; i < kTestCount; i++)
    urls.push_back(TestURL(i));
  writer_->AddURLs(urls);

  // Test that the database was written properly.
  Reload();
}

// Checks that we can delete things properly when there are collisions.
TEST_F(VisitedLinkTest, Delete) {
  static const int32_t kInitialSize = 17;
  ASSERT_TRUE(InitVisited(kInitialSize, true, true));

  // Add a cluster from 14-17 wrapping around to 0. These will all hash to the
  // same value.
  const VisitedLinkCommon::Fingerprint kFingerprint0 = kInitialSize * 0 + 14;
  const VisitedLinkCommon::Fingerprint kFingerprint1 = kInitialSize * 1 + 14;
  const VisitedLinkCommon::Fingerprint kFingerprint2 = kInitialSize * 2 + 14;
  const VisitedLinkCommon::Fingerprint kFingerprint3 = kInitialSize * 3 + 14;
  const VisitedLinkCommon::Fingerprint kFingerprint4 = kInitialSize * 4 + 14;
  ASSERT_EQ(writer_->AddFingerprint(kFingerprint0, false), 14);
  ASSERT_EQ(writer_->AddFingerprint(kFingerprint1, false), 15);
  ASSERT_EQ(writer_->AddFingerprint(kFingerprint2, false), 16);
  ASSERT_EQ(writer_->AddFingerprint(kFingerprint3, false), 0);
  ASSERT_EQ(writer_->AddFingerprint(kFingerprint4, false), 1);

  // Deleting 14 should move the next value up one slot (we do not specify an
  // order).
  EXPECT_EQ(kFingerprint3, writer_->hash_table_[0]);
  writer_->DeleteFingerprint(kFingerprint3, false);
  const VisitedLinkCommon::Fingerprint kZeroFingerprint = 0;
  EXPECT_EQ(kZeroFingerprint, writer_->hash_table_[1]);
  EXPECT_NE(kZeroFingerprint, writer_->hash_table_[0]);

  // Deleting the other four should leave the table empty.
  writer_->DeleteFingerprint(kFingerprint0, false);
  writer_->DeleteFingerprint(kFingerprint1, false);
  writer_->DeleteFingerprint(kFingerprint2, false);
  writer_->DeleteFingerprint(kFingerprint4, false);

  EXPECT_EQ(0, writer_->used_items_);
  for (int i = 0; i < kInitialSize; i++) {
    EXPECT_EQ(kZeroFingerprint, writer_->hash_table_[i])
        << "Hash table has values in it.";
  }
}

// When we delete more than kBulkOperationThreshold we trigger different
// behavior where the entire file is rewritten.
TEST_F(VisitedLinkTest, BigDelete) {
  ASSERT_TRUE(InitVisited(16381, true, true));

  // Add the base set of URLs that won't be deleted.
  // Reload() will test for these.
  for (int32_t i = 0; i < kTestCount; i++)
    writer_->AddURL(TestURL(i));

  // Add more URLs than necessary to trigger this case.
  const int kTestDeleteCount = VisitedLinkWriter::kBulkOperationThreshold + 2;
  URLs urls_to_delete;
  for (int32_t i = kTestCount; i < kTestCount + kTestDeleteCount; i++) {
    GURL url(TestURL(i));
    writer_->AddURL(url);
    urls_to_delete.push_back(url);
  }

  TestURLIterator iterator(urls_to_delete);
  writer_->DeleteURLs(&iterator);
  writer_->DebugValidate();

  Reload();
}

TEST_F(VisitedLinkTest, DeleteAll) {
  ASSERT_TRUE(InitVisited(0, true, true));

  {
    VisitedLinkReader reader;
    reader.UpdateVisitedLinks(
        writer_->mapped_table_memory().region.Duplicate());
    g_readers.push_back(&reader);

    // Add the test URLs.
    for (int i = 0; i < kTestCount; i++) {
      writer_->AddURL(TestURL(i));
      ASSERT_EQ(i + 1, writer_->GetUsedCount());
    }
    writer_->DebugValidate();

    // Make sure the reader picked up the adds.
    for (int i = 0; i < kTestCount; i++)
      EXPECT_TRUE(reader.IsVisited(TestURL(i)));

    // Clear the table and make sure the reader picked it up.
    writer_->DeleteAllURLs();
    EXPECT_EQ(0, writer_->GetUsedCount());
    for (int i = 0; i < kTestCount; i++) {
      EXPECT_FALSE(writer_->IsVisited(TestURL(i)));
      EXPECT_FALSE(reader.IsVisited(TestURL(i)));
    }

    // Close the database.
    g_readers.clear();
    ClearDB();
  }

  // Reopen and validate.
  ASSERT_TRUE(InitVisited(0, true, true));
  writer_->DebugValidate();
  EXPECT_EQ(0, writer_->GetUsedCount());
  for (int i = 0; i < kTestCount; i++)
    EXPECT_FALSE(writer_->IsVisited(TestURL(i)));
}

// This tests that the writer correctly resizes its tables when it gets too
// full, notifies its readers of the change, and updates the disk.
TEST_F(VisitedLinkTest, Resizing) {
  // Create a very small database.
  const int32_t initial_size = 17;
  ASSERT_TRUE(InitVisited(initial_size, true, true));

  // ...and a reader
  VisitedLinkReader reader;
  reader.UpdateVisitedLinks(writer_->mapped_table_memory().region.Duplicate());
  g_readers.push_back(&reader);

  int32_t used_count = writer_->GetUsedCount();
  ASSERT_EQ(used_count, 0);

  for (int i = 0; i < kTestCount; i++) {
    writer_->AddURL(TestURL(i));
    used_count = writer_->GetUsedCount();
    ASSERT_EQ(i + 1, used_count);
  }

  // Verify that the table got resized sufficiently.
  int32_t table_size;
  VisitedLinkCommon::Fingerprint* table;
  writer_->GetUsageStatistics(&table_size, &table);
  used_count = writer_->GetUsedCount();
  ASSERT_GT(table_size, used_count);
  ASSERT_EQ(used_count, kTestCount)
      << "table count doesn't match the # of things we added";

  // Verify that the reader got the resize message and has the same
  // table information.
  int32_t child_table_size;
  VisitedLinkCommon::Fingerprint* child_table;
  reader.GetUsageStatistics(&child_table_size, &child_table);
  ASSERT_EQ(table_size, child_table_size);
  for (int32_t i = 0; i < table_size; i++) {
    ASSERT_EQ(table[i], child_table[i]);
  }

  writer_->DebugValidate();
  g_readers.clear();

  // This tests that the file is written correctly by reading it in using
  // a new database.
  Reload();
}

// Tests that if the database doesn't exist, it will be rebuilt from history.
TEST_F(VisitedLinkTest, Rebuild) {
  // Add half of our URLs to history. This needs to be done before we
  // initialize the visited link DB.
  int history_count = kTestCount / 2;
  for (int i = 0; i < history_count; i++)
    delegate_.AddURLForRebuild(TestURL(i));

  // Initialize the visited link DB. Since the visited links file doesn't exist
  // and we don't suppress history rebuilding, this will load from history.
  ASSERT_TRUE(InitVisited(0, false, false));

  // While the table is rebuilding, add the rest of the URLs to the visited
  // link system. This isn't guaranteed to happen during the rebuild, so we
  // can't be 100% sure we're testing the right thing, but in practice is.
  // All the adds above will generally take some time queuing up on the
  // history thread, and it will take a while to catch up to actually
  // processing the rebuild that has queued behind it. We will generally
  // finish adding all of the URLs before it has even found the first URL.
  for (int i = history_count; i < kTestCount; i++)
    writer_->AddURL(TestURL(i));

  // Add one more and then delete it.
  writer_->AddURL(TestURL(kTestCount));
  URLs urls_to_delete;
  urls_to_delete.push_back(TestURL(kTestCount));
  TestURLIterator iterator(urls_to_delete);
  writer_->DeleteURLs(&iterator);

  // Wait for the rebuild to complete. The task will terminate the message
  // loop when the rebuild is done. There's no chance that the rebuild will
  // complete before we set the task because the rebuild completion message
  // is posted to the message loop; until we Run() it, rebuild can not
  // complete.
  base::RunLoop run_loop;
  writer_->set_rebuild_complete_task(run_loop.QuitClosure());
  run_loop.Run();

  // Test that all URLs were written to the database properly.
  Reload();

  // Make sure the extra one was *not* written (Reload won't test this).
  EXPECT_FALSE(writer_->IsVisited(TestURL(kTestCount)));
}

// Test that importing a large number of URLs will work
TEST_F(VisitedLinkTest, BigImport) {
  ASSERT_TRUE(InitVisited(0, false, false));

  // Before the table rebuilds, add a large number of URLs
  int total_count = VisitedLinkWriter::kDefaultTableSize + 10;
  for (int i = 0; i < total_count; i++)
    writer_->AddURL(TestURL(i));

  // Wait for the rebuild to complete.
  base::RunLoop run_loop;
  writer_->set_rebuild_complete_task(run_loop.QuitClosure());
  run_loop.Run();

  // Ensure that the right number of URLs are present
  int used_count = writer_->GetUsedCount();
  ASSERT_EQ(used_count, total_count);
}

TEST_F(VisitedLinkTest, Listener) {
  ASSERT_TRUE(InitVisited(0, true, true));

  TrackingVisitedLinkEventListener* listener =
      static_cast<TrackingVisitedLinkEventListener*>(writer_->GetListener());

  // Verify that VisitedLinkWriter::Listener::Reset(true) was never called when
  // the table was created.
  EXPECT_EQ(0, listener->completely_reset_count());

  // Add test URLs.
  for (int i = 0; i < kTestCount; i++) {
    writer_->AddURL(TestURL(i));
    ASSERT_EQ(i + 1, writer_->GetUsedCount());
  }

  // Delete an URL.
  URLs urls_to_delete;
  urls_to_delete.push_back(TestURL(0));
  TestURLIterator iterator(urls_to_delete);
  writer_->DeleteURLs(&iterator);

  // ... and all of the remaining ones.
  writer_->DeleteAllURLs();

  // Verify that VisitedLinkWriter::Listener::Add was called for each added URL.
  EXPECT_EQ(kTestCount, listener->add_count());
  // Verify that VisitedLinkWriter::Listener::Reset was called both when one and
  // all URLs are deleted.
  EXPECT_EQ(2, listener->reset_count());

  ClearDB();

  ASSERT_TRUE(InitVisited(0, true, true));

  listener =
      static_cast<TrackingVisitedLinkEventListener*>(writer_->GetListener());
  // Verify that VisitedLinkWriter::Listener::Reset(true) was called when the
  // table was loaded.
  EXPECT_EQ(1, listener->completely_reset_count());
}

TEST_F(VisitedLinkTest, HashRangeWraparound) {
  ASSERT_TRUE(InitVisited(0, true, true));

  // Create two fingerprints that, when added, will create a wraparound hash
  // range.
  const VisitedLinkCommon::Fingerprint kFingerprint0 =
      writer_->DefaultTableSize() - 1;
  const VisitedLinkCommon::Fingerprint kFingerprint1 = kFingerprint0 + 1;

  // Add the two fingerprints.
  const VisitedLinkCommon::Hash hash0 =
      writer_->AddFingerprint(kFingerprint0, false);
  const VisitedLinkCommon::Hash hash1 =
      writer_->AddFingerprint(kFingerprint1, false);

  // Verify the hashes form a range that wraps around.
  EXPECT_EQ(hash0, VisitedLinkCommon::Hash(writer_->DefaultTableSize() - 1));
  EXPECT_EQ(hash1, 0);

  // Write the database to file.
  writer_->WriteUsedItemCountToFile();
  writer_->WriteHashRangeToFile(hash0, hash1);

  // Close and reopen the database.
  ClearDB();
  ASSERT_TRUE(InitVisited(0, true, true));

  // Verify database contents.
  ASSERT_EQ(writer_->GetUsedCount(), 2);
  ASSERT_TRUE(writer_->IsVisited(kFingerprint0));
  ASSERT_TRUE(writer_->IsVisited(kFingerprint1));
}

TEST_F(VisitedLinkTest, ResizeErrorHandling) {
  // Create a small database.
  const int32_t initial_size = 17;
  ASSERT_TRUE(InitVisited(initial_size, true, true));

  // Add test URL.
  GURL url = TestURL(0);
  writer_->AddURL(url);

  // Simulate shared memory allocation failure, causing CreateURLTable() to
  // fail.
  VisitedLinkWriter::fail_table_creation_for_testing_ = true;

  // Expect resize to fail silently.
  const int32_t new_size = 23;
  writer_->ResizeTable(new_size);

  // Restore global state for subsequent tests.
  VisitedLinkWriter::fail_table_creation_for_testing_ = false;

  // Verify contents.
  ASSERT_EQ(writer_->GetUsedCount(), 1);
  ASSERT_TRUE(writer_->IsVisited(url));
}

enum TestMode {
  kPartitionedNoSelfLinks,
  kPartitionedWithSelfLinks,
  kPartitionedBothEnabled
};

class PartitionedVisitedLinkTest
    : public testing::Test,
      public ::testing::WithParamInterface<TestMode> {
 public:
  PartitionedVisitedLinkTest() {
    switch (GetParam()) {
      case TestMode::kPartitionedNoSelfLinks:
        scoped_feature_list_.InitWithFeatures(
            {blink::features::kPartitionVisitedLinkDatabase},
            {blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks});
        break;
      case TestMode::kPartitionedWithSelfLinks:
        scoped_feature_list_.InitWithFeatures(
            {blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks},
            {blink::features::kPartitionVisitedLinkDatabase});
        break;
      case TestMode::kPartitionedBothEnabled:
        scoped_feature_list_.InitWithFeatures(
            {blink::features::kPartitionVisitedLinkDatabase,
             blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks},
            {});
        break;
    }
  }

 protected:
  // Initializes the partitioned hashtable. Pass in the size that you want a
  // freshly created table to be. 0 means use the default. Tests may choose to
  // suppress building from the VisitedLinkDatabase, which will result in an
  // initialized but empty hashtable.
  bool InitVisited(bool suppress_build, int initial_size) {
    // Initialize the visited link system.
    partitioned_writer_ = std::make_unique<PartitionedVisitedLinkWriter>(
        std::make_unique<TrackingVisitedLinkEventListener>(), &delegate_,
        suppress_build, initial_size);
    bool result = partitioned_writer_->Init();
    if (!suppress_build && result) {
      // Wait for the build to complete on the DB thread. The task will
      // terminate the loop when the build is done.
      base::RunLoop run_loop;
      partitioned_writer_->set_build_complete_task(run_loop.QuitClosure());
      run_loop.Run();
    }
    return result;
  }

  void TearDown() override { g_readers.clear(); }

  bool are_self_links_enabled() {
    return GetParam() == TestMode::kPartitionedWithSelfLinks ||
           (GetParam() == TestMode::kPartitionedBothEnabled);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<PartitionedVisitedLinkWriter> partitioned_writer_;
  TestVisitedLinkDelegate delegate_;
  content::BrowserTaskEnvironment task_environment_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PartitionedVisitedLinkTest,
                         testing::Values(TestMode::kPartitionedNoSelfLinks,
                                         TestMode::kPartitionedWithSelfLinks,
                                         TestMode::kPartitionedBothEnabled));

TEST_P(PartitionedVisitedLinkTest, BuildPartitionedTable) {
  // Add half of our links to history. This needs to be done before we
  // initialize the visited link hashtable.
  int history_count = kTestCount / 2;
  for (int i = 0; i < history_count; i++) {
    VisitedLink link = {TestURL(i), net::SchemefulSite(TestURL(i)),
                        url::Origin::Create(TestURL(i))};
    delegate_.AddVisitedLinkForRebuild(link);
  }

  // Initialize the visited link hashtable. This will load from history.
  ASSERT_TRUE(InitVisited(false, 0));

  // While the table is building, add the rest of the URLs to the visited
  // link system. This isn't guaranteed to happen during the build, so we
  // can't be 100% sure we're testing the right thing, but in practice is.
  // All the adds above will generally take some time queuing up on the
  // history thread, and it will take a while to catch up to actually
  // processing the rebuild that has queued behind it.
  for (int i = history_count; i < kTestCount - 1; i++) {
    VisitedLink history_link = {TestURL(i), net::SchemefulSite(TestURL(i)),
                                url::Origin::Create(TestURL(i))};
    partitioned_writer_->AddVisitedLink(history_link);
  }

  // Add the last visited link to the hashtable once build has completed.
  const GURL last_url = TestURL(kTestCount - 1);
  const VisitedLink last_link = {last_url, net::SchemefulSite(last_url),
                                 url::Origin::Create(last_url)};
  partitioned_writer_->AddVisitedLink(last_link);

  bool found;
  for (int i = 0; i < kTestCount; i++) {
    GURL cur = TestURL(i);
    std::optional<uint64_t> salt =
        partitioned_writer_->GetOrAddOriginSalt(url::Origin::Create(cur));
    ASSERT_NE(salt, std::nullopt);
    found = partitioned_writer_->IsVisited(
        cur, net::SchemefulSite(cur), url::Origin::Create(cur), salt.value());
    EXPECT_TRUE(found) << "Partitioned link " << i << "not found in writer.";
  }
}

TEST_P(PartitionedVisitedLinkTest, BuildPartitionedTableWithSelfLinks) {
  // Add half of our links to history. This needs to be done before we
  // initialize the visited link hashtable.
  const net::SchemefulSite& top_level_site =
      net::SchemefulSite(GURL("https://example.com"));
  const url::Origin& frame_origin =
      url::Origin::Create(GURL("https://example.com"));
  int history_count = kTestCount / 2;
  for (int i = 0; i < history_count; i++) {
    VisitedLink link = {TestURL(i), top_level_site, frame_origin};
    delegate_.AddVisitedLinkForRebuild(link);
    if (are_self_links_enabled()) {
      // Because this test mocks out the VisitedLinkDatabase, we must add our
      // own self-links here. However, any calls to
      // `partitioned_writer->AddVisitedLink()` have full fidelity and we expect
      // them to correctly add self-links to the partitioned hashtable.
      VisitedLink self_link = {TestURL(i), net::SchemefulSite(TestURL(i)),
                               url::Origin::Create(TestURL(i))};
      delegate_.AddVisitedLinkForRebuild(self_link);
    }
  }

  // Initialize the visited link hashtable. This will load from history.
  ASSERT_TRUE(InitVisited(false, 0));

  // While the table is building, add the rest of the URLs to the visited
  // link system. This isn't guaranteed to happen during the build, so we
  // can't be 100% sure we're testing the right thing, but in practice is.
  // All the adds above will generally take some time queuing up on the
  // history thread, and it will take a while to catch up to actually
  // processing the rebuild that has queued behind it.
  for (int i = history_count; i < kTestCount - 1; i++) {
    VisitedLink history_link = {TestURL(i), top_level_site, frame_origin};
    partitioned_writer_->AddVisitedLink(history_link);
  }

  // Add the last visited link to the hashtable once build has completed.
  const GURL last_url = TestURL(kTestCount - 1);
  const VisitedLink last_link = {last_url, top_level_site, frame_origin};
  partitioned_writer_->AddVisitedLink(last_link);

  bool found, self_link_found;
  for (int i = 0; i < kTestCount; i++) {
    // Ensure that every link we added is "visited" (i.e. can be found in the
    // partitioned hashtable).
    GURL cur = TestURL(i);
    std::optional<uint64_t> salt =
        partitioned_writer_->GetOrAddOriginSalt(frame_origin);
    ASSERT_NE(salt, std::nullopt);
    found = partitioned_writer_->IsVisited(cur, top_level_site, frame_origin,
                                           salt.value());
    EXPECT_TRUE(found) << "Partitioned link " << i << "not found in writer.";

    // Ensure that when self-links are enabled, every link we added also has a
    // self-link counterpart which is "visited" (i.e. can be found in the
    // partitioned hashtable).
    std::optional<uint64_t> self_link_salt =
        partitioned_writer_->GetOrAddOriginSalt(url::Origin::Create(cur));
    ASSERT_NE(self_link_salt, std::nullopt);
    self_link_found = partitioned_writer_->IsVisited(
        cur, net::SchemefulSite(cur), url::Origin::Create(cur),
        self_link_salt.value());
    EXPECT_EQ(self_link_found, are_self_links_enabled())
        << "Partitioned self-link " << i << "not found in writer.";
  }
}

TEST_P(PartitionedVisitedLinkTest, NotVisitedEmptyDB) {
  ASSERT_TRUE(InitVisited(true, 0));
  ASSERT_EQ(partitioned_writer_->GetUsedCount(), 0);

  // Create a reader and copy the hashtable.
  VisitedLinkReader reader;
  reader.UpdateVisitedLinks(
      partitioned_writer_->GetMappedTableMemoryForTesting().region.Duplicate());
  g_readers.push_back(&reader);

  // Test a visited link that has not been added to the VisitedLinkDatabase.
  const url::Origin origin = url::Origin::Create(TestURL(0));
  const VisitedLink link_0 = {TestURL(0), net::SchemefulSite(TestURL(0)),
                              origin};
  const std::optional<uint64_t> salt =
      partitioned_writer_->GetOrAddOriginSalt(origin);
  ASSERT_TRUE(salt.has_value());
  // Ensure that we do not find this link in the writer's or reader's
  // hashtables.
  EXPECT_FALSE(partitioned_writer_->IsVisited(link_0, salt.value()));
  EXPECT_FALSE(reader.IsVisited(link_0, salt.value()));
}

TEST_P(PartitionedVisitedLinkTest, NotVisitedSomeFieldsMatch) {
  // Add a link to the mock VisitedLinkDatabase. This needs to be done before
  // we initialize the visited link hashtable.
  const GURL url_0 = GURL("http://example.com");
  const net::SchemefulSite site_0 = net::SchemefulSite(url_0);
  const url::Origin origin_0 = url::Origin::Create(url_0);
  const VisitedLink link_0 = {url_0, site_0, origin_0};
  delegate_.AddVisitedLinkForRebuild(link_0);

  // Initialize the visited link hashtable. This will load from history.
  ASSERT_TRUE(InitVisited(false, 0));
  ASSERT_EQ(partitioned_writer_->GetUsedCount(), 1);

  // Create a reader and copy the hashtable.
  VisitedLinkReader reader;
  reader.UpdateVisitedLinks(
      partitioned_writer_->GetMappedTableMemoryForTesting().region.Duplicate());
  g_readers.push_back(&reader);

  // Test visited links that have not been added to the VisitedLinkDatabase.
  const std::optional<uint64_t> salt_0 =
      partitioned_writer_->GetOrAddOriginSalt(origin_0);
  ASSERT_TRUE(salt_0.has_value());

  // Prepare cross-site, cross-origin values.
  const GURL url_1 = GURL("https://google.com");
  const net::SchemefulSite site_1 = net::SchemefulSite(url_1);
  const url::Origin origin_1 = url::Origin::Create(TestURL(1));
  const std::optional<uint64_t> salt_1 =
      partitioned_writer_->GetOrAddOriginSalt(origin_1);
  ASSERT_TRUE(salt_1.has_value());

  // Test visited links that have not been added to the VisitedLinkDatabase.
  std::map<VisitedLink, std::optional<uint64_t>> links = {
      // Create a link where no fields match the added partition key.
      {{url_1, site_1, origin_1}, salt_1},
      // Create a link where only link_url matches.
      {{url_0, site_1, origin_1}, salt_1},
      // Create a link where only top_level_site matches.
      {{url_1, site_0, origin_1}, salt_1},
      // Create a link where only the frame_origin matches.
      {{url_1, site_1, origin_0}, salt_0},
      // Create a link where link_url and top_level_site match.
      {{url_0, site_0, origin_1}, salt_1},
      // Create a link where link_url and frame_origin match.
      {{url_0, site_1, origin_0}, salt_0},
      // Create a link where top_level_site and frame_origin match.
      {{url_1, site_0, origin_0}, salt_0},
      // Create a link where the salt does not match frame_origin.
      {{url_0, site_0, origin_0}, salt_1}};

  for (const auto& [cur_link, cur_salt] : links) {
    // Ensure that we do not find this link in the writer's or reader's
    // hashtables.
    EXPECT_FALSE(partitioned_writer_->IsVisited(cur_link, cur_salt.value()));
    EXPECT_FALSE(reader.IsVisited(cur_link, cur_salt.value()));
  }

  // Ensure that we can find the original link in the writer's and reader's
  // hashtables when partitioning is enabled.
  EXPECT_TRUE(partitioned_writer_->IsVisited(link_0, salt_0.value()));
  EXPECT_TRUE(reader.IsVisited(link_0, salt_0.value()));
}

TEST_P(PartitionedVisitedLinkTest, AddAndDelete) {
  ASSERT_TRUE(InitVisited(true, 0));

  // Add a single link and ensure it is in the hashtable.
  VisitedLink link_0 = {TestURL(0), net::SchemefulSite(TestURL(0)),
                        url::Origin::Create(TestURL(0))};
  partitioned_writer_->AddVisitedLink(link_0);
  std::optional<uint64_t> salt_0 =
      partitioned_writer_->GetOrAddOriginSalt(link_0.frame_origin);
  ASSERT_NE(salt_0, std::nullopt);
  EXPECT_TRUE(partitioned_writer_->IsVisited(link_0, salt_0.value()));

  // Delete that link and ensure it isn't in the hashtable.
  std::vector<VisitedLink> links_to_delete = {link_0};
  TestVisitedLinkIterator iterator(links_to_delete);
  partitioned_writer_->DeleteVisitedLinks(&iterator);
  EXPECT_FALSE(partitioned_writer_->IsVisited(link_0, salt_0.value()));
}

TEST_P(PartitionedVisitedLinkTest, AddAndDeleteSelfLink) {
  ASSERT_TRUE(InitVisited(true, 0));

  // Add a single link and ensure it is in the hashtable.
  const GURL cross_site_url = GURL("https://self-link-test.com");
  VisitedLink link = {TestURL(0), net::SchemefulSite(cross_site_url),
                      url::Origin::Create(cross_site_url)};
  partitioned_writer_->AddVisitedLink(link);
  std::optional<uint64_t> salt =
      partitioned_writer_->GetOrAddOriginSalt(link.frame_origin);
  ASSERT_NE(salt, std::nullopt);
  EXPECT_TRUE(partitioned_writer_->IsVisited(link, salt.value()));

  // When self-links are enabled, ensure that the self-link counterpart to the
  // above link is also in the hashtable.
  VisitedLink self_link = {TestURL(0), net::SchemefulSite(TestURL(0)),
                           url::Origin::Create(TestURL(0))};
  std::optional<uint64_t> self_link_salt =
      partitioned_writer_->GetOrAddOriginSalt(self_link.frame_origin);
  ASSERT_NE(self_link_salt, std::nullopt);
  EXPECT_EQ(partitioned_writer_->IsVisited(self_link, self_link_salt.value()),
            are_self_links_enabled());

  // Request to delete both links and ensure they aren't in the hashtable. (If
  // self-links are not enabled, requesting to delete a link which has never
  // been added to the hashtable in the first place is a no-op).
  std::vector<VisitedLink> links_to_delete = {link, self_link};
  TestVisitedLinkIterator iterator(links_to_delete);
  partitioned_writer_->DeleteVisitedLinks(&iterator);
  EXPECT_FALSE(partitioned_writer_->IsVisited(link, salt.value()));
  EXPECT_FALSE(
      partitioned_writer_->IsVisited(self_link, self_link_salt.value()));
}

TEST_P(PartitionedVisitedLinkTest, DoesNotAddSubframeSelfLinks) {
  ASSERT_TRUE(InitVisited(true, 0));

  // Add a single link with cross-origin top-level and frame origins.
  // Ensure it is in the hashtable.
  const net::SchemefulSite top_level_site =
      net::SchemefulSite(GURL("https://toplevel.com"));
  const url::Origin frame_origin =
      url::Origin::Create(GURL("https://subframe.com"));
  VisitedLink link = {TestURL(0), top_level_site, frame_origin};
  partitioned_writer_->AddVisitedLink(link);
  std::optional<uint64_t> salt =
      partitioned_writer_->GetOrAddOriginSalt(link.frame_origin);
  ASSERT_NE(salt, std::nullopt);
  EXPECT_TRUE(partitioned_writer_->IsVisited(link, salt.value()));

  // We do not support self-links in cross-origin subframes, even when the
  // self-links feature flag is enabled. Ensure that the self-link is NOT added
  // to the hashtable.
  VisitedLink self_link = {TestURL(0), net::SchemefulSite(TestURL(0)),
                           url::Origin::Create(TestURL(0))};
  std::optional<uint64_t> self_link_salt =
      partitioned_writer_->GetOrAddOriginSalt(self_link.frame_origin);
  ASSERT_NE(self_link_salt, std::nullopt);
  EXPECT_FALSE(
      partitioned_writer_->IsVisited(self_link, self_link_salt.value()));
}

// Checks that we can delete things properly when there are collisions.
TEST_P(PartitionedVisitedLinkTest, DeleteWithCollisions) {
  static const int32_t kInitialSize = 17;
  ASSERT_TRUE(InitVisited(true, kInitialSize));

  // Add a cluster from 14-17 wrapping around to 0. These will all hash to the
  // same value.
  const VisitedLinkCommon::Fingerprint kFingerprint0 = kInitialSize * 0 + 14;
  const VisitedLinkCommon::Fingerprint kFingerprint1 = kInitialSize * 1 + 14;
  const VisitedLinkCommon::Fingerprint kFingerprint2 = kInitialSize * 2 + 14;
  const VisitedLinkCommon::Fingerprint kFingerprint3 = kInitialSize * 3 + 14;
  const VisitedLinkCommon::Fingerprint kFingerprint4 = kInitialSize * 4 + 14;

  ASSERT_EQ(partitioned_writer_->AddFingerprint(kFingerprint0, false), 14);
  ASSERT_EQ(partitioned_writer_->AddFingerprint(kFingerprint1, false), 15);
  ASSERT_EQ(partitioned_writer_->AddFingerprint(kFingerprint2, false), 16);
  ASSERT_EQ(partitioned_writer_->AddFingerprint(kFingerprint3, false), 0);
  ASSERT_EQ(partitioned_writer_->AddFingerprint(kFingerprint4, false), 1);

  // Deleting 14 should move the next value up one slot (we do not specify an
  // order).
  const VisitedLinkCommon::Fingerprint kZeroFingerprint = 0;
  EXPECT_EQ(kFingerprint3, partitioned_writer_->hash_table_[0]);
  partitioned_writer_->DeleteFingerprint(kFingerprint3);
  EXPECT_EQ(kZeroFingerprint, partitioned_writer_->hash_table_[1]);
  EXPECT_NE(kZeroFingerprint, partitioned_writer_->hash_table_[0]);

  // Deleting the other four should leave the table empty.
  partitioned_writer_->DeleteFingerprint(kFingerprint0);
  partitioned_writer_->DeleteFingerprint(kFingerprint1);
  partitioned_writer_->DeleteFingerprint(kFingerprint2);
  partitioned_writer_->DeleteFingerprint(kFingerprint4);

  EXPECT_EQ(0, partitioned_writer_->used_items_);
  for (int i = 0; i < kInitialSize; i++) {
    EXPECT_EQ(kZeroFingerprint, partitioned_writer_->hash_table_[i])
        << "Hash table has values in it.";
  }
}

TEST_P(PartitionedVisitedLinkTest, DeleteAll) {
  ASSERT_TRUE(InitVisited(true, 0));

  // Create a reader instance and populate its hashtable.
  {
    VisitedLinkReader reader;
    reader.UpdateVisitedLinks(
        partitioned_writer_->GetMappedTableMemoryForTesting()
            .region.Duplicate());
    g_readers.push_back(&reader);

    // Add the test links.
    for (int i = 0; i < kTestCount; i++) {
      const VisitedLink link = {TestURL(i), net::SchemefulSite(TestURL(i)),
                                url::Origin::Create(TestURL(i))};
      partitioned_writer_->AddVisitedLink(link);
      ASSERT_EQ(i + 1, partitioned_writer_->GetUsedCount());
    }
    partitioned_writer_->DebugValidate();

    // Make sure the reader received the added links via IPC.
    for (int i = 0; i < kTestCount; i++) {
      const url::Origin origin = url::Origin::Create(TestURL(i));
      const VisitedLink link = {TestURL(i), net::SchemefulSite(TestURL(i)),
                                origin};
      const std::optional<uint64_t> salt =
          partitioned_writer_->GetOrAddOriginSalt(origin);
      ASSERT_TRUE(salt.has_value());
      EXPECT_TRUE(reader.IsVisited(link, salt.value()));
    }

    // Clear the table and make sure the reader clears its hashtable as well.
    partitioned_writer_->DeleteAllVisitedLinks();
    EXPECT_EQ(0, partitioned_writer_->GetUsedCount());
    for (int i = 0; i < kTestCount; i++) {
      const url::Origin origin = url::Origin::Create(TestURL(i));
      const VisitedLink link = {TestURL(i), net::SchemefulSite(TestURL(i)),
                                origin};
      const std::optional<uint64_t> salt =
          partitioned_writer_->GetOrAddOriginSalt(origin);
      ASSERT_TRUE(salt.has_value());
      // Ensure that neither the writer nor the reader contain this link now.
      EXPECT_FALSE(partitioned_writer_->IsVisited(link, salt.value()));
      EXPECT_FALSE(reader.IsVisited(link, salt.value()));
    }
  }
}

TEST_P(PartitionedVisitedLinkTest, Resizing) {
  // Create a very small database.
  const int32_t initial_size = 17;
  ASSERT_TRUE(InitVisited(true, initial_size));
  ASSERT_EQ(partitioned_writer_->GetUsedCount(), 0);

  // Create a reader to notify of the resizing.
  VisitedLinkReader reader;
  reader.UpdateVisitedLinks(
      partitioned_writer_->GetMappedTableMemoryForTesting().region.Duplicate());
  g_readers.push_back(&reader);

  // Populate the very small database.
  for (int i = 0; i < kTestCount; i++) {
    VisitedLink link = {TestURL(i), net::SchemefulSite(TestURL(i)),
                        url::Origin::Create(TestURL(i))};
    partitioned_writer_->AddVisitedLink(link);
    ASSERT_EQ(i + 1, partitioned_writer_->GetUsedCount());
  }

  // Verify that the table got resized sufficiently.
  int32_t table_size;
  VisitedLinkCommon::Fingerprint* table;
  partitioned_writer_->GetUsageStatistics(&table_size, &table);
  const int32_t used_count = partitioned_writer_->GetUsedCount();
  ASSERT_GT(table_size, used_count);
  ASSERT_EQ(used_count, kTestCount)
      << "table count doesn't match the # of things we added";

  // Verify that the reader got the resize message and has the same
  // table information.
  int32_t reader_table_size;
  VisitedLinkCommon::Fingerprint* reader_table;
  reader.GetUsageStatistics(&reader_table_size, &reader_table);
  ASSERT_EQ(table_size, reader_table_size);
  for (int32_t i = 0; i < table_size; i++) {
    ASSERT_EQ(table[i], reader_table[i]);
  }
}

TEST_P(PartitionedVisitedLinkTest, HashRangeWraparound) {
  ASSERT_TRUE(InitVisited(true, 0));

  // Create two fingerprints that, when added, will create a wraparound hash
  // range.
  const VisitedLinkCommon::Fingerprint kFingerprint0 =
      partitioned_writer_->DefaultTableSize() - 1;
  const VisitedLinkCommon::Fingerprint kFingerprint1 = kFingerprint0 + 1;

  // Add the two fingerprints.
  const VisitedLinkCommon::Hash hash0 =
      partitioned_writer_->AddFingerprint(kFingerprint0, false);
  const VisitedLinkCommon::Hash hash1 =
      partitioned_writer_->AddFingerprint(kFingerprint1, false);

  // Verify the hashes form a range that wraps around.
  EXPECT_EQ(hash0, VisitedLinkCommon::Hash(
                       partitioned_writer_->DefaultTableSize() - 1));
  EXPECT_EQ(hash1, 0);
}

TEST_P(PartitionedVisitedLinkTest, Listener) {
  // Create an initialized but empty hashtable.
  ASSERT_TRUE(InitVisited(false, 0));

  // Obtain the Listener and cast it into our test-friendly subclass.
  TrackingVisitedLinkEventListener* listener =
      static_cast<TrackingVisitedLinkEventListener*>(
          partitioned_writer_->GetListener());

  // Verify that PartitionedVisitedLinkWriter::Listener::Reset(false) was called
  // when the hashtable was created.
  EXPECT_EQ(1, listener->reset_count());
  // Verify that PartitionedVisitedLinkWriter::Listener::UpdateOriginSalts() was
  // called when the hashtable finished building.
  EXPECT_EQ(1, listener->salts_update_count());

  // Add test links to the hashtable.
  for (int i = 0; i < kTestCount; i++) {
    VisitedLink link = {TestURL(i), net::SchemefulSite(TestURL(i)),
                        url::Origin::Create(TestURL(i))};
    partitioned_writer_->AddVisitedLink(link);
    ASSERT_EQ(i + 1, partitioned_writer_->GetUsedCount());
  }
  // Verify that PartitionedVisitedLinkWriter::Listener::Add was called for each
  // added link.
  EXPECT_EQ(kTestCount, listener->add_count());

  // Delete the first test link.
  VisitedLink link_0 = {TestURL(0), net::SchemefulSite(TestURL(0)),
                        url::Origin::Create(TestURL(0))};
  std::vector<VisitedLink> links_to_delete = {link_0};
  TestVisitedLinkIterator iterator(links_to_delete);
  partitioned_writer_->DeleteVisitedLinks(&iterator);
  // Verify that PartitionedVisitedLinkWriter::Listener::Reset(false) was called
  // when the single link was deleted.
  EXPECT_EQ(2, listener->reset_count());

  // ... and all of the remaining ones.
  partitioned_writer_->DeleteAllVisitedLinks();
  // Verify that PartitionedVisitedLinkWriter::Listener::Reset(false) was called
  // when all the links were deleted.
  EXPECT_EQ(3, listener->reset_count());
}

class VisitCountingContext : public mojom::VisitedLinkNotificationSink {
 public:
  VisitCountingContext()
      : add_count_(0),
        add_event_count_(0),
        reset_event_count_(0),
        completely_reset_event_count_(0),
        new_table_count_(0),
        salts_update_event_count_(0) {}

  VisitCountingContext(const VisitCountingContext&) = delete;
  VisitCountingContext& operator=(const VisitCountingContext&) = delete;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Add(this,
                  mojo::PendingReceiver<mojom::VisitedLinkNotificationSink>(
                      std::move(handle)));
  }

  void WaitForUpdate() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForNoUpdate() { receiver_.FlushForTesting(); }

  mojo::ReceiverSet<mojom::VisitedLinkNotificationSink>& binding() {
    return receiver_;
  }

  void NotifyUpdate() {
    if (!quit_closure_.is_null())
      std::move(quit_closure_).Run();
  }

  void UpdateVisitedLinks(
      base::ReadOnlySharedMemoryRegion table_region) override {
    new_table_count_++;
    NotifyUpdate();
  }

  void AddVisitedLinks(const std::vector<uint64_t>& link_hashes) override {
    add_count_ += link_hashes.size();
    add_event_count_++;
    NotifyUpdate();
  }

  void ResetVisitedLinks(bool invalidate_cached_hashes) override {
    if (invalidate_cached_hashes) {
      completely_reset_event_count_++;
    } else {
      reset_event_count_++;
    }
    NotifyUpdate();
  }

  void UpdateOriginSalts(
      const base::flat_map<url::Origin, uint64_t>& origin_salts) override {
    salts_update_event_count_++;
    for (const auto& [origin, salt] : origin_salts) {
      salts_[origin] = salt;
    }
    NotifyUpdate();
  }

  int add_count() const { return add_count_; }
  int add_event_count() const { return add_event_count_; }
  int reset_event_count() const { return reset_event_count_; }
  int completely_reset_event_count() const {
    return completely_reset_event_count_;
  }
  int new_table_count() const { return new_table_count_; }
  int salts_update_event_count() const { return salts_update_event_count_; }
  std::map<url::Origin, uint64_t> salts() const { return salts_; }

 private:
  int add_count_;
  int add_event_count_;
  int reset_event_count_;
  int completely_reset_event_count_;
  int new_table_count_;
  int salts_update_event_count_;
  std::map<url::Origin, uint64_t> salts_;

  base::OnceClosure quit_closure_;
  mojo::ReceiverSet<mojom::VisitedLinkNotificationSink> receiver_;
};

// Stub out as little as possible, borrowing from RenderProcessHost.
class VisitRelayingRenderProcessHost : public MockRenderProcessHost {
 public:
  explicit VisitRelayingRenderProcessHost(
      content::BrowserContext* browser_context,
      VisitCountingContext* context)
      : MockRenderProcessHost(browser_context) {
    OverrideBinderForTesting(mojom::VisitedLinkNotificationSink::Name_,
                             base::BindRepeating(&VisitCountingContext::Bind,
                                                 base::Unretained(context)));
  }

  VisitRelayingRenderProcessHost(const VisitRelayingRenderProcessHost&) =
      delete;
  VisitRelayingRenderProcessHost& operator=(
      const VisitRelayingRenderProcessHost&) = delete;
};

class VisitedLinkRenderProcessHostFactory
    : public content::RenderProcessHostFactory {
 public:
  VisitedLinkRenderProcessHostFactory() : context_(new VisitCountingContext) {}

  VisitedLinkRenderProcessHostFactory(
      const VisitedLinkRenderProcessHostFactory&) = delete;
  VisitedLinkRenderProcessHostFactory& operator=(
      const VisitedLinkRenderProcessHostFactory&) = delete;

  content::RenderProcessHost* CreateRenderProcessHost(
      content::BrowserContext* browser_context,
      content::SiteInstance* site_instance) override {
    auto rph = std::make_unique<VisitRelayingRenderProcessHost>(browser_context,
                                                                context_.get());
    content::RenderProcessHost* result = rph.get();
    processes_.push_back(std::move(rph));
    return result;
  }

  // RenderProcessHostImpl::OnProcessLaunched only notifies once the child
  // process has launched. This is after RenderWidgetHost has been created. We
  // will notify at the end of SetUp.
  void NotifyProcessLaunced() {
    DCHECK(listener_);
    for (auto& rph : processes_) {
      listener_->OnRenderProcessHostCreated(rph.get());
    }
  }

  void SetVisitedLinkEventListener(VisitedLinkEventListener* listener) {
    DCHECK(listener);
    listener_ = listener;
  }

  VisitCountingContext* context() { return context_.get(); }

  void DeleteRenderProcessHosts() { processes_.clear(); }

 private:
  raw_ptr<VisitedLinkEventListener, DanglingUntriaged> listener_ = nullptr;

  std::list<std::unique_ptr<VisitRelayingRenderProcessHost>> processes_;
  std::unique_ptr<VisitCountingContext> context_;
};

class VisitedLinkEventsTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    SetRenderProcessHostFactory(&vc_rph_factory_);
    content::RenderViewHostTestHarness::SetUp();
    vc_rph_factory_.NotifyProcessLaunced();
  }

  void TearDown() override {
    // Explicitly destroy the writer before proceeding with the rest
    // of teardown because it posts a task to close a file handle, and
    // we need to make sure we've finished all file related work
    // before our superclass sets about destroying the scoped temp
    // directory.
    writer_.reset();
    DeleteContents();
    vc_rph_factory_.DeleteRenderProcessHosts();
    RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    auto context = std::make_unique<content::TestBrowserContext>();
    CreateVisitedLinkWriter(context.get());
    vc_rph_factory_.SetVisitedLinkEventListener(
        static_cast<VisitedLinkEventListener*>(writer_->GetListener()));
    return context;
  }

  VisitCountingContext* context() { return vc_rph_factory_.context(); }

  VisitedLinkWriter* writer() const { return writer_.get(); }

 protected:
  void CreateVisitedLinkWriter(content::BrowserContext* browser_context) {
    timer_ = std::make_unique<base::MockOneShotTimer>();
    writer_ =
        std::make_unique<VisitedLinkWriter>(browser_context, &delegate_, true);
    static_cast<VisitedLinkEventListener*>(writer_->GetListener())
        ->SetCoalesceTimerForTest(timer_.get());
    writer_->Init();
  }

  VisitedLinkRenderProcessHostFactory vc_rph_factory_;

  TestVisitedLinkDelegate delegate_;
  std::unique_ptr<base::MockOneShotTimer> timer_;
  std::unique_ptr<VisitedLinkWriter> writer_;
};

TEST_F(VisitedLinkEventsTest, Coalescence) {
  // Waiting complete rebuild the table.
  content::RunAllTasksUntilIdle();

  // After rebuild table expect reset event.
  EXPECT_EQ(1, context()->reset_event_count());

  // add some URLs to writer.
  // Add a few URLs.
  writer()->AddURL(GURL("http://acidtests.org/"));
  writer()->AddURL(GURL("http://google.com/"));
  writer()->AddURL(GURL("http://chromium.org/"));
  // Just for kicks, add a duplicate URL. This shouldn't increase the resulting
  writer()->AddURL(GURL("http://acidtests.org/"));
  ASSERT_TRUE(timer_->IsRunning());
  timer_->Fire();

  context()->WaitForUpdate();

  // We now should have 3 entries added in 1 event.
  EXPECT_EQ(3, context()->add_count());
  EXPECT_EQ(1, context()->add_event_count());

  // Test whether the coalescing continues by adding a few more URLs.
  writer()->AddURL(GURL("http://google.com/chrome/"));
  writer()->AddURL(GURL("http://webkit.org/"));
  writer()->AddURL(GURL("http://acid3.acidtests.org/"));

  ASSERT_TRUE(timer_->IsRunning());
  timer_->Fire();
  context()->WaitForUpdate();

  // We should have 6 entries added in 2 events.
  EXPECT_EQ(6, context()->add_count());
  EXPECT_EQ(2, context()->add_event_count());

  // Test whether duplicate entries produce add events.
  writer()->AddURL(GURL("http://acidtests.org/"));
  EXPECT_FALSE(timer_->IsRunning());
  context()->WaitForNoUpdate();

  // We should have no change in results.
  EXPECT_EQ(6, context()->add_count());
  EXPECT_EQ(2, context()->add_event_count());

  // Ensure that the coalescing does not resume after resetting.
  writer()->AddURL(GURL("http://build.chromium.org/"));
  EXPECT_TRUE(timer_->IsRunning());
  writer()->DeleteAllURLs();
  EXPECT_FALSE(timer_->IsRunning());
  context()->WaitForNoUpdate();

  // We should have no change in results except for one new reset event.
  EXPECT_EQ(6, context()->add_count());
  EXPECT_EQ(2, context()->add_event_count());
  EXPECT_EQ(2, context()->reset_event_count());
}

TEST_F(VisitedLinkEventsTest, Basics) {
  RenderViewHostTester::For(rvh())->CreateTestRenderView();

  // Waiting complete rebuild the table.
  content::RunAllTasksUntilIdle();

  // After rebuild table expect reset event.
  EXPECT_EQ(1, context()->reset_event_count());

  // Add a few URLs.
  writer()->AddURL(GURL("http://acidtests.org/"));
  writer()->AddURL(GURL("http://google.com/"));
  writer()->AddURL(GURL("http://chromium.org/"));
  ASSERT_TRUE(timer_->IsRunning());
  timer_->Fire();
  context()->WaitForUpdate();

  // We now should have 1 add event.
  EXPECT_EQ(1, context()->add_event_count());
  EXPECT_EQ(1, context()->reset_event_count());

  writer()->DeleteAllURLs();

  EXPECT_FALSE(timer_->IsRunning());
  context()->WaitForNoUpdate();

  // We should have no change in add results, plus one new reset event.
  EXPECT_EQ(1, context()->add_event_count());
  EXPECT_EQ(2, context()->reset_event_count());
}

TEST_F(VisitedLinkEventsTest, TabVisibility) {
  RenderViewHostTester::For(rvh())->CreateTestRenderView();

  // Waiting complete rebuild the table.
  content::RunAllTasksUntilIdle();

  // After rebuild table expect reset event.
  EXPECT_EQ(1, context()->reset_event_count());

  // Simulate tab becoming inactive.
  RenderViewHostTester::For(rvh())->SimulateWasHidden();

  // Add a few URLs.
  writer()->AddURL(GURL("http://acidtests.org/"));
  writer()->AddURL(GURL("http://google.com/"));
  writer()->AddURL(GURL("http://chromium.org/"));
  ASSERT_TRUE(timer_->IsRunning());
  timer_->Fire();
  context()->WaitForNoUpdate();

  // We shouldn't have any events.
  EXPECT_EQ(0, context()->add_event_count());
  EXPECT_EQ(1, context()->reset_event_count());

  // Simulate the tab becoming active.
  RenderViewHostTester::For(rvh())->SimulateWasShown();
  context()->WaitForUpdate();

  // We should now have 3 add events, still no reset events.
  EXPECT_EQ(1, context()->add_event_count());
  EXPECT_EQ(1, context()->reset_event_count());

  // Deactivate the tab again.
  RenderViewHostTester::For(rvh())->SimulateWasHidden();

  // Add a bunch of URLs (over 50) to exhaust the link event buffer.
  for (int i = 0; i < 100; i++)
    writer()->AddURL(TestURL(i));

  ASSERT_TRUE(timer_->IsRunning());
  timer_->Fire();
  context()->WaitForNoUpdate();

  // Again, no change in events until tab is active.
  EXPECT_EQ(1, context()->add_event_count());
  EXPECT_EQ(1, context()->reset_event_count());

  // Activate the tab.
  RenderViewHostTester::For(rvh())->SimulateWasShown();
  EXPECT_FALSE(timer_->IsRunning());
  context()->WaitForUpdate();

  // We should have only one more reset event.
  EXPECT_EQ(1, context()->add_event_count());
  EXPECT_EQ(2, context()->reset_event_count());
}

// Tests that VisitedLink ignores renderer process creation notification for a
// different context.
TEST_F(VisitedLinkEventsTest, IgnoreRendererCreationFromDifferentContext) {
  content::TestBrowserContext different_context;
  VisitCountingContext counting_context;
  // There are two render process hosts in play with this test. The primary
  // one is where the observer callback (done below) will be received
  // and don't need an observer for the other process host as it isn't
  // needed in the test.
  VisitRelayingRenderProcessHost different_process_host(&different_context,
                                                        &counting_context);

  size_t old_size = counting_context.binding().size();

  static_cast<VisitedLinkEventListener*>(writer()->GetListener())
      ->OnRenderProcessHostCreated(&different_process_host);
  size_t new_size = counting_context.binding().size();
  EXPECT_EQ(old_size, new_size);
}

// This class allows us to test PartitionedVisitedLinkWriter's inter-process
// communication via the VisitedLinkNotificationSink interface. We also test
// whether the VisitedLinkEventListener is able to connect the
// PartitionedVisitedLinkWriter's updates with each RenderProcessHost.
class PartitionedVisitedLinkEventsTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    SetRenderProcessHostFactory(&vc_rph_factory_);
    content::RenderViewHostTestHarness::SetUp();
    vc_rph_factory_.NotifyProcessLaunced();
  }

  void TearDown() override {
    partitioned_writer_.reset();
    DeleteContents();
    vc_rph_factory_.DeleteRenderProcessHosts();
    RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    auto context = std::make_unique<content::TestBrowserContext>();
    CreatePartitionedVisitedLinkWriter(context.get());
    vc_rph_factory_.SetVisitedLinkEventListener(
        static_cast<VisitedLinkEventListener*>(
            partitioned_writer_->GetListener()));
    return context;
  }

  VisitCountingContext* context() { return vc_rph_factory_.context(); }

  PartitionedVisitedLinkWriter* partitioned_writer() const {
    return partitioned_writer_.get();
  }

 protected:
  void CreatePartitionedVisitedLinkWriter(
      content::BrowserContext* browser_context) {
    timer_ = std::make_unique<base::MockOneShotTimer>();
    partitioned_writer_ = std::make_unique<PartitionedVisitedLinkWriter>(
        browser_context, &delegate_);
    static_cast<VisitedLinkEventListener*>(partitioned_writer_->GetListener())
        ->SetCoalesceTimerForTest(timer_.get());
    partitioned_writer_->Init();
  }

  VisitedLinkRenderProcessHostFactory vc_rph_factory_;

  TestVisitedLinkDelegate delegate_;
  std::unique_ptr<base::MockOneShotTimer> timer_;
  std::unique_ptr<PartitionedVisitedLinkWriter> partitioned_writer_;
};

TEST_F(PartitionedVisitedLinkEventsTest, Coalescence) {
  // Waiting for notifications that the table build is complete.
  content::RunAllTasksUntilIdle();

  // A reset should be called after the hashtable is built from the
  // VisitedLinkDatabase.
  EXPECT_EQ(1, context()->reset_event_count());

  // Add test links to the hashtable.
  const int kFirstBatchCount = 3;
  for (int i = 0; i < kFirstBatchCount; i++) {
    VisitedLink link = {TestURL(i), net::SchemefulSite(TestURL(i)),
                        url::Origin::Create(TestURL(i))};
    partitioned_writer_->AddVisitedLink(link);
    ASSERT_EQ(i + 1, partitioned_writer_->GetUsedCount());
  }
  // Ensure that adding a duplicate link doesn't increase the add count.
  const int kDupeIndex = kFirstBatchCount - 1;
  VisitedLink duplicate_link1 = {TestURL(kDupeIndex),
                                 net::SchemefulSite(TestURL(kDupeIndex)),
                                 url::Origin::Create(TestURL(kDupeIndex))};
  partitioned_writer_->AddVisitedLink(duplicate_link1);
  ASSERT_TRUE(timer_->IsRunning());
  timer_->Fire();

  context()->WaitForUpdate();

  // We now should have 3 entries added in 1 event.
  EXPECT_EQ(kFirstBatchCount, context()->add_count());
  EXPECT_EQ(1, context()->add_event_count());

  // Test whether the coalescing continues by adding a few more URLs.
  const int kSecondBatchCount = 6;
  for (int i = kFirstBatchCount; i < kSecondBatchCount; i++) {
    VisitedLink link = {TestURL(i), net::SchemefulSite(TestURL(i)),
                        url::Origin::Create(TestURL(i))};
    partitioned_writer_->AddVisitedLink(link);
    ASSERT_EQ(i + 1, partitioned_writer_->GetUsedCount());
  }
  ASSERT_TRUE(timer_->IsRunning());
  timer_->Fire();
  context()->WaitForUpdate();

  // We should have 6 entries added in 2 events.
  EXPECT_EQ(kSecondBatchCount, context()->add_count());
  EXPECT_EQ(2, context()->add_event_count());

  // Test again whether duplicate entries produce add events.
  const int kDupeIndex2 = kSecondBatchCount - 1;
  VisitedLink duplicate_link2 = {TestURL(kDupeIndex2),
                                 net::SchemefulSite(TestURL(kDupeIndex2)),
                                 url::Origin::Create(TestURL(kDupeIndex2))};
  partitioned_writer_->AddVisitedLink(duplicate_link2);
  EXPECT_FALSE(timer_->IsRunning());
  context()->WaitForNoUpdate();

  // We should have no change in results.
  EXPECT_EQ(kSecondBatchCount, context()->add_count());
  EXPECT_EQ(2, context()->add_event_count());

  // Ensure that the coalescing does not resume after resetting.
  VisitedLink final_link = {TestURL(kSecondBatchCount),
                            net::SchemefulSite(TestURL(kSecondBatchCount)),
                            url::Origin::Create(TestURL(kSecondBatchCount))};
  partitioned_writer_->AddVisitedLink(final_link);
  EXPECT_TRUE(timer_->IsRunning());
  partitioned_writer()->DeleteAllVisitedLinks();
  EXPECT_FALSE(timer_->IsRunning());
  context()->WaitForNoUpdate();

  // We should have no change in results except for one new reset event.
  EXPECT_EQ(kSecondBatchCount, context()->add_count());
  EXPECT_EQ(2, context()->add_event_count());
  EXPECT_EQ(2, context()->reset_event_count());
}

TEST_F(PartitionedVisitedLinkEventsTest, Basics) {
  RenderViewHostTester::For(rvh())->CreateTestRenderView();

  // Waiting for notifications that the table build is complete.
  content::RunAllTasksUntilIdle();

  // A reset and a salt update should be called after building the hashtable
  // from the VisitedLinkDatabase.
  EXPECT_EQ(1, context()->reset_event_count());
  EXPECT_EQ(1, context()->salts_update_event_count());
  EXPECT_EQ(1u, context()->salts().size());

  // Ensure that the salt sent to the VisitedLinkReader for a given origin is
  // equivalent to what we have stored in PartitionedVisitedLinkWriter.
  for (const auto& [origin, salt] : context()->salts()) {
    const std::optional<uint64_t> expected_salt =
        partitioned_writer_->GetOrAddOriginSalt(origin);
    EXPECT_TRUE(expected_salt.has_value());
    EXPECT_EQ(salt, expected_salt.value());
  }

  // Add test links to the hashtable.
  const int kLinkCount = 3;
  for (int i = 0; i < kLinkCount; i++) {
    VisitedLink link = {TestURL(i), net::SchemefulSite(TestURL(i)),
                        url::Origin::Create(TestURL(i))};
    partitioned_writer_->AddVisitedLink(link);
    ASSERT_EQ(i + 1, partitioned_writer_->GetUsedCount());
  }
  ASSERT_TRUE(timer_->IsRunning());
  timer_->Fire();
  context()->WaitForUpdate();

  // We now should have 1 add event.
  EXPECT_EQ(1, context()->add_event_count());
  EXPECT_EQ(1, context()->reset_event_count());

  partitioned_writer()->DeleteAllVisitedLinks();

  EXPECT_FALSE(timer_->IsRunning());
  context()->WaitForNoUpdate();

  // We should have no change in add results, plus one new reset event.
  EXPECT_EQ(1, context()->add_event_count());
  EXPECT_EQ(2, context()->reset_event_count());
}

TEST_F(PartitionedVisitedLinkEventsTest, TabVisibility) {
  RenderViewHostTester::For(rvh())->CreateTestRenderView();

  // Waiting for notifications that the table build is complete.
  content::RunAllTasksUntilIdle();

  // A reset should be called after building the hashtable from the
  // VisitedLinkDatabase.
  EXPECT_EQ(1, context()->reset_event_count());

  // Simulate tab becoming inactive.
  RenderViewHostTester::For(rvh())->SimulateWasHidden();

  // Add test links to the hashtable.
  const int kFirstBatchCount = 3;
  for (int i = 0; i < kFirstBatchCount; i++) {
    VisitedLink link = {TestURL(i), net::SchemefulSite(TestURL(i)),
                        url::Origin::Create(TestURL(i))};
    partitioned_writer_->AddVisitedLink(link);
    ASSERT_EQ(i + 1, partitioned_writer_->GetUsedCount());
  }
  ASSERT_TRUE(timer_->IsRunning());
  timer_->Fire();
  context()->WaitForNoUpdate();

  // We shouldn't have any add events because the tab is not visible.
  EXPECT_EQ(0, context()->add_event_count());
  EXPECT_EQ(1, context()->reset_event_count());

  // Simulate the tab becoming active.
  RenderViewHostTester::For(rvh())->SimulateWasShown();
  context()->WaitForUpdate();

  // Now that the tab is active, we should receive the add event.
  EXPECT_EQ(1, context()->add_event_count());
  EXPECT_EQ(1, context()->reset_event_count());

  // Deactivate the tab again.
  RenderViewHostTester::For(rvh())->SimulateWasHidden();

  // Add a bunch of links (over 50) to exhaust the link event buffer.
  for (int i = kFirstBatchCount; i < 100; i++) {
    VisitedLink link = {TestURL(i), net::SchemefulSite(TestURL(i)),
                        url::Origin::Create(TestURL(i))};
    partitioned_writer_->AddVisitedLink(link);
    ASSERT_EQ(i + 1, partitioned_writer_->GetUsedCount());
  }

  ASSERT_TRUE(timer_->IsRunning());
  timer_->Fire();
  context()->WaitForNoUpdate();

  // Again, no change in events until tab is active.
  EXPECT_EQ(1, context()->add_event_count());
  EXPECT_EQ(1, context()->reset_event_count());

  // Activate the tab.
  RenderViewHostTester::For(rvh())->SimulateWasShown();
  EXPECT_FALSE(timer_->IsRunning());
  context()->WaitForUpdate();

  // We should have only one more reset event because the links were added in
  // bulk (which triggers the code to just reset rather than send individually).
  EXPECT_EQ(1, context()->add_event_count());
  EXPECT_EQ(2, context()->reset_event_count());
}

// Tests that VisitedLink ignores renderer process creation notification for a
// different context.
TEST_F(PartitionedVisitedLinkEventsTest,
       IgnoreRendererCreationFromDifferentContext) {
  content::TestBrowserContext different_context;
  VisitCountingContext counting_context;
  // There are two render process hosts in play with this test. The primary
  // one is where the observer callback (done below) will be received
  // and don't need an observer for the other process host as it isn't
  // needed in the test.
  VisitRelayingRenderProcessHost different_process_host(&different_context,
                                                        &counting_context);

  size_t old_size = counting_context.binding().size();

  static_cast<VisitedLinkEventListener*>(partitioned_writer()->GetListener())
      ->OnRenderProcessHostCreated(&different_process_host);
  size_t new_size = counting_context.binding().size();
  EXPECT_EQ(old_size, new_size);
}

class VisitedLinkCompletelyResetEventTest : public VisitedLinkEventsTest {
 public:
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    auto context = std::make_unique<content::TestBrowserContext>();
    CreateVisitedLinkFile(context.get());
    CreateVisitedLinkWriter(context.get());
    vc_rph_factory_.SetVisitedLinkEventListener(
        static_cast<VisitedLinkEventListener*>(writer_->GetListener()));
    return context;
  }

  void CreateVisitedLinkFile(content::BrowserContext* browser_context) {
    base::FilePath visited_file =
        browser_context->GetPath().Append(FILE_PATH_LITERAL("Visited Links"));
    std::unique_ptr<VisitedLinkWriter> writer(
        new VisitedLinkWriter(new TrackingVisitedLinkEventListener(),
                              &delegate_, true, true, visited_file, 0));
    writer->Init();
    // Waiting complete create the table.
    content::RunAllTasksUntilIdle();

    writer.reset();
    // Wait for all pending file I/O to be completed.
    content::RunAllTasksUntilIdle();
  }
};

TEST_F(VisitedLinkCompletelyResetEventTest, LoadTable) {
  // Waiting complete loading the table.
  content::RunAllTasksUntilIdle();

  context()->binding().FlushForTesting();

  // After load table expect completely reset event.
  EXPECT_EQ(1, context()->completely_reset_event_count());
}

}  // namespace visitedlink
