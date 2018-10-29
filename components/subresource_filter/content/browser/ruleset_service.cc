// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/ruleset_service.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/subresource_filter/content/common/subresource_filter_messages.h"
#include "components/subresource_filter/core/browser/copying_file_stream.h"
#include "components/subresource_filter/core/browser/ruleset_service_delegate.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "components/subresource_filter/core/common/unindexed_ruleset.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace subresource_filter {

// Constant definitions and helper functions ---------------------------------

namespace {

// Names of the preferences storing the most recent ruleset version that
// was successfully stored to disk.
const char kSubresourceFilterRulesetContentVersion[] =
    "subresource_filter.ruleset_version.content";
const char kSubresourceFilterRulesetFormatVersion[] =
    "subresource_filter.ruleset_version.format";
const char kSubresourceFilterRulesetChecksum[] =
    "subresource_filter.ruleset_version.checksum";

void RecordIndexAndWriteRulesetResult(
    RulesetService::IndexAndWriteRulesetResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "SubresourceFilter.WriteRuleset.Result", static_cast<int>(result),
      static_cast<int>(RulesetService::IndexAndWriteRulesetResult::MAX));
}

// Implements operations on a `sentinel file`, which is used as a safeguard to
// prevent crash-looping if ruleset indexing crashes right after start-up.
//
// The sentinel file is placed in the ruleset version directory just before
// indexing commences, and removed afterwards. Therefore, if a sentinel file is
// found on next start-up, it is an indication that the previous indexing
// operation may have crashed, and indexing will not be attempted again.
//
// After the first failed indexing attempt, the sentinel file will not be
// removed unless |RulesetIndexer::kIndexedFormatVersion| is incremented. It is
// expected that by that time, either the indexing logic or the ruleset contents
// will be fixed. The consumed disk space is negligible as no ruleset data will
// be written to disk when indexing fails.
//
// Admittedly, this approach errs on the side of caution, and false positives
// can happen. For example, the sentinel file may fail to be removed in case of
// an unclean shutdown, or an unrelated crash as well. This should not be a
// problem, however, as the sentinel file only affects one version of the
// ruleset, and it is expected that version updates will be frequent enough.
class SentinelFile {
 public:
  explicit SentinelFile(const base::FilePath& version_directory)
      : path_(IndexedRulesetLocator::GetSentinelFilePath(version_directory)) {}

  bool IsPresent() { return base::PathExists(path_); }
  bool Create() { return base::WriteFile(path_, nullptr, 0) == 0; }
  bool Remove() { return base::DeleteFile(path_, false /* recursive */); }

 private:
  base::FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(SentinelFile);
};

void SendRulesetToRenderProcess(base::File* file,
                                content::RenderProcessHost* rph) {
  DCHECK(rph);
  DCHECK(file);
  DCHECK(file->IsValid());
  rph->Send(new SubresourceFilterMsg_SetRulesetForProcess(
      IPC::TakePlatformFileForTransit(file->Duplicate())));
}

// The file handle is closed when the argument goes out of scope.
void CloseFile(base::File) {}

// Posts the |file| handle to the file thread so it can be closed.
void CloseFileOnFileThread(base::File* file) {
  if (!file->IsValid())
    return;
  base::PostTaskWithTraits(FROM_HERE,
                           {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
                           base::BindOnce(&CloseFile, std::move(*file)));
}

}  // namespace

// UnindexedRulesetInfo -------------------------------------------------------

UnindexedRulesetInfo::UnindexedRulesetInfo() = default;
UnindexedRulesetInfo::~UnindexedRulesetInfo() = default;

// IndexedRulesetVersion ------------------------------------------------------

IndexedRulesetVersion::IndexedRulesetVersion() = default;
IndexedRulesetVersion::IndexedRulesetVersion(const std::string& content_version,
                                             int format_version)
    : content_version(content_version), format_version(format_version) {}
IndexedRulesetVersion::~IndexedRulesetVersion() = default;
IndexedRulesetVersion& IndexedRulesetVersion::operator=(
    const IndexedRulesetVersion&) = default;

