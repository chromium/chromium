// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_BROWSER_PNACL_HOST_H_
#define COMPONENTS_NACL_BROWSER_PNACL_HOST_H_

#include <stddef.h>

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_checker.h"
#include "components/nacl/browser/nacl_file_host.h"
#include "components/nacl/common/pnacl_types.h"
#include "ipc/ipc_platform_file.h"

namespace net {
class DrainableIOBuffer;
}

namespace pnacl {

class PnaclHostTest;
class PnaclHostTestDisk;
class PnaclTranslationCache;

// Shared state (translation cache) and common utilities (temp file creation)
// for all PNaCl translations. Unless otherwise specified, all methods should be
// called on the IO thread.
class PnaclHost {
 public:
  typedef base::Callback<void(base::File)> TempFileCallback;
  typedef base::Callback<void(const base::File&, bool is_hit)> NexeFdCallback;

  // Gets the PnaclHost singleton instance (creating it if necessary).
  // PnaclHost is a singleton because there is only one translation cache, and
  // so that the BrowsingDataRemover can clear it even if no translation has
  // ever been started.
  static PnaclHost* GetInstance();

  // The PnaclHost instance is intentionally leaked on shutdown. DeInitIfSafe()
  // attempts to cleanup |disk_cache_| earlier, but if it fails to do so in
  // time, it will be too late when AtExitManager kicks in anway so subscribing
  // to it is useless.
  ~PnaclHost() = delete;

  // Initialize cache backend. GetNexeFd will also initialize the backend if
  // necessary, but calling Init ahead of time will minimize the latency.
  void Init();

  // Creates a temporary file that will be deleted when the last handle
  // is closed, or earlier. Returns a PlatformFile handle.
  void CreateTemporaryFile(TempFileCallback cb);

  // Create a temporary file, which will be deleted by the time the last
  // handle is closed (or earlier on POSIX systems), to use for the nexe
  // with the cache information given in |cache_info|. The specific instance
  // is identified by the combination of |render_process_id| and |pp_instance|.
  // Returns by calling |cb| with a PlatformFile handle.
  // If the nexe is already present
  // in the cache, |is_hit| is set to true and the contents of the nexe
  // have been copied into the temporary file. Otherwise |is_hit| is set to
  // false and the temporary file will be writeable.
  // Currently the implementation is a stub, which always sets is_hit to false
  // and calls the implementation of CreateTemporaryFile.
  // If the cache request was a miss, the caller is expected to call
  // TranslationFinished after it finishes translation to allow the nexe to be
  // stored in the cache.
  // The returned temp fd may be closed at any time by PnaclHost, so it should
  // be duplicated (e.g. with IPC::GetPlatformFileForTransit) before the
  // callback returns.
  // If |is_incognito| is true, the nexe will not be stored
  // in the cache, but the renderer is still expected to call
  // TranslationFinished.
  void GetNexeFd(int render_process_id,
                 int render_view_id,
                 int pp_instance,
                 bool is_incognito,
                 const nacl::PnaclCacheInfo& cache_info,
                 const NexeFdCallback& cb);

  // Called after the translation of a pexe instance identified by
  // |render_process_id| and |pp_instance| finishes. If |success| is true,
  // store the nexe translated for the instance in the cache.
  void TranslationFinished(int render_process_id,
                           int pp_instance,
                           bool success);

  // Called when the renderer identified by |render_process_id| is closing.
  // Clean up any outstanding translations for that renderer. If there are no
  // more pending translations, the backend is freed, allowing it to flush.
  void RendererClosing(int render_process_id);

  // Doom all entries between |initial_time| and |end_time|. Like disk_cache_,
  // PnaclHost supports supports unbounded deletes in either direction by using
  // null Time values for either argument. |callback| will be called on the UI
  // thread when finished.
  void ClearTranslationCacheEntriesBetween(base::Time initial_time,
                                           base::Time end_time,
                                           base::OnceClosure callback);

  // Return the number of tracked translations or FD requests currently pending.
  size_t pending_translations() {
    DCHECK(thread_checker_.CalledOnValidThread());
    return pending_translations_.size();
  }

 private:
  friend class FileProxy;
  friend class PnaclHostTest;
  friend class PnaclHostTestDisk;
  enum CacheState {
    CacheUninitialized,
    CacheInitializing,
    CacheReady
  };
  class PendingTranslation {
   public:
    PendingTranslation();
    PendingTranslation(const PendingTranslation& other);
    ~PendingTranslation();
    base::ProcessHandle process_handle;
    int render_view_id;
    base::File* nexe_fd;
    bool got_nexe_fd;
    bool got_cache_reply;
    bool got_cache_hit;
    bool is_incognito;
    scoped_refptr<net::DrainableIOBuffer> nexe_read_buffer;
    NexeFdCallback callback;
    std::string cache_key;
    nacl::PnaclCacheInfo cache_info;
  };

  typedef std::pair<int, int> TranslationID;
  typedef std::map<TranslationID, PendingTranslation> PendingTranslationMap;

  PnaclHost();

  static bool TranslationMayBeCached(
      const PendingTranslationMap::iterator& entry);

  void InitForTest(base::FilePath temp_dir, bool in_memory);
  void OnCacheInitialized(int net_error);

  static void DoCreateTemporaryFile(base::FilePath temp_dir_,
                                    TempFileCallback cb);

  // GetNexeFd common steps
  void SendCacheQueryAndTempFileRequest(const std::string& key,
                                        const TranslationID& id);
  void OnCacheQueryReturn(const TranslationID& id,
                          int net_error,
                          scoped_refptr<net::DrainableIOBuffer> buffer);
  void OnTempFileReturn(const TranslationID& id, base::File file);
  void CheckCacheQueryReady(const PendingTranslationMap::iterator& entry);

  // GetNexeFd miss path
  void ReturnMiss(const PendingTranslationMap::iterator& entry);
  static scoped_refptr<net::DrainableIOBuffer> CopyFileToBuffer(
      std::unique_ptr<base::File> file);
  void StoreTranslatedNexe(TranslationID id,
                           scoped_refptr<net::DrainableIOBuffer>);
  void OnTranslatedNexeStored(const TranslationID& id, int net_error);
  void RequeryMatchingTranslations(const std::string& key);

  // GetNexeFd hit path
  void OnBufferCopiedToTempFile(const TranslationID& id,
                                std::unique_ptr<base::File> file,
                                int file_error);

  void OnEntriesDoomed(base::OnceClosure callback, int net_error);

  void DeInitIfSafe();

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_ =
      base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                       base::TaskPriority::USER_VISIBLE});

  // Operations which are pending with the cache backend, which we should
  // wait for before destroying it (see comment on DeInitIfSafe).
  int pending_backend_operations_ = 0;
  CacheState cache_state_ = CacheUninitialized;
  base::FilePath temp_dir_;
  std::unique_ptr<PnaclTranslationCache> disk_cache_;
  PendingTranslationMap pending_translations_;
  base::ThreadChecker thread_checker_;
  DISALLOW_COPY_AND_ASSIGN(PnaclHost);
};

}  // namespace pnacl

#endif  // COMPONENTS_NACL_BROWSER_PNACL_HOST_H_
