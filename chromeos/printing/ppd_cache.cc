// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/printing/ppd_cache.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/printing/printing_constants.h"
#include "crypto/sha2.h"
#include "net/base/io_buffer.h"
#include "net/filter/gzip_header.h"

namespace chromeos {
namespace {

// Return the (full) path to the file we expect to find the given key at.
base::FilePath FilePathForKey(const base::FilePath& base_dir,
                              const std::string& key) {
  return base_dir.Append(base::HexEncode(crypto::SHA256HashString(key)));
}

// If the cache doesn't already exist, create it.
void MaybeCreateCache(const base::FilePath& base_dir) {
  if (!base::PathExists(base_dir)) {
    base::CreateDirectory(base_dir);
  }
}

// Find implementation, blocks on file access.  Must be run on a thread that
// allows I/O.
PpdCache::FindResult FindImpl(const base::FilePath& cache_dir,
                              const std::string& key) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  PpdCache::FindResult result;
  result.success = false;
  if (!base::PathExists(cache_dir)) {
    // If the cache dir was missing, we'll miss anyway.
    return result;
  }

  base::File file(FilePathForKey(cache_dir, key),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);

  base::File::Info info;
  if (!file.IsValid() || !file.GetInfo(&info))
    return result;

  if (info.size < static_cast<int64_t>(crypto::kSHA256Length) ||
      info.size > static_cast<int64_t>(kMaxPpdSizeBytes) +
                      static_cast<int64_t>(crypto::kSHA256Length)) {
    return result;
  }

  std::vector<char> buf(info.size);
  if (file.ReadAtCurrentPos(buf.data(), info.size) != info.size)
    return result;

  std::string_view contents(buf.data(), info.size - crypto::kSHA256Length);
  std::string_view checksum(buf.data() + info.size - crypto::kSHA256Length,
                            crypto::kSHA256Length);
  if (crypto::SHA256HashString(contents) != checksum) {
    LOG(ERROR) << "Bad checksum for cache key " << key;
    return result;
  }

  result.success = true;
  result.age = base::Time::Now() - info.last_modified;
  result.contents = std::string(contents);
  return result;
}

// Store implementation, blocks on file access.  Must be run on a thread that
// allows I/O.  If |age| is non-zero, explicitly set the age of the resulting
// file to be |age| before Now.
void StoreImpl(const base::FilePath& cache_dir,
               const std::string& key,
               const std::string& contents,
               base::TimeDelta age) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  MaybeCreateCache(cache_dir);
  if (contents.size() > kMaxPpdSizeBytes) {
    LOG(ERROR) << "Ignoring attempt to cache large object";
    return;
  }

  auto path = FilePathForKey(cache_dir, key);
  base::File file(path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  std::string checksum = crypto::SHA256HashString(contents);
  if (!file.IsValid() ||
      !file.WriteAtCurrentPosAndCheck(base::as_byte_span(contents)) ||
      !file.WriteAtCurrentPosAndCheck(base::as_byte_span(checksum))) {
    LOG(ERROR) << "Failed to create ppd cache file";
    file.Close();
    if (!base::DeleteFile(path)) {
      LOG(ERROR) << "Failed to cleanup failed creation.";
    }
    return;
  }

  // Successfully wrote the file, adjust the age if requested.
  if (!age.is_zero()) {
    base::Time mod_time = base::Time::Now() - age;
    file.SetTimes(mod_time, mod_time);
  }
}

// Implementation of the PpdCache that uses two separate task runners for Store
// and Fetch since the two operations have different priorities. Note that the
// two operations are not sequenced so there should be no expectation that a
// call to Find will return a file that was previously Stored until the Store
// callback is run.
class PpdCacheImpl : public PpdCache {
 public:
  explicit PpdCacheImpl(
      const base::FilePath& cache_base_dir,
      scoped_refptr<base::SequencedTaskRunner> fetch_task_runner,
      scoped_refptr<base::SequencedTaskRunner> store_task_runner)
      : cache_base_dir_(cache_base_dir),
        fetch_task_runner_(std::move(fetch_task_runner)),
        store_task_runner_(std::move(store_task_runner)) {}

  PpdCacheImpl(const PpdCacheImpl&) = delete;
  PpdCacheImpl& operator=(const PpdCacheImpl&) = delete;

  // Public API functions.
  void Find(const std::string& key, FindCallback cb) override {
    fetch_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&FindImpl, cache_base_dir_, key),
        std::move(cb));
  }

  // Store the given contents at the given key.  If cb is non-null, it will
  // be invoked on completion.
  void Store(const std::string& key, const std::string& contents) override {
    store_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&StoreImpl, cache_base_dir_, key, contents,
                                  base::TimeDelta()));
  }

  void StoreForTesting(const std::string& key,
                       const std::string& contents,
                       base::TimeDelta age) override {
    store_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&StoreImpl, cache_base_dir_, key, contents, age));
  }

 private:
  ~PpdCacheImpl() override = default;

  base::FilePath cache_base_dir_;
  scoped_refptr<base::SequencedTaskRunner> fetch_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> store_task_runner_;
};

}  // namespace

// static
scoped_refptr<PpdCache> PpdCache::Create(const base::FilePath& cache_base_dir) {
  return scoped_refptr<PpdCache>(
      new PpdCacheImpl(cache_base_dir,
                       base::ThreadPool::CreateSequencedTaskRunner(
                           {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
                            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}),
                       base::ThreadPool::CreateSequencedTaskRunner(
                           {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
                            base::TaskShutdownBehavior::BLOCK_SHUTDOWN})));
}

scoped_refptr<PpdCache> PpdCache::CreateForTesting(
    const base::FilePath& cache_base_dir,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  return scoped_refptr<PpdCache>(
      new PpdCacheImpl(cache_base_dir, io_task_runner, io_task_runner));
}

}  // namespace chromeos
