// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/browser/pnacl_host.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/nacl/browser/pnacl_translation_cache.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace pnacl {
namespace {

// Size of a buffer used for writing and reading from a file.
const size_t kBufferSize = 16u;

}  // namespace

class PnaclHostTest : public testing::Test {
 protected:
  PnaclHostTest()
      : host_(nullptr),
        temp_callback_count_(0),
        write_callback_count_(0),
        task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}
  void SetUp() override {
    host_ = PnaclHost::GetInstance();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    host_->InitForTest(temp_dir_.GetPath(), true);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(PnaclHost::CacheReady, host_->cache_state_);
  }
  void TearDown() override {
    EXPECT_EQ(0U, host_->pending_translations());
    // Give the host a chance to de-init the backend, and then delete it.
    host_->RendererClosing(0);
    content::RunAllTasksUntilIdle();
    disk_cache::FlushCacheThreadForTesting();
    EXPECT_EQ(PnaclHost::CacheUninitialized, host_->cache_state_);
  }
  int GetCacheSize() { return host_->disk_cache_->Size(); }
  int CacheIsInitialized() {
    return host_->cache_state_ == PnaclHost::CacheReady;
  }
  void ReInitBackend() {
    host_->InitForTest(temp_dir_.GetPath(), true);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(PnaclHost::CacheReady, host_->cache_state_);
  }

 public:  // Required for derived classes to bind this method
          // Callbacks used by tests which call GetNexeFd.
  // CallbackExpectMiss checks that the fd is valid and a miss is reported,
  // and also writes some data into the file, which is read back by
  // CallbackExpectHit
  void CallbackExpectMiss(const base::File& file, bool is_hit) {
    EXPECT_FALSE(is_hit);
    ASSERT_TRUE(file.IsValid());
    base::File::Info info;
    base::File* mutable_file = const_cast<base::File*>(&file);
    EXPECT_TRUE(mutable_file->GetInfo(&info));
    EXPECT_FALSE(info.is_directory);
    EXPECT_EQ(0LL, info.size);
    char str[kBufferSize];
    memset(str, 0x0, kBufferSize);
    snprintf(str, kBufferSize, "testdata%d", ++write_callback_count_);
    EXPECT_EQ(kBufferSize, static_cast<size_t>(UNSAFE_TODO(
                               mutable_file->Write(0, str, kBufferSize))));
    temp_callback_count_++;
  }
  void CallbackExpectHit(const base::File& file, bool is_hit) {
    EXPECT_TRUE(is_hit);
    ASSERT_TRUE(file.IsValid());
    base::File::Info info;
    base::File* mutable_file = const_cast<base::File*>(&file);
    EXPECT_TRUE(mutable_file->GetInfo(&info));
    EXPECT_FALSE(info.is_directory);
    EXPECT_EQ(kBufferSize, static_cast<size_t>(info.size));
    char data[kBufferSize];
    memset(data, 0x0, kBufferSize);
    char str[kBufferSize];
    memset(str, 0x0, kBufferSize);
    snprintf(str, kBufferSize, "testdata%d", write_callback_count_);
    EXPECT_EQ(kBufferSize, static_cast<size_t>(UNSAFE_TODO(
                               mutable_file->Read(0, data, kBufferSize))));
    EXPECT_STREQ(str, data);
    temp_callback_count_++;
  }

 protected:
  raw_ptr<PnaclHost> host_;
  int temp_callback_count_;
  int write_callback_count_;
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

static nacl::PnaclCacheInfo GetTestCacheInfo() {
  nacl::PnaclCacheInfo info;
  info.pexe_url = GURL("http://www.google.com");
  info.abi_version = 0;
  info.opt_level = 0;
  info.has_no_store_header = false;
  info.use_subzero = false;
  return info;
}

#define GET_NEXE_FD(renderer, instance, incognito, info, expect_hit)         \
  do {                                                                       \
    SCOPED_TRACE("");                                                        \
    host_->GetNexeFd(                                                        \
        renderer, instance, incognito, info,                                 \
        base::BindRepeating(expect_hit ? &PnaclHostTest::CallbackExpectHit   \
                                       : &PnaclHostTest::CallbackExpectMiss, \
                            base::Unretained(this)));                        \
  } while (0)

TEST_F(PnaclHostTest, BasicMiss) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  // Test cold miss.
  GET_NEXE_FD(0, 0, false, info, false);
  EXPECT_EQ(1U, host_->pending_translations());
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1U, host_->pending_translations());
  EXPECT_EQ(1, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0U, host_->pending_translations());
  // Test that a different cache info field also misses.
  info.etag = std::string("something else");
  GET_NEXE_FD(0, 0, false, info, false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2, temp_callback_count_);
  EXPECT_EQ(1U, host_->pending_translations());
  host_->RendererClosing(0);
  content::RunAllTasksUntilIdle();
  // Check that the cache has de-initialized after the last renderer goes away.
  EXPECT_FALSE(CacheIsInitialized());
}

