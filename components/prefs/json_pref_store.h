// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_JSON_PREF_STORE_H_
#define COMPONENTS_PREFS_JSON_PREF_STORE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/prefs_export.h"

class PrefFilter;

namespace base {
class FilePath;
class JsonPrefStoreCallbackTest;
class JsonPrefStoreLossyWriteTest;
class SequencedTaskRunner;
class WriteCallbacksObserver;
}  // namespace base

// A writable PrefStore implementation that is used for user preferences.
class COMPONENTS_PREFS_EXPORT JsonPrefStore final
    : public PersistentPrefStore,
      public base::ImportantFileWriter::DataSerializer,
      public base::ImportantFileWriter::BackgroundDataSerializer {
 public:
  struct ReadResult;

  // A pair of callbacks to call before and after the preference file is written
  // to disk.
  using OnWriteCallbackPair =
      std::pair<base::OnceClosure, base::OnceCallback<void(bool success)>>;

  // |pref_filename| is the path to the file to read prefs from. It is incorrect
  // to create multiple JsonPrefStore with the same |pref_filename|.
  // |file_task_runner| is used for asynchronous reads and writes. It must
  // have the base::TaskShutdownBehavior::BLOCK_SHUTDOWN and base::MayBlock()
  // traits. Unless external tasks need to run on the same sequence as
  // JsonPrefStore tasks, keep the default value.
  // The initial read is done synchronously, the TaskPriority is thus only used
  // for flushes to disks and BEST_EFFORT is therefore appropriate. Priority of
  // remaining BEST_EFFORT+BLOCK_SHUTDOWN tasks is bumped by the ThreadPool on
  // shutdown. However, some shutdown use cases happen without
  // ThreadPoolInstance::Shutdown() (e.g. ChromeRestartRequest::Start() and
  // BrowserProcessImpl::EndSession()) and we must thus unfortunately make this
  // USER_VISIBLE until we solve https://crbug.com/747495 to allow bumping
  // priority of a sequence on demand.
  JsonPrefStore(const base::FilePath& pref_filename,
                std::unique_ptr<PrefFilter> pref_filter = nullptr,
                scoped_refptr<base::SequencedTaskRunner> file_task_runner =
                    base::ThreadPool::CreateSequencedTaskRunner(
                        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                         base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
                bool read_only = false);

  JsonPrefStore(const JsonPrefStore&) = delete;
  JsonPrefStore& operator=(const JsonPrefStore&) = delete;

  // PrefStore overrides:
  bool GetValue(std::string_view key,
                const base::Value** result) const override;
  base::Value::Dict GetValues() const override;
  void AddObserver(PrefStore::Observer* observer) override;
  void RemoveObserver(PrefStore::Observer* observer) override;
  bool HasObservers() const override;
  bool IsInitializationComplete() const override;

  // PersistentPrefStore overrides:
  bool GetMutableValue(std::string_view key, base::Value** result) override;
  void SetValue(std::string_view key,
                base::Value value,
                uint32_t flags) override;
  void SetValueSilently(std::string_view key,
                        base::Value value,
                        uint32_t flags) override;
  void RemoveValue(std::string_view key, uint32_t flags) override;
  bool ReadOnly() const override;
  PrefReadError GetReadError() const override;
  bool HasReadErrorDelegate() const override;
  // Note this method may be asynchronous if this instance has a |pref_filter_|
  // in which case it will return PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE.
  // See details in pref_filter.h.
  PrefReadError ReadPrefs() override;
  void ReadPrefsAsync(ReadErrorDelegate* error_delegate) override;
  void CommitPendingWrite(
      base::OnceClosure reply_callback = base::OnceClosure(),
      base::OnceClosure synchronous_done_callback =
          base::OnceClosure()) override;
  void SchedulePendingLossyWrites() override;
  void ReportValueChanged(std::string_view key, uint32_t flags) override;

  // Just like RemoveValue(), but doesn't notify observers. Used when doing some
  // cleanup that shouldn't otherwise alert observers.
  void RemoveValueSilently(std::string_view key, uint32_t flags);

  // Just like RemoveValue(), but removes all the prefs that start with
  // |prefix|. Used for pref-initialization cleanup.
  void RemoveValuesByPrefixSilently(std::string_view prefix) override;

  // Registers |on_next_successful_write_reply| to be called once, on the next
  // successful write event of |writer_|.
  // |on_next_successful_write_reply| will be called on the thread from which
  // this method is called and does not need to be thread safe.
  void RegisterOnNextSuccessfulWriteReply(
      base::OnceClosure on_next_successful_write_reply);

  void OnStoreDeletionFromDisk() override;

  base::WeakPtr<JsonPrefStore> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

#if defined(UNIT_TEST)
  base::ImportantFileWriter& get_writer() { return writer_; }
#endif

 private:
  friend class base::JsonPrefStoreCallbackTest;
  friend class base::JsonPrefStoreLossyWriteTest;
  friend class base::WriteCallbacksObserver;

  ~JsonPrefStore() override;

  // Perform pre-serialization bookkeeping common to either serialization flow
  // (main thread or background thread).
  void PerformPreserializationTasks();

  // If |write_success| is true, runs |on_next_successful_write_|.
  // Otherwise, re-registers |on_next_successful_write_|.
  void RunOrScheduleNextSuccessfulWriteCallback(bool write_success);

  // Handles the result of a write with result |write_success|. Runs
  // |on_next_write_callback| on the current thread and posts
  // |on_next_write_reply| on |reply_task_runner|.
  static void PostWriteCallback(
      base::OnceCallback<void(bool success)> on_next_write_callback,
      base::OnceCallback<void(bool success)> on_next_write_reply,
      scoped_refptr<base::SequencedTaskRunner> reply_task_runner,
      bool write_success);

  // Registers the |callbacks| pair to be called once synchronously before and
  // after, respectively, the next write event of |writer_|.
  // Both callbacks must be thread-safe.
  void RegisterOnNextWriteSynchronousCallbacks(OnWriteCallbackPair callbacks);

  // This method is called after the JSON file has been read.  It then hands
  // |value| (or an empty dictionary in some read error cases) to the
  // |pref_filter| if one is set. It also gives a callback pointing at
  // FinalizeFileRead() to that |pref_filter_| which is then responsible for
  // invoking it when done. If there is no |pref_filter_|, FinalizeFileRead()
  // is invoked directly.
  void OnFileRead(std::unique_ptr<ReadResult> read_result);

  // ImportantFileWriter::DataSerializer overrides:
  std::optional<std::string> SerializeData() override;
  // ImportantFileWriter::BackgroundDataSerializer implementation.
  base::ImportantFileWriter::BackgroundDataProducerCallback
  GetSerializedDataProducerForBackgroundSequence() override;

  // This method is called after the JSON file has been read and the result has
  // potentially been intercepted and modified by |pref_filter_|.
  // |initialization_successful| is pre-determined by OnFileRead() and should
  // be used when reporting OnInitializationCompleted().
  // |schedule_write| indicates whether a write should be immediately scheduled
  // (typically because the |pref_filter_| has already altered the |prefs|) --
  // this will be ignored if this store is read-only.
  void FinalizeFileRead(bool initialization_successful,
                        base::Value::Dict prefs,
                        bool schedule_write);

  // Schedule a write with the file writer as long as |flags| doesn't contain
  // WriteablePrefStore::LOSSY_PREF_WRITE_FLAG.
  void ScheduleWrite(uint32_t flags);

  const base::FilePath path_;
  const scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  base::Value::Dict prefs_;

  bool read_only_;

  // Helper for safely writing pref data.
  base::ImportantFileWriter writer_;

  std::unique_ptr<PrefFilter> pref_filter_;
  base::ObserverList<PrefStore::Observer, true> observers_;

  // Optional so we can differentiate `nullopt` from `nullptr`.
  std::optional<std::unique_ptr<ReadErrorDelegate>> error_delegate_;

  bool initialized_;
  bool filtering_in_progress_;
  bool pending_lossy_write_;
  PrefReadError read_error_;

  std::set<std::string> keys_need_empty_value_;

  bool has_pending_write_reply_ = true;
  base::OnceClosure on_next_successful_write_reply_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<JsonPrefStore> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_PREFS_JSON_PREF_STORE_H_