// static
void IndexedRulesetVersion::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kSubresourceFilterRulesetContentVersion,
                               std::string());
  registry->RegisterIntegerPref(kSubresourceFilterRulesetFormatVersion, 0);
  registry->RegisterIntegerPref(kSubresourceFilterRulesetChecksum, 0);
}

// static
int IndexedRulesetVersion::CurrentFormatVersion() {
  return RulesetIndexer::kIndexedFormatVersion;
}

void IndexedRulesetVersion::ReadFromPrefs(PrefService* local_state) {
  format_version =
      local_state->GetInteger(kSubresourceFilterRulesetFormatVersion);
  content_version =
      local_state->GetString(kSubresourceFilterRulesetContentVersion);
  checksum = local_state->GetInteger(kSubresourceFilterRulesetChecksum);
}

bool IndexedRulesetVersion::IsValid() const {
  return format_version != 0 && !content_version.empty();
}

bool IndexedRulesetVersion::IsCurrentFormatVersion() const {
  return format_version == CurrentFormatVersion();
}

void IndexedRulesetVersion::SaveToPrefs(PrefService* local_state) const {
  local_state->SetInteger(kSubresourceFilterRulesetFormatVersion,
                          format_version);
  local_state->SetString(kSubresourceFilterRulesetContentVersion,
                         content_version);
  local_state->SetInteger(kSubresourceFilterRulesetChecksum, checksum);
}

std::unique_ptr<base::trace_event::TracedValue>
IndexedRulesetVersion::ToTracedValue() const {
  auto value = std::make_unique<base::trace_event::TracedValue>();
  value->SetString("content_version", content_version);
  value->SetInteger("format_version", format_version);
  return value;
}

// IndexedRulesetLocator ------------------------------------------------------

// static
base::FilePath IndexedRulesetLocator::GetSubdirectoryPathForVersion(
    const base::FilePath& base_dir,
    const IndexedRulesetVersion& version) {
  return base_dir.AppendASCII(base::IntToString(version.format_version))
      .AppendASCII(version.content_version);
}

// static
base::FilePath IndexedRulesetLocator::GetRulesetDataFilePath(
    const base::FilePath& version_directory) {
  return version_directory.Append(kRulesetDataFileName);
}

// static
base::FilePath IndexedRulesetLocator::GetLicenseFilePath(
    const base::FilePath& version_directory) {
  return version_directory.Append(kLicenseFileName);
}

// static
base::FilePath IndexedRulesetLocator::GetSentinelFilePath(
    const base::FilePath& version_directory) {
  return version_directory.Append(kSentinelFileName);
}

// static
void IndexedRulesetLocator::DeleteObsoleteRulesets(
    const base::FilePath& indexed_ruleset_base_dir,
    const IndexedRulesetVersion& most_recent_version) {
  base::FilePath current_format_dir(indexed_ruleset_base_dir.AppendASCII(
      base::IntToString(IndexedRulesetVersion::CurrentFormatVersion())));

  // First delete all directories containing rulesets of obsolete formats.
  base::FileEnumerator format_dirs(indexed_ruleset_base_dir,
                                   false /* recursive */,
                                   base::FileEnumerator::DIRECTORIES);
  for (base::FilePath format_dir = format_dirs.Next(); !format_dir.empty();
       format_dir = format_dirs.Next()) {
    if (format_dir != current_format_dir)
      base::DeleteFile(format_dir, true /* recursive */);
  }

  base::FilePath most_recent_version_dir =
      most_recent_version.IsValid()
          ? IndexedRulesetLocator::GetSubdirectoryPathForVersion(
                indexed_ruleset_base_dir, most_recent_version)
          : base::FilePath();

  // Then delete all indexed rulesets of the current format with obsolete
  // content versions, except those with a sentinel file present.
  base::FileEnumerator version_dirs(current_format_dir, false /* recursive */,
                                    base::FileEnumerator::DIRECTORIES);
  for (base::FilePath version_dir = version_dirs.Next(); !version_dir.empty();
       version_dir = version_dirs.Next()) {
    if (SentinelFile(version_dir).IsPresent())
      continue;
    if (version_dir == most_recent_version_dir)
      continue;
    base::DeleteFile(version_dir, true /* recursive */);
  }
}

