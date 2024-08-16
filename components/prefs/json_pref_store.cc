// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/json_pref_store.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "components/prefs/pref_filter.h"

// Result returned from internal read tasks.
struct JsonPrefStore::ReadResult {
 public:
  ReadResult();
  ~ReadResult();
  ReadResult(const ReadResult&) = delete;
  ReadResult& operator=(const ReadResult&) = delete;

  std::unique_ptr<base::Value> value;
  PrefReadError error = PersistentPrefStore::PREF_READ_ERROR_NONE;
  bool no_dir = false;
  size_t num_bytes_read = 0u;
};

JsonPrefStore::ReadResult::ReadResult() = default;
JsonPrefStore::ReadResult::~ReadResult() = default;

namespace {

// Some extensions we'll tack on to copies of the Preferences files.
const base::FilePath::CharType kBadExtension[] = FILE_PATH_LITERAL("bad");

// Report a key that triggers a write into the Preferences files.
void ReportKeyChangedToUMA(std::string_view key) {
  // Truncate the sign bit. Even if the type is unsigned, UMA displays 32-bit
  // negative numbers.
  const uint32_t hash = base::PersistentHash(key) & 0x7FFFFFFF;
  UMA_HISTOGRAM_SPARSE("Prefs.JSonStore.SetValueKey", hash);
}

bool BackupPrefsFile(const base::FilePath& path) {
  const base::FilePath bad = path.ReplaceExtension(kBadExtension);
  const bool bad_existed = base::PathExists(bad);
  base::Move(path, bad);
  return bad_existed;
}

PersistentPrefStore::PrefReadError HandleReadErrors(
    const base::Value* value,
    const base::FilePath& path,
    int error_code,
    const std::string& error_msg) {
  if (!value) {
    DVLOG(1) << "Error while loading JSON file: " << error_msg
             << ", file: " << path.value();
    switch (error_code) {
      case JSONFileValueDeserializer::JSON_ACCESS_DENIED:
        return PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED;
      case JSONFileValueDeserializer::JSON_CANNOT_READ_FILE:
        return PersistentPrefStore::PREF_READ_ERROR_FILE_OTHER;
      case JSONFileValueDeserializer::JSON_FILE_LOCKED:
        return PersistentPrefStore::PREF_READ_ERROR_FILE_LOCKED;
      case JSONFileValueDeserializer::JSON_NO_SUCH_FILE:
        return PersistentPrefStore::PREF_READ_ERROR_NO_FILE;
      default:
        // JSON errors indicate file corruption of some sort.
        // Since the file is corrupt, move it to the side and continue with
        // empty preferences.  This will result in them losing their settings.
        // We keep the old file for possible support and debugging assistance
        // as well as to detect if they're seeing these errors repeatedly.
        // TODO(erikkay) Instead, use the last known good file.
        // If they've ever had a parse error before, put them in another bucket.
        // TODO(erikkay) if we keep this error checking for very long, we may
        // want to differentiate between recent and long ago errors.
        const bool bad_existed = BackupPrefsFile(path);
        return bad_existed ? PersistentPrefStore::PREF_READ_ERROR_JSON_REPEAT
                           : PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE;
    }
  }
  if (!value->is_dict())
    return PersistentPrefStore::PREF_READ_ERROR_JSON_TYPE;
  return PersistentPrefStore::PREF_READ_ERROR_NONE;
}

std::unique_ptr<JsonPrefStore::ReadResult> ReadPrefsFromDisk(
    const base::FilePath& path) {
  int error_code;
  std::string error_msg;
  auto read_result = std::make_unique<JsonPrefStore::ReadResult>();
  JSONFileValueDeserializer deserializer(path);
  read_result->value = deserializer.Deserialize(&error_code, &error_msg);
  read_result->error =
      HandleReadErrors(read_result->value.get(), path, error_code, error_msg);
  read_result->no_dir = !base::PathExists(path.DirName());
  read_result->num_bytes_read = deserializer.get_last_read_size();

  return read_result;
}

// Returns the a histogram suffix for a few allowlisted JsonPref files.
const char* GetHistogramSuffix(const base::FilePath& path) {
  std::string spaceless_basename;
  base::ReplaceChars(path.BaseName().MaybeAsASCII(), " ", "_",
                     &spaceless_basename);
  // Entries here should be reflected in the ImportantFileClients variant in
  // histograms.xml.
  static constexpr std::array<const char*, 4> kAllowList{
      "Secure_Preferences", "Preferences", "Local_State", "AccountPreferences"};
  auto it = base::ranges::find(kAllowList, spaceless_basename);
  return it != kAllowList.end() ? *it : "";
}

std::optional<std::string> DoSerialize(base::ValueView value,
                                       const base::FilePath& path) {
  std::string output;
  if (!base::JSONWriter::Write(value, &output)) {
    // Failed to serialize prefs file. Backup the existing prefs file and
    // crash.
    BackupPrefsFile(path);
    NOTREACHED() << "Failed to serialize preferences : " << path
                 << "\nBacked up under "
                 << path.ReplaceExtension(kBadExtension);
  }
  return output;
}

}  // namespace

