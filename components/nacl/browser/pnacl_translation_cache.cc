// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/browser/pnacl_translation_cache.h"

#include <string.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_checker.h"
#include "components/nacl/common/pnacl_types.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"

using base::NumberToString;
using content::BrowserThread;

namespace {

void CloseDiskCacheEntry(disk_cache::Entry* entry) { entry->Close(); }

}  // namespace

namespace pnacl {
// This is in pnacl namespace instead of static so they can be used
// by the unit test.
const int kMaxMemCacheSize = 100 * 1024 * 1024;

//////////////////////////////////////////////////////////////////////
// Handle Reading/Writing to Cache.

// PnaclTranslationCacheEntry is a shim that provides storage for the
// 'key' and 'data' strings as the disk_cache is performing various async
// operations. It also tracks the open disk_cache::Entry
// and ensures that the entry is closed.
class PnaclTranslationCacheEntry
    : public base::RefCountedThreadSafe<PnaclTranslationCacheEntry> {
 public:
  static PnaclTranslationCacheEntry* GetReadEntry(
      base::WeakPtr<PnaclTranslationCache> cache,
      const std::string& key,
      const GetNexeCallback& callback);
  static PnaclTranslationCacheEntry* GetWriteEntry(
      base::WeakPtr<PnaclTranslationCache> cache,
      const std::string& key,
      net::DrainableIOBuffer* write_nexe,
      CompletionOnceCallback callback);

  void Start();

  // Writes:                                ---
  //                                        v  |
  // Start -> Open Existing --------------> Write ---> Close
  //                          \              ^
  //                           \             /
  //                            --> Create --
  // Reads:
  // Start -> Open --------Read ----> Close
  //                       |  ^
  //                       |__|
  enum CacheStep {
    UNINITIALIZED,
    OPEN_ENTRY,
    CREATE_ENTRY,
    TRANSFER_ENTRY,
    CLOSE_ENTRY,
    FINISHED
  };

 private:
  friend class base::RefCountedThreadSafe<PnaclTranslationCacheEntry>;
  PnaclTranslationCacheEntry(base::WeakPtr<PnaclTranslationCache> cache,
                             const std::string& key,
                             bool is_read);
  ~PnaclTranslationCacheEntry();

  // Try to open an existing entry in the backend
  void OpenEntry();
  // Create a new entry in the backend (for writes)
  void CreateEntry();
  // Write |len| bytes to the backend, starting at |offset|
  void WriteEntry(int offset, int len);
  // Read |len| bytes from the backend, starting at |offset|
  void ReadEntry(int offset, int len);
  // If there was an error, doom the entry. Then post a task to the IO
  // thread to close (and delete) it.
  void CloseEntry(int rv);
  // Call the user callback, and signal to the cache to delete this.
  void Finish(int rv);
  // Used as the callback for all operations to the backend except those that
  // first open/create entries. Handle state transitions, track bytes
  // transferred, and call the other helper methods.
  void DispatchNext(int rv);
  // Like above but for first opening or creating of |entry_|.
  void SaveEntryAndDispatchNext(disk_cache::EntryResult result);

  base::WeakPtr<PnaclTranslationCache> cache_;
  std::string key_;
  disk_cache::Entry* entry_;
  CacheStep step_;
  bool is_read_;
  GetNexeCallback read_callback_;
  CompletionOnceCallback write_callback_;
  scoped_refptr<net::DrainableIOBuffer> io_buf_;
  base::ThreadChecker thread_checker_;
  DISALLOW_COPY_AND_ASSIGN(PnaclTranslationCacheEntry);
};

// static
PnaclTranslationCacheEntry* PnaclTranslationCacheEntry::GetReadEntry(
    base::WeakPtr<PnaclTranslationCache> cache,
    const std::string& key,
    const GetNexeCallback& callback) {
  PnaclTranslationCacheEntry* entry(
      new PnaclTranslationCacheEntry(cache, key, true));
  entry->read_callback_ = callback;
  return entry;
}

// static
PnaclTranslationCacheEntry* PnaclTranslationCacheEntry::GetWriteEntry(
    base::WeakPtr<PnaclTranslationCache> cache,
    const std::string& key,
    net::DrainableIOBuffer* write_nexe,
    CompletionOnceCallback callback) {
  PnaclTranslationCacheEntry* entry(
      new PnaclTranslationCacheEntry(cache, key, false));
  entry->io_buf_ = write_nexe;
  entry->write_callback_ = std::move(callback);
  return entry;
}

PnaclTranslationCacheEntry::PnaclTranslationCacheEntry(
    base::WeakPtr<PnaclTranslationCache> cache,
    const std::string& key,
    bool is_read)
    : cache_(cache),
      key_(key),
      entry_(NULL),
      step_(UNINITIALIZED),
      is_read_(is_read) {}

PnaclTranslationCacheEntry::~PnaclTranslationCacheEntry() {
  // Ensure we have called the user's callback
  if (step_ != FINISHED) {
    if (!read_callback_.is_null()) {
      base::PostTask(FROM_HERE, {BrowserThread::IO},
                     base::BindOnce(read_callback_, net::ERR_ABORTED,
                                    scoped_refptr<net::DrainableIOBuffer>()));
    }
    if (!write_callback_.is_null()) {
      base::PostTask(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(std::move(write_callback_), net::ERR_ABORTED));
    }
  }
}

void PnaclTranslationCacheEntry::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  step_ = OPEN_ENTRY;
  OpenEntry();
}