// RulesetService -------------------------------------------------------------

// static
decltype(&RulesetService::IndexRuleset) RulesetService::g_index_ruleset_func =
    &RulesetService::IndexRuleset;

// static
decltype(&base::ReplaceFile) RulesetService::g_replace_file_func =
    &base::ReplaceFile;

RulesetService::RulesetService(
    PrefService* local_state,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    RulesetServiceDelegate* delegate,
    const base::FilePath& indexed_ruleset_base_dir)
    : local_state_(local_state),
      background_task_runner_(std::move(background_task_runner)),
      delegate_(delegate),
      is_initialized_(false),
      indexed_ruleset_base_dir_(indexed_ruleset_base_dir) {
  DCHECK(delegate_);
  DCHECK_NE(local_state_->GetInitializationStatus(),
            PrefService::INITIALIZATION_STATUS_WAITING);
}

void RulesetService::StartInitialization() {
  IndexedRulesetVersion most_recently_indexed_version;
  most_recently_indexed_version.ReadFromPrefs(local_state_);
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("loading"),
               "RulesetService::RulesetService", "prefs_version",
               most_recently_indexed_version.ToTracedValue());
  if (most_recently_indexed_version.IsValid() &&
      most_recently_indexed_version.IsCurrentFormatVersion()) {
    OpenAndPublishRuleset(most_recently_indexed_version);
  } else {
    IndexedRulesetVersion().SaveToPrefs(local_state_);
  }

  DCHECK(delegate_->BestEffortTaskRunner()->BelongsToCurrentThread());
  delegate_->BestEffortTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RulesetService::FinishInitialization, AsWeakPtr()));
}

RulesetService::~RulesetService() {}

void RulesetService::IndexAndStoreAndPublishRulesetIfNeeded(
    const UnindexedRulesetInfo& unindexed_ruleset_info) {
  if (unindexed_ruleset_info.content_version.empty())
    return;

  // Trying to store a ruleset with the same version for a second time would
  // not only be futile, but would fail on Windows due to "File System
  // Tunneling" as long as the previously stored copy of the rules is still
  // in use.
  IndexedRulesetVersion most_recently_indexed_version;
  most_recently_indexed_version.ReadFromPrefs(local_state_);
  if (most_recently_indexed_version.IsCurrentFormatVersion() &&
      most_recently_indexed_version.content_version ==
          unindexed_ruleset_info.content_version) {
    return;
  }

  // Before initialization, retain information about the most recently supplied
  // unindexed ruleset, to be processed during initialization.
  if (!is_initialized_) {
    queued_unindexed_ruleset_info_ = unindexed_ruleset_info;
    return;
  }

  IndexAndStoreRuleset(
      unindexed_ruleset_info,
      base::BindOnce(&RulesetService::OpenAndPublishRuleset, AsWeakPtr()));
}

IndexedRulesetVersion RulesetService::GetMostRecentlyIndexedVersion() const {
  IndexedRulesetVersion version;
  version.ReadFromPrefs(local_state_);
  return version;
}