JsonPrefStore::JsonPrefStore(
    const base::FilePath& pref_filename,
    std::unique_ptr<PrefFilter> pref_filter,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    bool read_only)
    : path_(pref_filename),
      file_task_runner_(std::move(file_task_runner)),
      read_only_(read_only),
      writer_(pref_filename,
              file_task_runner_,
              GetHistogramSuffix(pref_filename)),
      pref_filter_(std::move(pref_filter)),
      initialized_(false),
      filtering_in_progress_(false),
      pending_lossy_write_(false),
      read_error_(PREF_READ_ERROR_NONE),
      has_pending_write_reply_(false) {
  DCHECK(!path_.empty());
}

bool JsonPrefStore::GetValue(std::string_view key,
                             const base::Value** result) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value* tmp = prefs_.FindByDottedPath(key);
  if (!tmp)
    return false;

  if (result)
    *result = tmp;
  return true;
}

base::Value::Dict JsonPrefStore::GetValues() const {
  return prefs_.Clone();
}

void JsonPrefStore::AddObserver(PrefStore::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.AddObserver(observer);
}

void JsonPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.RemoveObserver(observer);
}

bool JsonPrefStore::HasObservers() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !observers_.empty();
}

bool JsonPrefStore::IsInitializationComplete() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return initialized_;
}

bool JsonPrefStore::GetMutableValue(std::string_view key,
                                    base::Value** result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Value* tmp = prefs_.FindByDottedPath(key);
  if (!tmp)
    return false;

  if (result)
    *result = tmp;
  return true;
}

void JsonPrefStore::SetValue(std::string_view key,
                             base::Value value,
                             uint32_t flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Value* old_value = prefs_.FindByDottedPath(key);
  if (!old_value || value != *old_value) {
    prefs_.SetByDottedPath(key, std::move(value));
    ReportValueChanged(key, flags);
    ReportKeyChangedToUMA(key);
  }
}

void JsonPrefStore::SetValueSilently(std::string_view key,
                                     base::Value value,
                                     uint32_t flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Value* old_value = prefs_.FindByDottedPath(key);
  if (!old_value || value != *old_value) {
    prefs_.SetByDottedPath(key, std::move(value));
    ScheduleWrite(flags);
    ReportKeyChangedToUMA(key);
  }
}

void JsonPrefStore::RemoveValue(std::string_view key, uint32_t flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (prefs_.RemoveByDottedPath(key)) {
    ReportValueChanged(key, flags);
  }
}

void JsonPrefStore::RemoveValueSilently(std::string_view key, uint32_t flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  prefs_.RemoveByDottedPath(key);
  ScheduleWrite(flags);
}

void JsonPrefStore::RemoveValuesByPrefixSilently(std::string_view prefix) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveValueSilently(prefix, /*flags*/ 0);
}

bool JsonPrefStore::ReadOnly() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return read_only_;
}

PersistentPrefStore::PrefReadError JsonPrefStore::GetReadError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return read_error_;
}

PersistentPrefStore::PrefReadError JsonPrefStore::ReadPrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  OnFileRead(ReadPrefsFromDisk(path_));
  return filtering_in_progress_ ? PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE
                                : read_error_;
}

