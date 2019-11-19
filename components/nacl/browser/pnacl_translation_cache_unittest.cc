// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/browser/pnacl_translation_cache.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/nacl/common/pnacl_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using base::FilePath;

namespace pnacl {

const int kTestDiskCacheSize = 16 * 1024 * 1024;

class PnaclTranslationCacheTest : public testing::Test {
 protected:
  PnaclTranslationCacheTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}
  ~PnaclTranslationCacheTest() override {}
  void SetUp() override { cache_.reset(new PnaclTranslationCache()); }
  void TearDown() override {
    // The destructor of PnaclTranslationCacheWriteEntry posts a task to the IO
    // thread to close the backend cache entry. We want to make sure the entries
    // are closed before we delete the backend (and in particular the destructor
    // for the memory backend has a DCHECK to verify this), so we run the loop
    // here to ensure the task gets processed.
    base::RunLoop().RunUntilIdle();
    cache_.reset();
  }

  void InitBackend(bool in_mem);
  void StoreNexe(const std::string& key, const std::string& nexe);
  std::string GetNexe(const std::string& key);

  std::unique_ptr<PnaclTranslationCache> cache_;
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

void PnaclTranslationCacheTest::InitBackend(bool in_mem) {
  net::TestCompletionCallback init_cb;
  if (!in_mem) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }
  // Use the private init method so we can control the size
  int rv = cache_->Init(in_mem ? net::MEMORY_CACHE : net::PNACL_CACHE,
                        in_mem ? base::FilePath() : temp_dir_.GetPath(),
                        in_mem ? kMaxMemCacheSize : kTestDiskCacheSize,
                        init_cb.callback());
  if (in_mem)
    ASSERT_EQ(net::OK, rv);
  ASSERT_EQ(net::OK, init_cb.GetResult(rv));
  ASSERT_EQ(0, cache_->Size());
}

void PnaclTranslationCacheTest::StoreNexe(const std::string& key,
                                          const std::string& nexe) {
  net::TestCompletionCallback store_cb;
  scoped_refptr<net::DrainableIOBuffer> nexe_buf =
      base::MakeRefCounted<net::DrainableIOBuffer>(
          base::MakeRefCounted<net::StringIOBuffer>(nexe), nexe.size());
  cache_->StoreNexe(key, nexe_buf.get(), store_cb.callback());
  // Using ERR_IO_PENDING here causes the callback to wait for the result
  // which should be harmless even if it returns OK immediately. This is because
  // we don't plumb the intermediate writing stages all the way out.
  EXPECT_EQ(net::OK, store_cb.GetResult(net::ERR_IO_PENDING));
}

// Inspired by net::TestCompletionCallback. Instantiate a TestNexeCallback and
// pass the GetNexeCallback returned by the callback() method to GetNexe.
// Then call GetResult, which will pump the message loop until it gets a result,
// return the resulting IOBuffer and fill in the return value
class TestNexeCallback {
 public:
  TestNexeCallback()
      : have_result_(false),
        result_(-1),
        cb_(base::Bind(&TestNexeCallback::SetResult, base::Unretained(this))) {}
  GetNexeCallback callback() { return cb_; }
  net::DrainableIOBuffer* GetResult(int* result) {
    while (!have_result_)
      base::RunLoop().RunUntilIdle();
    have_result_ = false;
    *result = result_;
    return buf_.get();
  }

 private:
  void SetResult(int rv, scoped_refptr<net::DrainableIOBuffer> buf) {
    have_result_ = true;
    result_ = rv;
    buf_ = buf;
  }
  bool have_result_;
  int result_;
  scoped_refptr<net::DrainableIOBuffer> buf_;
  const GetNexeCallback cb_;
};

std::string PnaclTranslationCacheTest::GetNexe(const std::string& key) {
  TestNexeCallback load_cb;
  cache_->GetNexe(key, load_cb.callback());
  int rv;
  scoped_refptr<net::DrainableIOBuffer> buf(load_cb.GetResult(&rv));
  EXPECT_EQ(net::OK, rv);
  if (buf.get() == NULL) // for some reason ASSERT macros don't work here.
    return std::string();
  std::string nexe(buf->data(), buf->size());
  return nexe;
}

static const std::string test_key("1");
static const std::string test_store_val("testnexe");
static const int kLargeNexeSize = 8 * 1024 * 1024;