// static
IndexedRulesetVersion RulesetService::IndexAndWriteRuleset(
    const base::FilePath& indexed_ruleset_base_dir,
    const UnindexedRulesetInfo& unindexed_ruleset_info) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);

  base::File unindexed_ruleset_file(
      unindexed_ruleset_info.ruleset_path,
      base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!unindexed_ruleset_file.IsValid()) {
    RecordIndexAndWriteRulesetResult(
        IndexAndWriteRulesetResult::FAILED_OPENING_UNINDEXED_RULESET);
    return IndexedRulesetVersion();
  }

  IndexedRulesetVersion indexed_version(
      unindexed_ruleset_info.content_version,
      IndexedRulesetVersion::CurrentFormatVersion());
  base::FilePath indexed_ruleset_version_dir =
      IndexedRulesetLocator::GetSubdirectoryPathForVersion(
          indexed_ruleset_base_dir, indexed_version);

  if (!base::CreateDirectory(indexed_ruleset_version_dir)) {
    RecordIndexAndWriteRulesetResult(
        IndexAndWriteRulesetResult::FAILED_CREATING_VERSION_DIR);
    return IndexedRulesetVersion();
  }

  SentinelFile sentinel_file(indexed_ruleset_version_dir);
  if (sentinel_file.IsPresent()) {
    RecordIndexAndWriteRulesetResult(
        IndexAndWriteRulesetResult::ABORTED_BECAUSE_SENTINEL_FILE_PRESENT);
    return IndexedRulesetVersion();
  }

  if (!sentinel_file.Create()) {
    RecordIndexAndWriteRulesetResult(
        IndexAndWriteRulesetResult::FAILED_CREATING_SENTINEL_FILE);
    return IndexedRulesetVersion();
  }

  // --- Begin of guarded section.
  //
  // Crashes or errors occurring here will leave behind a sentinel file that
  // will prevent this version of the ruleset from ever being indexed again.

  RulesetIndexer indexer;
  if (!(*g_index_ruleset_func)(std::move(unindexed_ruleset_file), &indexer)) {
    RecordIndexAndWriteRulesetResult(
        IndexAndWriteRulesetResult::FAILED_PARSING_UNINDEXED_RULESET);
    return IndexedRulesetVersion();
  }

  // --- End of guarded section.
  indexed_version.checksum = indexer.GetChecksum();
  if (!sentinel_file.Remove()) {
    RecordIndexAndWriteRulesetResult(
        IndexAndWriteRulesetResult::FAILED_DELETING_SENTINEL_FILE);
    return IndexedRulesetVersion();
  }

  IndexAndWriteRulesetResult result = WriteRuleset(
      indexed_ruleset_version_dir, unindexed_ruleset_info.license_path,
      indexer.data(), indexer.size());
  RecordIndexAndWriteRulesetResult(result);
  if (result != IndexAndWriteRulesetResult::SUCCESS)
    return IndexedRulesetVersion();

  DCHECK(indexed_version.IsValid());
  return indexed_version;
}

// static
bool RulesetService::IndexRuleset(base::File unindexed_ruleset_file,
                                  RulesetIndexer* indexer) {
  SCOPED_UMA_HISTOGRAM_TIMER("SubresourceFilter.IndexRuleset.WallDuration");
  SCOPED_UMA_HISTOGRAM_THREAD_TIMER(
      "SubresourceFilter.IndexRuleset.CPUDuration");

  int64_t unindexed_ruleset_size = unindexed_ruleset_file.GetLength();
  if (unindexed_ruleset_size < 0)
    return false;
  CopyingFileInputStream copying_stream(std::move(unindexed_ruleset_file));
  google::protobuf::io::CopyingInputStreamAdaptor zero_copy_stream_adaptor(
      &copying_stream, 4096 /* buffer_size */);
  UnindexedRulesetReader reader(&zero_copy_stream_adaptor);

  size_t num_unsupported_rules = 0;
  url_pattern_index::proto::FilteringRules ruleset_chunk;
  while (reader.ReadNextChunk(&ruleset_chunk)) {
    for (const auto& rule : ruleset_chunk.url_rules()) {
      if (!indexer->AddUrlRule(rule))
        ++num_unsupported_rules;
    }
  }
  indexer->Finish();

  UMA_HISTOGRAM_COUNTS_10000(
      "SubresourceFilter.IndexRuleset.NumUnsupportedRules",
      num_unsupported_rules);

  return reader.num_bytes_read() == unindexed_ruleset_size;
}