// OpenEntry, CreateEntry, WriteEntry, ReadEntry and CloseEntry are only called
// from DispatchNext, so they know that cache_ is still valid.
void PnaclTranslationCacheEntry::OpenEntry() {
  disk_cache::EntryResult result = cache_->backend()->OpenEntry(
      key_, net::HIGHEST,
      base::BindOnce(&PnaclTranslationCacheEntry::SaveEntryAndDispatchNext,
                     this));
  if (result.net_error() != net::ERR_IO_PENDING)
    SaveEntryAndDispatchNext(std::move(result));
}

void PnaclTranslationCacheEntry::CreateEntry() {
  disk_cache::EntryResult result = cache_->backend()->CreateEntry(
      key_, net::HIGHEST,
      base::BindOnce(&PnaclTranslationCacheEntry::SaveEntryAndDispatchNext,
                     this));
  if (result.net_error() != net::ERR_IO_PENDING)
    SaveEntryAndDispatchNext(std::move(result));
}

void PnaclTranslationCacheEntry::WriteEntry(int offset, int len) {
  DCHECK(io_buf_->BytesRemaining() == len);
  int rv = entry_->WriteData(
      1, offset, io_buf_.get(), len,
      base::BindOnce(&PnaclTranslationCacheEntry::DispatchNext, this), false);
  if (rv != net::ERR_IO_PENDING)
    DispatchNext(rv);
}

void PnaclTranslationCacheEntry::ReadEntry(int offset, int len) {
  int rv = entry_->ReadData(
      1, offset, io_buf_.get(), len,
      base::BindOnce(&PnaclTranslationCacheEntry::DispatchNext, this));
  if (rv != net::ERR_IO_PENDING)
    DispatchNext(rv);
}

void PnaclTranslationCacheEntry::CloseEntry(int rv) {
  DCHECK(entry_);
  if (rv < 0) {
    LOG(ERROR) << "Failed to close entry: " << net::ErrorToString(rv);
    entry_->Doom();
  }
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&CloseDiskCacheEntry, entry_));
  Finish(rv);
}

void PnaclTranslationCacheEntry::Finish(int rv) {
  step_ = FINISHED;
  if (is_read_) {
    if (!read_callback_.is_null()) {
      base::PostTask(FROM_HERE, {BrowserThread::IO},
                     base::BindOnce(read_callback_, rv, io_buf_));
    }
  } else {
    if (!write_callback_.is_null()) {
      base::PostTask(FROM_HERE, {BrowserThread::IO},
                     base::BindOnce(std::move(write_callback_), rv));
    }
  }
  cache_->OpComplete(this);
}