TEST_F(PnaclHostTest, BadArguments) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  GET_NEXE_FD(0, 0, false, info, false);
  EXPECT_EQ(1U, host_->pending_translations());
  host_->TranslationFinished(0, 1, true);  // nonexistent translation
  EXPECT_EQ(1U, host_->pending_translations());
  host_->RendererClosing(1);  // nonexistent renderer
  EXPECT_EQ(1U, host_->pending_translations());
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, temp_callback_count_);
  host_->RendererClosing(0);  // close without finishing
}

TEST_F(PnaclHostTest, BasicHit) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  GET_NEXE_FD(0, 0, false, info, false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  content::RunAllTasksUntilIdle();
  GET_NEXE_FD(0, 1, false, info, true);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2, temp_callback_count_);
  EXPECT_EQ(0U, host_->pending_translations());
}

TEST_F(PnaclHostTest, TranslationErrors) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  GET_NEXE_FD(0, 0, false, info, false);
  // Early abort, before temp file request returns
  host_->TranslationFinished(0, 0, false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0U, host_->pending_translations());
  EXPECT_EQ(0, temp_callback_count_);
  // The backend will have been freed when the query comes back and there
  // are no pending translations.
  EXPECT_FALSE(CacheIsInitialized());
  ReInitBackend();
  // Check that another request for the same info misses successfully.
  GET_NEXE_FD(0, 0, false, info, false);
  content::RunAllTasksUntilIdle();
  host_->TranslationFinished(0, 0, true);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, temp_callback_count_);
  EXPECT_EQ(0U, host_->pending_translations());

  // Now try sending the error after the temp file request returns
  info.abi_version = 222;
  GET_NEXE_FD(0, 0, false, info, false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2, temp_callback_count_);
  host_->TranslationFinished(0, 0, false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0U, host_->pending_translations());
  // Check another successful miss
  GET_NEXE_FD(0, 0, false, info, false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(3, temp_callback_count_);
  host_->TranslationFinished(0, 0, false);
  EXPECT_EQ(0U, host_->pending_translations());
}

TEST_F(PnaclHostTest, OverlappedMissesAfterTempReturn) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  GET_NEXE_FD(0, 0, false, info, false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, temp_callback_count_);
  EXPECT_EQ(1U, host_->pending_translations());
  // Test that a second request for the same nexe while the first one is still
  // outstanding eventually hits.
  GET_NEXE_FD(0, 1, false, info, true);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2U, host_->pending_translations());
  // The temp file should not be returned to the second request until after the
  // first is finished translating.
  EXPECT_EQ(1, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2, temp_callback_count_);
  EXPECT_EQ(0U, host_->pending_translations());
}

TEST_F(PnaclHostTest, OverlappedMissesBeforeTempReturn) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  GET_NEXE_FD(0, 0, false, info, false);
  // Send the 2nd fd request before the first one returns a temp file.
  GET_NEXE_FD(0, 1, false, info, true);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, temp_callback_count_);
  EXPECT_EQ(2U, host_->pending_translations());
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2U, host_->pending_translations());
  EXPECT_EQ(1, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2, temp_callback_count_);
  EXPECT_EQ(0U, host_->pending_translations());
}

TEST_F(PnaclHostTest, OverlappedHitsBeforeTempReturn) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  // Store one in the cache and complete it.
  GET_NEXE_FD(0, 0, false, info, false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0U, host_->pending_translations());
  GET_NEXE_FD(0, 0, false, info, true);
  // Request the second before the first temp file returns.
  GET_NEXE_FD(0, 1, false, info, true);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(3, temp_callback_count_);
  EXPECT_EQ(0U, host_->pending_translations());
}

TEST_F(PnaclHostTest, OverlappedHitsAfterTempReturn) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  // Store one in the cache and complete it.
  GET_NEXE_FD(0, 0, false, info, false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0U, host_->pending_translations());
  GET_NEXE_FD(0, 0, false, info, true);
  content::RunAllTasksUntilIdle();
  GET_NEXE_FD(0, 1, false, info, true);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(3, temp_callback_count_);
  EXPECT_EQ(0U, host_->pending_translations());
}

TEST_F(PnaclHostTest, OverlappedMissesRendererClosing) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  GET_NEXE_FD(0, 0, false, info, false);
  // Send the 2nd fd request from a different renderer.
  // Test that it eventually gets an fd after the first renderer closes.
  GET_NEXE_FD(1, 1, false, info, false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, temp_callback_count_);
  EXPECT_EQ(2U, host_->pending_translations());
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2U, host_->pending_translations());
  EXPECT_EQ(1, temp_callback_count_);
  host_->RendererClosing(0);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2, temp_callback_count_);
  EXPECT_EQ(1U, host_->pending_translations());
  host_->RendererClosing(1);
}