void JsonPrefStore::ReadPrefsAsync(ReadErrorDelegate* error_delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  initialized_ = false;
  error_delegate_.emplace(error_delegate);

  // Weakly binds the read task so that it doesn't kick in during shutdown.
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadPrefsFromDisk, path_),
      base::BindOnce(&JsonPrefStore::OnFileRead,
                     weak_ptr_factory_.GetWeakPtr()));
}

void JsonPrefStore::CommitPendingWrite(
    base::OnceClosure reply_callback,
    base::OnceClosure synchronous_done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Schedule a write for any lossy writes that are outstanding to ensure that
  // they get flushed when this function is called.
  SchedulePendingLossyWrites();

  if (writer_.HasPendingWrite() && !read_only_)
    writer_.DoScheduledWrite();

  // Since disk operations occur on |file_task_runner_|, the reply of a task
  // posted to |file_task_runner_| will run after currently pending disk
  // operations. Also, by definition of PostTaskAndReply(), the reply (in the
  // |reply_callback| case will run on the current sequence.

  if (synchronous_done_callback) {
    file_task_runner_->PostTask(FROM_HERE,
                                std::move(synchronous_done_callback));
  }

  if (reply_callback) {
    file_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                        std::move(reply_callback));
  }
}

void JsonPrefStore::SchedulePendingLossyWrites() {
  if (pending_lossy_write_)
    writer_.ScheduleWrite(this);
}

void JsonPrefStore::ReportValueChanged(std::string_view key, uint32_t flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pref_filter_)
    pref_filter_->FilterUpdate(key);

  for (PrefStore::Observer& observer : observers_)
    observer.OnPrefValueChanged(key);

  ScheduleWrite(flags);
}

void JsonPrefStore::PerformPreserializationTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_lossy_write_ = false;
  if (pref_filter_) {
    OnWriteCallbackPair callbacks = pref_filter_->FilterSerializeData(prefs_);
    if (!callbacks.first.is_null() || !callbacks.second.is_null())
      RegisterOnNextWriteSynchronousCallbacks(std::move(callbacks));
  }
}

void JsonPrefStore::RunOrScheduleNextSuccessfulWriteCallback(
    bool write_success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  has_pending_write_reply_ = false;
  if (!on_next_successful_write_reply_.is_null()) {
    base::OnceClosure on_successful_write =
        std::move(on_next_successful_write_reply_);
    if (write_success) {
      std::move(on_successful_write).Run();
    } else {
      RegisterOnNextSuccessfulWriteReply(std::move(on_successful_write));
    }
  }
}

// static
void JsonPrefStore::PostWriteCallback(
    base::OnceCallback<void(bool success)> on_next_write_callback,
    base::OnceCallback<void(bool success)> on_next_write_reply,
    scoped_refptr<base::SequencedTaskRunner> reply_task_runner,
    bool write_success) {
  if (!on_next_write_callback.is_null())
    std::move(on_next_write_callback).Run(write_success);

  // We can't run |on_next_write_reply| on the current thread. Bounce back to
  // the |reply_task_runner| which is the correct sequenced thread.
  reply_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_next_write_reply), write_success));
}

void JsonPrefStore::RegisterOnNextSuccessfulWriteReply(
    base::OnceClosure on_next_successful_write_reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(on_next_successful_write_reply_.is_null());

  on_next_successful_write_reply_ = std::move(on_next_successful_write_reply);

  // If there are pending callbacks, avoid erasing them; the reply will be used
  // as we set |on_next_successful_write_reply_|. Otherwise, setup a reply with
  // an empty callback.
  if (!has_pending_write_reply_) {
    has_pending_write_reply_ = true;
    writer_.RegisterOnNextWriteCallbacks(
        base::OnceClosure(),
        base::BindOnce(
            &PostWriteCallback, base::OnceCallback<void(bool success)>(),
            base::BindOnce(
                &JsonPrefStore::RunOrScheduleNextSuccessfulWriteCallback,
                weak_ptr_factory_.GetWeakPtr()),
            base::SequencedTaskRunner::GetCurrentDefault()));
  }
}