void PnaclTranslationCacheEntry::DispatchNext(int rv) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!cache_)
    return;

  switch (step_) {
    case UNINITIALIZED:
    case FINISHED:
      LOG(ERROR) << "DispatchNext called uninitialized";
      break;

    case OPEN_ENTRY:
      if (rv == net::OK) {
        step_ = TRANSFER_ENTRY;
        if (is_read_) {
          int bytes_to_transfer = entry_->GetDataSize(1);
          io_buf_ = base::MakeRefCounted<net::DrainableIOBuffer>(
              base::MakeRefCounted<net::IOBuffer>(bytes_to_transfer),
              bytes_to_transfer);
          ReadEntry(0, bytes_to_transfer);
        } else {
          WriteEntry(0, io_buf_->size());
        }
      } else {
        if (rv != net::ERR_FAILED) {
          // ERROR_FAILED is what we expect if the entry doesn't exist.
          LOG(ERROR) << "OpenEntry failed: " << net::ErrorToString(rv);
        }
        if (is_read_) {
          // Just a cache miss, not necessarily an error.
          entry_ = NULL;
          Finish(rv);
        } else {
          step_ = CREATE_ENTRY;
          CreateEntry();
        }
      }
      break;

    case CREATE_ENTRY:
      if (rv == net::OK) {
        step_ = TRANSFER_ENTRY;
        WriteEntry(io_buf_->BytesConsumed(), io_buf_->BytesRemaining());
      } else {
        LOG(ERROR) << "Failed to Create Entry: " << net::ErrorToString(rv);
        Finish(rv);
      }
      break;

    case TRANSFER_ENTRY:
      if (rv < 0) {
        // We do not call DispatchNext directly if WriteEntry/ReadEntry returns
        // ERR_IO_PENDING, and the callback should not return that value either.
        LOG(ERROR) << "Failed to complete write to entry: "
                   << net::ErrorToString(rv);
        step_ = CLOSE_ENTRY;
        CloseEntry(rv);
        break;
      } else if (rv > 0) {
        io_buf_->DidConsume(rv);
        if (io_buf_->BytesRemaining() > 0) {
          is_read_
              ? ReadEntry(io_buf_->BytesConsumed(), io_buf_->BytesRemaining())
              : WriteEntry(io_buf_->BytesConsumed(), io_buf_->BytesRemaining());
          break;
        }
      }
      // rv == 0 or we fell through (i.e. we have transferred all the bytes)
      step_ = CLOSE_ENTRY;
      DCHECK(io_buf_->BytesConsumed() == io_buf_->size());
      if (is_read_)
        io_buf_->SetOffset(0);
      CloseEntry(0);
      break;

    case CLOSE_ENTRY:
      step_ = UNINITIALIZED;
      break;
  }
}

void PnaclTranslationCacheEntry::SaveEntryAndDispatchNext(
    disk_cache::EntryResult result) {
  int rv = result.net_error();
  entry_ = result.ReleaseEntry();
  DispatchNext(rv);
}

//////////////////////////////////////////////////////////////////////
void PnaclTranslationCache::OpComplete(PnaclTranslationCacheEntry* entry) {
  open_entries_.erase(entry);
}

//////////////////////////////////////////////////////////////////////
// Construction and cache backend initialization
PnaclTranslationCache::PnaclTranslationCache() : in_memory_(false) {}

PnaclTranslationCache::~PnaclTranslationCache() {}

int PnaclTranslationCache::Init(net::CacheType cache_type,
                                const base::FilePath& cache_dir,
                                int cache_size,
                                CompletionOnceCallback callback) {
  int rv = disk_cache::CreateCacheBackend(
      cache_type, net::CACHE_BACKEND_DEFAULT, cache_dir, cache_size,
      disk_cache::ResetHandling::kResetOnError, nullptr, /* dummy net log */
      &disk_cache_,
      base::BindOnce(&PnaclTranslationCache::OnCreateBackendComplete,
                     AsWeakPtr()));
  if (rv == net::ERR_IO_PENDING) {
    init_callback_ = std::move(callback);
  }
  return rv;
}