TEST_F(PnaclHostTest, Incognito) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  GET_NEXE_FD(0, 0, true, info, false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  content::RunAllTasksUntilIdle();
  // Check that an incognito translation is not stored in the cache
  GET_NEXE_FD(0, 0, false, info, false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  content::RunAllTasksUntilIdle();
  // Check that an incognito translation can hit from a normal one.
  GET_NEXE_FD(0, 0, true, info, true);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(3, temp_callback_count_);
}

TEST_F(PnaclHostTest, IncognitoOverlappedMiss) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  GET_NEXE_FD(0, 0, true, info, false);
  GET_NEXE_FD(0, 1, false, info, false);
  content::RunAllTasksUntilIdle();
  // Check that both translations have returned misses, (i.e. that the
  // second one has not blocked on the incognito one)
  EXPECT_EQ(2, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  host_->TranslationFinished(0, 1, true);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0U, host_->pending_translations());

  // Same test, but issue the 2nd request after the first has returned a miss.
  info.abi_version = 222;
  GET_NEXE_FD(0, 0, true, info, false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(3, temp_callback_count_);
  GET_NEXE_FD(0, 1, false, info, false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(4, temp_callback_count_);
  host_->RendererClosing(0);
}

TEST_F(PnaclHostTest, IncognitoSecondOverlappedMiss) {
  // If the non-incognito request comes first, it should
  // behave exactly like OverlappedMissBeforeTempReturn
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  GET_NEXE_FD(0, 0, false, info, false);
  // Send the 2nd fd request before the first one returns a temp file.
  GET_NEXE_FD(0, 1, true, info, true);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, temp_callback_count_);
  EXPECT_EQ(2U, host_->pending_translations());
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2U, host_->pending_translations());
  EXPECT_EQ(1, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2, temp_callback_count_);
  EXPECT_EQ(0U, host_->pending_translations());
}

// Test that pexes with the no-store header do not get cached.
TEST_F(PnaclHostTest, CacheControlNoStore) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  info.has_no_store_header = true;
  GET_NEXE_FD(0, 0, false, info, false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0U, host_->pending_translations());
  EXPECT_EQ(0, GetCacheSize());
}

// Test that no-store pexes do not wait, but do duplicate translations
TEST_F(PnaclHostTest, NoStoreOverlappedMiss) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  info.has_no_store_header = true;
  GET_NEXE_FD(0, 0, false, info, false);
  GET_NEXE_FD(0, 1, false, info, false);
  content::RunAllTasksUntilIdle();
  // Check that both translations have returned misses, (i.e. that the
  // second one has not blocked on the first one)
  EXPECT_EQ(2, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  host_->TranslationFinished(0, 1, true);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0U, host_->pending_translations());

  // Same test, but issue the 2nd request after the first has returned a miss.
  info.abi_version = 222;
  GET_NEXE_FD(0, 0, false, info, false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(3, temp_callback_count_);
  GET_NEXE_FD(0, 1, false, info, false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(4, temp_callback_count_);
  host_->RendererClosing(0);
}

TEST_F(PnaclHostTest, ClearTranslationCache) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  // Add 2 entries in the cache
  GET_NEXE_FD(0, 0, false, info, false);
  info.abi_version = 222;
  GET_NEXE_FD(0, 1, false, info, false);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  host_->TranslationFinished(0, 1, true);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0U, host_->pending_translations());
  EXPECT_EQ(2, GetCacheSize());
  net::TestCompletionCallback cb;
  // Since we are using a memory backend, the clear should happen immediately.
  host_->ClearTranslationCacheEntriesBetween(base::Time(), base::Time(),
                                             base::BindOnce(cb.callback(), 0));
  // Check that the translation cache has been cleared before flushing the
  // queues, because the backend will be freed once it is.
  EXPECT_EQ(0, GetCacheSize());
  EXPECT_EQ(0, cb.GetResult(net::ERR_IO_PENDING));
  // Call posted PnaclHost::CopyFileToBuffer() tasks.
  base::RunLoop().RunUntilIdle();
  // Now check that the backend has been freed.
  EXPECT_FALSE(CacheIsInitialized());
}

// A version of PnaclHostTest that initializes cache on disk.
class PnaclHostTestDisk : public PnaclHostTest {
 protected:
  void SetUp() override {
    host_ = PnaclHost::GetInstance();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    host_->InitForTest(temp_dir_.GetPath(), false);
    EXPECT_EQ(PnaclHost::CacheInitializing, host_->cache_state_);
  }
  void DeInit() {
    host_->DeInitIfSafe();
  }
};
TEST_F(PnaclHostTestDisk, DeInitWhileInitializing) {
  // Since there's no easy way to pump message queues one message at a time, we
  // have to simulate what would happen if 1 DeInitIfsafe task gets queued, then
  // a GetNexeFd gets queued, and then another DeInitIfSafe gets queued before
  // the first one runs. We can just shortcut and call DeInitIfSafe while the
  // cache is still initializing.
  DeInit();

  // Now let it finish initializing. (Other tests don't need this since they
  // use in-memory storage).
  disk_cache::FlushCacheThreadForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(CacheIsInitialized());
}

}  // namespace pnacl