void JsonPrefStore::RegisterOnNextWriteSynchronousCallbacks(
    OnWriteCallbackPair callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  has_pending_write_reply_ = true;

  writer_.RegisterOnNextWriteCallbacks(
      std::move(callbacks.first),
      base::BindOnce(
          &PostWriteCallback, std::move(callbacks.second),
          base::BindOnce(
              &JsonPrefStore::RunOrScheduleNextSuccessfulWriteCallback,
              weak_ptr_factory_.GetWeakPtr()),
          base::SequencedTaskRunner::GetCurrentDefault()));
}

void JsonPrefStore::OnStoreDeletionFromDisk() {
  if (pref_filter_)
    pref_filter_->OnStoreDeletionFromDisk();
}

void JsonPrefStore::OnFileRead(std::unique_ptr<ReadResult> read_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(read_result);

  base::Value::Dict unfiltered_prefs;

  read_error_ = read_result->error;

  bool initialization_successful = !read_result->no_dir;

  if (initialization_successful) {
    switch (read_error_) {
      case PREF_READ_ERROR_ACCESS_DENIED:
      case PREF_READ_ERROR_FILE_OTHER:
      case PREF_READ_ERROR_FILE_LOCKED:
      case PREF_READ_ERROR_JSON_TYPE:
      case PREF_READ_ERROR_FILE_NOT_SPECIFIED:
        read_only_ = true;
        break;
      case PREF_READ_ERROR_NONE:
        DCHECK(read_result->value);
        DCHECK(read_result->value->is_dict());
        writer_.set_previous_data_size(read_result->num_bytes_read);
        unfiltered_prefs = std::move(*read_result->value).TakeDict();
        break;
      case PREF_READ_ERROR_NO_FILE:
        // If the file just doesn't exist, maybe this is first run.  In any case
        // there's no harm in writing out default prefs in this case.
      case PREF_READ_ERROR_JSON_PARSE:
      case PREF_READ_ERROR_JSON_REPEAT:
        break;
      case PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE:
        // This is a special error code to be returned by ReadPrefs when it
        // can't complete synchronously, it should never be returned by the read
        // operation itself.
      case PREF_READ_ERROR_MAX_ENUM:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  if (pref_filter_) {
    filtering_in_progress_ = true;
    PrefFilter::PostFilterOnLoadCallback post_filter_on_load_callback(
        base::BindOnce(&JsonPrefStore::FinalizeFileRead,
                       weak_ptr_factory_.GetWeakPtr(),
                       initialization_successful));
    pref_filter_->FilterOnLoad(std::move(post_filter_on_load_callback),
                               std::move(unfiltered_prefs));
  } else {
    FinalizeFileRead(initialization_successful, std::move(unfiltered_prefs),
                     false);
  }
}

JsonPrefStore::~JsonPrefStore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CommitPendingWrite();
}

std::optional<std::string> JsonPrefStore::SerializeData() {
  PerformPreserializationTasks();
  return DoSerialize(prefs_, path_);
}

base::ImportantFileWriter::BackgroundDataProducerCallback
JsonPrefStore::GetSerializedDataProducerForBackgroundSequence() {
  PerformPreserializationTasks();
  return base::BindOnce(&DoSerialize, prefs_.Clone(), path_);
}

void JsonPrefStore::FinalizeFileRead(bool initialization_successful,
                                     base::Value::Dict prefs,
                                     bool schedule_write) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  filtering_in_progress_ = false;

  if (!initialization_successful) {
    for (PrefStore::Observer& observer : observers_)
      observer.OnInitializationCompleted(false);
    return;
  }

  prefs_ = std::move(prefs);

  initialized_ = true;

  if (schedule_write)
    ScheduleWrite(DEFAULT_PREF_WRITE_FLAGS);

  if (error_delegate_.has_value() && error_delegate_.value() &&
      read_error_ != PREF_READ_ERROR_NONE) {
    error_delegate_.value()->OnError(read_error_);
  }

  for (PrefStore::Observer& observer : observers_)
    observer.OnInitializationCompleted(true);

  return;
}

void JsonPrefStore::ScheduleWrite(uint32_t flags) {
  if (read_only_)
    return;

  if (flags & LOSSY_PREF_WRITE_FLAG) {
    pending_lossy_write_ = true;
  } else {
    writer_.ScheduleWriteWithBackgroundDataSerializer(this);
  }
}

bool JsonPrefStore::HasReadErrorDelegate() const {
  return error_delegate_.has_value();
}