// static
RulesetService::IndexAndWriteRulesetResult RulesetService::WriteRuleset(
    const base::FilePath& indexed_ruleset_version_dir,
    const base::FilePath& license_source_path,
    const uint8_t* indexed_ruleset_data,
    size_t indexed_ruleset_size) {
  base::ScopedTempDir scratch_dir;
  if (!scratch_dir.CreateUniqueTempDirUnderPath(
          indexed_ruleset_version_dir.DirName())) {
    return IndexAndWriteRulesetResult::FAILED_CREATING_SCRATCH_DIR;
  }

  static_assert(sizeof(uint8_t) == sizeof(char), "Expected char = byte.");
  const int data_size_in_chars = base::checked_cast<int>(indexed_ruleset_size);
  if (base::WriteFile(
          IndexedRulesetLocator::GetRulesetDataFilePath(scratch_dir.GetPath()),
          reinterpret_cast<const char*>(indexed_ruleset_data),
          data_size_in_chars) != data_size_in_chars) {
    return IndexAndWriteRulesetResult::FAILED_WRITING_RULESET_DATA;
  }

  if (base::PathExists(license_source_path) &&
      !base::CopyFile(
          license_source_path,
          IndexedRulesetLocator::GetLicenseFilePath(scratch_dir.GetPath()))) {
    return IndexAndWriteRulesetResult::FAILED_WRITING_LICENSE;
  }

  // Creating a temporary directory also makes sure the path (except for the
  // final segment) gets created. ReplaceFile would not create the path.
  DCHECK(base::PathExists(indexed_ruleset_version_dir.DirName()));

  // Need to manually delete the previously stored ruleset with the same
  // version, if any, as ReplaceFile would not overwrite a non-empty directory.
  // Due to the same-version check in IndexAndStoreAndPublishRulesetIfNeeded, we
  // would not normally find a pre-existing copy at this point unless the
  // previous write was interrupted.
  if (!base::DeleteFile(indexed_ruleset_version_dir, true))
    return IndexAndWriteRulesetResult::FAILED_DELETE_PREEXISTING;

  base::FilePath scratch_dir_with_new_indexed_ruleset = scratch_dir.Take();
  base::File::Error error;
  if (!(*g_replace_file_func)(scratch_dir_with_new_indexed_ruleset,
                              indexed_ruleset_version_dir, &error)) {
    base::DeleteFile(scratch_dir_with_new_indexed_ruleset, true);
    // While enumerators of base::File::Error all have negative values, the
    // histogram records the absolute values.
    UMA_HISTOGRAM_ENUMERATION("SubresourceFilter.WriteRuleset.ReplaceFileError",
                              -error, -base::File::FILE_ERROR_MAX);
    return IndexAndWriteRulesetResult::FAILED_REPLACE_FILE;
  }

  return IndexAndWriteRulesetResult::SUCCESS;
}

void RulesetService::FinishInitialization() {
  is_initialized_ = true;

  IndexedRulesetVersion most_recently_indexed_version;
  most_recently_indexed_version.ReadFromPrefs(local_state_);
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&IndexedRulesetLocator::DeleteObsoleteRulesets,
                     indexed_ruleset_base_dir_, most_recently_indexed_version));

  if (!queued_unindexed_ruleset_info_.content_version.empty()) {
    IndexAndStoreRuleset(
        queued_unindexed_ruleset_info_,
        base::BindOnce(&RulesetService::OpenAndPublishRuleset, AsWeakPtr()));
    queued_unindexed_ruleset_info_ = UnindexedRulesetInfo();
  }
}

void RulesetService::IndexAndStoreRuleset(
    const UnindexedRulesetInfo& unindexed_ruleset_info,
    WriteRulesetCallback success_callback) {
  DCHECK(!unindexed_ruleset_info.content_version.empty());
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&RulesetService::IndexAndWriteRuleset,
                     indexed_ruleset_base_dir_, unindexed_ruleset_info),
      base::BindOnce(&RulesetService::OnWrittenRuleset, AsWeakPtr(),
                     std::move(success_callback)));
}

void RulesetService::OnWrittenRuleset(WriteRulesetCallback result_callback,
                                      const IndexedRulesetVersion& version) {
  DCHECK(!result_callback.is_null());
  if (!version.IsValid())
    return;
  version.SaveToPrefs(local_state_);
  std::move(result_callback).Run(version);
}

void RulesetService::OpenAndPublishRuleset(
    const IndexedRulesetVersion& version) {
  const base::FilePath file_path =
      IndexedRulesetLocator::GetRulesetDataFilePath(
          IndexedRulesetLocator::GetSubdirectoryPathForVersion(
              indexed_ruleset_base_dir_, version));

  delegate_->TryOpenAndSetRulesetFile(
      file_path, version.checksum,
      base::BindOnce(&RulesetService::OnRulesetSet, AsWeakPtr()));
}