void PnaclTranslationCache::OnCreateBackendComplete(int rv) {
  if (rv < 0) {
    LOG(ERROR) << "Backend init failed:" << net::ErrorToString(rv);
  }
  // Invoke our client's callback function.
  if (!init_callback_.is_null()) {
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(std::move(init_callback_), rv));
  }
}

//////////////////////////////////////////////////////////////////////
// High-level API

void PnaclTranslationCache::StoreNexe(const std::string& key,
                                      net::DrainableIOBuffer* nexe_data,
                                      CompletionOnceCallback callback) {
  PnaclTranslationCacheEntry* entry = PnaclTranslationCacheEntry::GetWriteEntry(
      AsWeakPtr(), key, nexe_data, std::move(callback));
  open_entries_[entry] = entry;
  entry->Start();
}

void PnaclTranslationCache::GetNexe(const std::string& key,
                                    const GetNexeCallback& callback) {
  PnaclTranslationCacheEntry* entry =
      PnaclTranslationCacheEntry::GetReadEntry(AsWeakPtr(), key, callback);
  open_entries_[entry] = entry;
  entry->Start();
}

int PnaclTranslationCache::InitOnDisk(const base::FilePath& cache_directory,
                                      CompletionOnceCallback callback) {
  in_memory_ = false;
  return Init(net::PNACL_CACHE, cache_directory, 0 /* auto size */,
              std::move(callback));
}

int PnaclTranslationCache::InitInMemory(CompletionOnceCallback callback) {
  in_memory_ = true;
  return Init(net::MEMORY_CACHE, base::FilePath(), kMaxMemCacheSize,
              std::move(callback));
}

int PnaclTranslationCache::Size() {
  if (!disk_cache_)
    return -1;
  return disk_cache_->GetEntryCount();
}

// Beware that any changes to this function or to PnaclCacheInfo will
// effectively invalidate existing translation cache entries.

// static
std::string PnaclTranslationCache::GetKey(const nacl::PnaclCacheInfo& info) {
  if (!info.pexe_url.is_valid() || info.abi_version < 0 || info.opt_level < 0 ||
      info.extra_flags.size() > 512)
    return std::string();
  std::string retval("ABI:");
  retval += NumberToString(info.abi_version) + ";" +
            "opt:" + NumberToString(info.opt_level) +
            (info.use_subzero ? "subzero;" : ";") + "URL:";
  // Filter the username, password, and ref components from the URL
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearRef();
  GURL key_url(info.pexe_url.ReplaceComponents(replacements));
  retval += key_url.spec() + ";";
  // You would think that there is already code to format base::Time values
  // somewhere, but I haven't found it yet. In any case, doing it ourselves
  // here means we can keep the format stable.
  base::Time::Exploded exploded;
  info.last_modified.UTCExplode(&exploded);
  if (info.last_modified.is_null() || !exploded.HasValidValues()) {
    memset(&exploded, 0, sizeof(exploded));
  }
  retval += "modified:" + NumberToString(exploded.year) + ":" +
            NumberToString(exploded.month) + ":" +
            NumberToString(exploded.day_of_month) + ":" +
            NumberToString(exploded.hour) + ":" +
            NumberToString(exploded.minute) + ":" +
            NumberToString(exploded.second) + ":" +
            NumberToString(exploded.millisecond) + ":UTC;";
  retval += "etag:" + info.etag + ";";
  retval += "sandbox:" + info.sandbox_isa + ";";
  retval += "extra_flags:" + info.extra_flags + ";";
  return retval;
}

int PnaclTranslationCache::DoomEntriesBetween(base::Time initial,
                                              base::Time end,
                                              CompletionOnceCallback callback) {
  return disk_cache_->DoomEntriesBetween(initial, end, std::move(callback));
}

}  // namespace pnacl