TEST(PnaclTranslationCacheKeyTest, CacheKeyTest) {
  nacl::PnaclCacheInfo info;
  info.pexe_url = GURL("http://www.google.com");
  info.abi_version = 0;
  info.opt_level = 0;
  info.sandbox_isa = "x86-32";
  std::string test_time("Wed, 15 Nov 1995 06:25:24 GMT");
  EXPECT_TRUE(base::Time::FromString(test_time.c_str(), &info.last_modified));
  // Basic check for URL and time components
  EXPECT_EQ("ABI:0;opt:0;URL:http://www.google.com/;"
            "modified:1995:11:15:6:25:24:0:UTC;etag:;"
            "sandbox:x86-32;extra_flags:;",
            PnaclTranslationCache::GetKey(info));
  // Check that query portion of URL is not stripped
  info.pexe_url = GURL("http://www.google.com/?foo=bar");
  EXPECT_EQ("ABI:0;opt:0;URL:http://www.google.com/?foo=bar;"
            "modified:1995:11:15:6:25:24:0:UTC;etag:;"
            "sandbox:x86-32;extra_flags:;",
            PnaclTranslationCache::GetKey(info));
  // Check that username, password, and normal port are stripped
  info.pexe_url = GURL("https://user:host@www.google.com:443/");
  EXPECT_EQ("ABI:0;opt:0;URL:https://www.google.com/;"
            "modified:1995:11:15:6:25:24:0:UTC;etag:;"
            "sandbox:x86-32;extra_flags:;",
            PnaclTranslationCache::GetKey(info));
  // Check that unusual port is not stripped but ref is stripped
  info.pexe_url = GURL("https://www.google.com:444/#foo");
  EXPECT_EQ("ABI:0;opt:0;URL:https://www.google.com:444/;"
            "modified:1995:11:15:6:25:24:0:UTC;etag:;"
            "sandbox:x86-32;extra_flags:;",
            PnaclTranslationCache::GetKey(info));
  // Check chrome-extesnsion scheme
  info.pexe_url = GURL("chrome-extension://ljacajndfccfgnfohlgkdphmbnpkjflk/");
  EXPECT_EQ("ABI:0;opt:0;"
            "URL:chrome-extension://ljacajndfccfgnfohlgkdphmbnpkjflk/;"
            "modified:1995:11:15:6:25:24:0:UTC;etag:;"
            "sandbox:x86-32;extra_flags:;",
            PnaclTranslationCache::GetKey(info));
  // Check that ABI version, opt level, and etag are in the key
  info.pexe_url = GURL("http://www.google.com/");
  info.abi_version = 2;
  EXPECT_EQ("ABI:2;opt:0;URL:http://www.google.com/;"
            "modified:1995:11:15:6:25:24:0:UTC;etag:;"
            "sandbox:x86-32;extra_flags:;",
            PnaclTranslationCache::GetKey(info));
  info.opt_level = 2;
  EXPECT_EQ("ABI:2;opt:2;URL:http://www.google.com/;"
            "modified:1995:11:15:6:25:24:0:UTC;etag:;"
            "sandbox:x86-32;extra_flags:;",
            PnaclTranslationCache::GetKey(info));
  // Check that Subzero gets a different cache key.
  info.use_subzero = true;
  EXPECT_EQ("ABI:2;opt:2subzero;URL:http://www.google.com/;"
            "modified:1995:11:15:6:25:24:0:UTC;etag:;"
            "sandbox:x86-32;extra_flags:;",
            PnaclTranslationCache::GetKey(info));
  info.use_subzero = false;
  info.etag = std::string("etag");
  EXPECT_EQ("ABI:2;opt:2;URL:http://www.google.com/;"
            "modified:1995:11:15:6:25:24:0:UTC;etag:etag;"
            "sandbox:x86-32;extra_flags:;",
            PnaclTranslationCache::GetKey(info));

  info.extra_flags = "-mavx-neon";
  EXPECT_EQ("ABI:2;opt:2;URL:http://www.google.com/;"
            "modified:1995:11:15:6:25:24:0:UTC;etag:etag;"
            "sandbox:x86-32;extra_flags:-mavx-neon;",
            PnaclTranslationCache::GetKey(info));

  // Check for all the time components, and null time
  info.last_modified = base::Time();
  EXPECT_EQ("ABI:2;opt:2;URL:http://www.google.com/;"
            "modified:0:0:0:0:0:0:0:UTC;etag:etag;"
            "sandbox:x86-32;extra_flags:-mavx-neon;",
            PnaclTranslationCache::GetKey(info));
  test_time.assign("Fri, 29 Feb 2008 13:04:12 GMT");
  EXPECT_TRUE(base::Time::FromString(test_time.c_str(), &info.last_modified));
  EXPECT_EQ("ABI:2;opt:2;URL:http://www.google.com/;"
            "modified:2008:2:29:13:4:12:0:UTC;etag:etag;"
            "sandbox:x86-32;extra_flags:-mavx-neon;",
            PnaclTranslationCache::GetKey(info));
}