void RulesetService::OnRulesetSet(base::File file) {
  // The file has just been successfully written, so a failure here is unlikely
  // unless |indexed_ruleset_base_dir_| has been tampered with or there are disk
  // errors. Still, restore the invariant that a valid version in preferences
  // always points to an existing version of disk by invalidating the prefs.
  if (!file.IsValid()) {
    IndexedRulesetVersion().SaveToPrefs(local_state_);
    return;
  }

  delegate_->PublishNewRulesetVersion(std::move(file));
}

// ContentRulesetService ------------------------------------------------------

ContentRulesetService::ContentRulesetService(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : ruleset_dealer_(std::make_unique<VerifiedRulesetDealer::Handle>(
          std::move(blocking_task_runner))) {
  best_effort_task_runner_ = base::CreateSingleThreadTaskRunnerWithTraits(
      {content::BrowserThread::UI, base::TaskPriority::BEST_EFFORT});
  DCHECK(best_effort_task_runner_->BelongsToCurrentThread());
  // Must rely on notifications as RenderProcessHostObserver::RenderProcessReady
  // would only be called after queued IPC messages (potentially triggering a
  // navigation) had already been sent to the new renderer.
  notification_registrar_.Add(
      this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
      content::NotificationService::AllBrowserContextsAndSources());
}

ContentRulesetService::~ContentRulesetService() {
  CloseFileOnFileThread(&ruleset_data_);
}

void ContentRulesetService::SetRulesetPublishedCallbackForTesting(
    base::OnceClosure callback) {
  ruleset_published_callback_ = std::move(callback);
}

void ContentRulesetService::TryOpenAndSetRulesetFile(
    const base::FilePath& file_path,
    int expected_checksum,
    base::OnceCallback<void(base::File)> callback) {
  ruleset_dealer_->TryOpenAndSetRulesetFile(file_path, expected_checksum,
                                            std::move(callback));
}

void ContentRulesetService::PublishNewRulesetVersion(base::File ruleset_data) {
  DCHECK(ruleset_data.IsValid());
  CloseFileOnFileThread(&ruleset_data_);

  // If Ad Tagging is running, then every request does a lookup and it's
  // important that we verify the ruleset early on.
  if (base::FeatureList::IsEnabled(kAdTagging)) {
    // Even though the handle will immediately be destroyed, it will still
    // validate the ruleset on its task runner.
    VerifiedRuleset::Handle ruleset_handle(ruleset_dealer_.get());
  }

  ruleset_data_ = std::move(ruleset_data);
  for (auto it = content::RenderProcessHost::AllHostsIterator(); !it.IsAtEnd();
       it.Advance()) {
    SendRulesetToRenderProcess(&ruleset_data_, it.GetCurrentValue());
  }

  if (!ruleset_published_callback_.is_null())
    std::move(ruleset_published_callback_).Run();
}

scoped_refptr<base::SingleThreadTaskRunner>
ContentRulesetService::BestEffortTaskRunner() {
  return best_effort_task_runner_;
}

void ContentRulesetService::SetAndInitializeRulesetService(
    std::unique_ptr<RulesetService> ruleset_service) {
  ruleset_service_ = std::move(ruleset_service);
  ruleset_service_->StartInitialization();
}

void ContentRulesetService::IndexAndStoreAndPublishRulesetIfNeeded(
    const UnindexedRulesetInfo& unindexed_ruleset_info) {
  DCHECK(ruleset_service_);
  ruleset_service_->IndexAndStoreAndPublishRulesetIfNeeded(
      unindexed_ruleset_info);
}

void ContentRulesetService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_RENDERER_PROCESS_CREATED);
  if (!ruleset_data_.IsValid())
    return;
  SendRulesetToRenderProcess(
      &ruleset_data_,
      content::Source<content::RenderProcessHost>(source).ptr());
}

}  // namespace subresource_filter