TEST_F(PnaclTranslationCacheTest, StoreSmallInMem) {
  // Test that a single store puts something in the mem backend
  InitBackend(true);
  StoreNexe(test_key, test_store_val);
  EXPECT_EQ(1, cache_->Size());
}

TEST_F(PnaclTranslationCacheTest, StoreSmallOnDisk) {
  // Test that a single store puts something in the disk backend
  InitBackend(false);
  StoreNexe(test_key, test_store_val);
  EXPECT_EQ(1, cache_->Size());
}

TEST_F(PnaclTranslationCacheTest, StoreLargeOnDisk) {
  // Test a value too large(?) for a single I/O operation
  InitBackend(false);
  const std::string large_buffer(kLargeNexeSize, 'a');
  StoreNexe(test_key, large_buffer);
  EXPECT_EQ(1, cache_->Size());
}

TEST_F(PnaclTranslationCacheTest, InMemSizeLimit) {
  InitBackend(true);
  scoped_refptr<net::DrainableIOBuffer> large_buffer =
      base::MakeRefCounted<net::DrainableIOBuffer>(
          base::MakeRefCounted<net::StringIOBuffer>(
              std::string(kMaxMemCacheSize + 1, 'a')),
          kMaxMemCacheSize + 1);
  net::TestCompletionCallback store_cb;
  cache_->StoreNexe(test_key, large_buffer.get(), store_cb.callback());
  EXPECT_EQ(net::ERR_FAILED, store_cb.GetResult(net::ERR_IO_PENDING));
  base::RunLoop().RunUntilIdle();  // Ensure the entry is closed.
  EXPECT_EQ(0, cache_->Size());
}

TEST_F(PnaclTranslationCacheTest, GetOneInMem) {
  InitBackend(true);
  StoreNexe(test_key, test_store_val);
  EXPECT_EQ(1, cache_->Size());
  EXPECT_EQ(0, GetNexe(test_key).compare(test_store_val));
}

TEST_F(PnaclTranslationCacheTest, GetOneOnDisk) {
  InitBackend(false);
  StoreNexe(test_key, test_store_val);
  EXPECT_EQ(1, cache_->Size());
  EXPECT_EQ(0, GetNexe(test_key).compare(test_store_val));
}

TEST_F(PnaclTranslationCacheTest, GetLargeOnDisk) {
  InitBackend(false);
  const std::string large_buffer(kLargeNexeSize, 'a');
  StoreNexe(test_key, large_buffer);
  EXPECT_EQ(1, cache_->Size());
  EXPECT_EQ(0, GetNexe(test_key).compare(large_buffer));
}

TEST_F(PnaclTranslationCacheTest, StoreTwice) {
  // Test that storing twice with the same key overwrites
  InitBackend(true);
  StoreNexe(test_key, test_store_val);
  StoreNexe(test_key, test_store_val + "aaa");
  EXPECT_EQ(1, cache_->Size());
  EXPECT_EQ(0, GetNexe(test_key).compare(test_store_val + "aaa"));
}

TEST_F(PnaclTranslationCacheTest, StoreTwo) {
  InitBackend(true);
  StoreNexe(test_key, test_store_val);
  StoreNexe(test_key + "a", test_store_val + "aaa");
  EXPECT_EQ(2, cache_->Size());
  EXPECT_EQ(0, GetNexe(test_key).compare(test_store_val));
  EXPECT_EQ(0, GetNexe(test_key + "a").compare(test_store_val + "aaa"));
}

TEST_F(PnaclTranslationCacheTest, GetMiss) {
  InitBackend(true);
  StoreNexe(test_key, test_store_val);
  TestNexeCallback load_cb;
  std::string nexe;
  cache_->GetNexe(test_key + "a", load_cb.callback());
  int rv;
  scoped_refptr<net::DrainableIOBuffer> buf(load_cb.GetResult(&rv));
  EXPECT_EQ(net::ERR_FAILED, rv);
}

}  // namespace pnacl
