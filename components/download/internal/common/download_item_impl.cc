// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// File method ordering: Methods in this file are in the same order as
// in download_item_impl.h, with the following exception: The public
// interface Start is placed in chronological order with the other
// (private) routines that together define a DownloadItem's state
// transitions as the download progresses.  See "Download progression
// cascade" later in this file.

// A regular DownloadItem (created for a download in this session of
// the browser) normally goes through the following states:
//      * Created (when download starts)
//      * Destination filename determined
//      * Entered into the history database.
//      * Made visible in the download shelf.
//      * All the data is saved.  Note that the actual data download occurs
//        in parallel with the above steps, but until those steps are
//        complete, the state of the data save will be ignored.
//      * Download file is renamed to its final name, and possibly
//        auto-opened.

#include "components/download/public/common/download_item_impl.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/download/internal/common/download_job_impl.h"
#include "components/download/internal/common/parallel_download_utils.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_file.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item_impl_delegate.h"
#include "components/download/public/common/download_item_rename_handler.h"
#include "components/download/public/common/download_job_factory.h"
#include "components/download/public/common/download_stats.h"
#include "components/download/public/common/download_task_runner.h"
#include "components/download/public/common/download_ukm_helper.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/download_utils.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/referrer_policy.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/download/internal/common/android/download_collection_bridge.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

namespace download {

namespace {

void DeleteDownloadedFileDone(base::WeakPtr<DownloadItemImpl> item,
                              base::OnceCallback<void(bool)> callback,
                              bool success) {
  if (success && item.get())
    item->OnDownloadedFileRemoved();
  std::move(callback).Run(success);
}

// Wrapper around DownloadFile::Detach and DownloadFile::Cancel that
// takes ownership of the DownloadFile and hence implicitly destroys it
// at the end of the function.
base::FilePath DownloadFileDetach(std::unique_ptr<DownloadFile> download_file) {
  DCHECK(GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  base::FilePath full_path = download_file->FullPath();
  download_file->Detach();
  return full_path;
}

base::FilePath MakeCopyOfDownloadFile(DownloadFile* download_file) {
  DCHECK(GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  base::FilePath temp_file_path;
  if (!base::CreateTemporaryFile(&temp_file_path))
    return base::FilePath();

  if (!base::CopyFile(download_file->FullPath(), temp_file_path)) {
    DeleteDownloadedFile(temp_file_path);
    return base::FilePath();
  }

  return temp_file_path;
}

void DownloadFileCancel(std::unique_ptr<DownloadFile> download_file) {
  DCHECK(GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  download_file->Cancel();
}

// Most of the cancellation pathways behave the same whether the cancellation
// was initiated by ther user (CANCELED) or initiated due to browser context
// shutdown (SHUTDOWN).
bool IsCancellation(DownloadInterruptReason reason) {
  return reason == DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN ||
         reason == DOWNLOAD_INTERRUPT_REASON_USER_CANCELED;
}

std::string GetDownloadCreationTypeNames(
    DownloadItem::DownloadCreationType type) {
  switch (type) {
    case DownloadItem::TYPE_ACTIVE_DOWNLOAD:
      return "NEW_DOWNLOAD";
    case DownloadItem::TYPE_HISTORY_IMPORT:
      return "HISTORY_IMPORT";
    case DownloadItem::TYPE_SAVE_PAGE_AS:
      return "SAVE_PAGE_AS";
    default:
      NOTREACHED_IN_MIGRATION();
      return "INVALID_TYPE";
  }
}

std::string GetDownloadDangerNames(DownloadDangerType type) {
  switch (type) {
    case DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
      return "NOT_DANGEROUS";
    case DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return "DANGEROUS_FILE";
    case DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      return "DANGEROUS_URL";
    case DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      return "DANGEROUS_CONTENT";
    case DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
      return "MAYBE_DANGEROUS_CONTENT";
    case DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return "UNCOMMON_CONTENT";
    case DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
      return "USER_VALIDATED";
    case DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      return "DANGEROUS_HOST";
    case DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return "POTENTIALLY_UNWANTED";
    case DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
      return "ALLOWLISTED_BY_POLICY";
    case DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
      return "ASYNC_SCANNING";
    case DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
      return "BLOCKED_PASSWORD_PROTECTED";
    case DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
      return "BLOCKED_TOO_LARGE";
    case DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
      return "SENSITIVE_CONTENT_WARNING";
    case DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
      return "SENSITIVE_CONTENT_BLOCK";
    case DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
      return "DEEP_SCANNED_SAFE";
    case DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
      return "DEEP_SCANNED_OPENED_DANGEROUS";
    case DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
      return "PROMPT_FOR_SCANNING";
    case DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
      return "DANGEROUS_ACCOUNT_COMPROMISE";
    case DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
      return "DEEP_SCANNED_FAILED";
    case DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
      return "PROMPT_FOR_LOCAL_PASSWORD_SCANNING";
    case DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
      return "ASYNC_LOCAL_PASSWORD_SCANNING";
    case DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED:
      return "BLOCKED_SCAN_FAILED";
    case DOWNLOAD_DANGER_TYPE_MAX:
      NOTREACHED_IN_MIGRATION();
      return "UNKNOWN_DANGER_TYPE";
  }
}

class DownloadItemActivatedData
    : public base::trace_event::ConvertableToTraceFormat {
 public:
  DownloadItemActivatedData(DownloadItem::DownloadCreationType download_type,
                            uint32_t download_id,
                            const GURL& original_url,
                            const GURL& final_url,
                            std::string file_name,
                            DownloadDangerType danger_type,
                            int64_t start_offset,
                            bool has_user_gesture)
      : download_type_(download_type),
        download_id_(download_id),
        original_url_(original_url),
        final_url_(final_url),
        file_name_(file_name),
        danger_type_(danger_type),
        start_offset_(start_offset),
        has_user_gesture_(has_user_gesture) {}

  DownloadItemActivatedData(const DownloadItemActivatedData&) = delete;
  DownloadItemActivatedData& operator=(const DownloadItemActivatedData&) =
      delete;

  ~DownloadItemActivatedData() override = default;

  void AppendAsTraceFormat(std::string* out) const override {
    out->append("{");
    out->append(base::StringPrintf(
        "\"type\":\"%s\",",
        GetDownloadCreationTypeNames(download_type_).c_str()));
    out->append(base::StringPrintf("\"id\":\"%d\",", download_id_));
    out->append("\"original_url\":");
    base::EscapeJSONString(original_url_.is_valid() ? original_url_.spec() : "",
                           true, out);
    out->append(",");
    out->append("\"final_url\":");
    base::EscapeJSONString(final_url_.is_valid() ? final_url_.spec() : "", true,
                           out);
    out->append(",");
    out->append("\"file_name\":");
    base::EscapeJSONString(file_name_, true, out);
    out->append(",");
    out->append(
        base::StringPrintf("\"danger_type\":\"%s\",",
                           GetDownloadDangerNames(danger_type_).c_str()));
    out->append(
        base::StringPrintf("\"start_offset\":\"%" PRId64 "\",", start_offset_));
    out->append(base::StringPrintf("\"has_user_gesture\":\"%s\"",
                                   has_user_gesture_ ? "true" : "false"));
    out->append("}");
  }

 private:
  DownloadItem::DownloadCreationType download_type_;
  uint32_t download_id_;
  GURL original_url_;
  GURL final_url_;
  std::string file_name_;
  DownloadDangerType danger_type_;
  int64_t start_offset_;
  bool has_user_gesture_;
};

}  // namespace

// The maximum number of attempts we will make to resume automatically.
const int DownloadItemImpl::kMaxAutoResumeAttempts = 5;

DownloadItemImpl::RequestInfo::RequestInfo(
    const std::vector<GURL>& url_chain,
    const GURL& referrer_url,
    const std::string& serialized_embedder_download_data,
    const GURL& tab_url,
    const GURL& tab_referrer_url,
    const std::optional<url::Origin>& request_initiator,
    const std::string& suggested_filename,
    const base::FilePath& forced_file_path,
    ui::PageTransition transition_type,
    bool has_user_gesture,
    const std::string& remote_address,
    base::Time start_time,
    ::network::mojom::CredentialsMode credentials_mode,
    const std::optional<net::IsolationInfo>& isolation_info,
    int64_t range_request_from,
    int64_t range_request_to)
    : url_chain(url_chain),
      referrer_url(referrer_url),
      serialized_embedder_download_data(serialized_embedder_download_data),
      tab_url(tab_url),
      tab_referrer_url(tab_referrer_url),
      request_initiator(request_initiator),
      suggested_filename(suggested_filename),
      forced_file_path(forced_file_path),
      transition_type(transition_type),
      has_user_gesture(has_user_gesture),
      remote_address(remote_address),
      start_time(start_time),
      credentials_mode(credentials_mode),
      isolation_info(isolation_info),
      range_request_from(range_request_from),
      range_request_to(range_request_to) {}

DownloadItemImpl::RequestInfo::RequestInfo(const GURL& url)
    : url_chain(std::vector<GURL>(1, url)), start_time(base::Time::Now()) {}

DownloadItemImpl::RequestInfo::RequestInfo() = default;

DownloadItemImpl::RequestInfo::RequestInfo(
    const DownloadItemImpl::RequestInfo& other) = default;

DownloadItemImpl::RequestInfo::~RequestInfo() = default;

DownloadItemImpl::DestinationInfo::DestinationInfo(
    const base::FilePath& target_path,
    const base::FilePath& current_path,
    int64_t received_bytes,
    bool all_data_saved,
    const std::string& hash,
    base::Time end_time)
    : target_path(target_path),
      current_path(current_path),
      received_bytes(received_bytes),
      all_data_saved(all_data_saved),
      hash(hash),
      end_time(end_time) {}

DownloadItemImpl::DestinationInfo::DestinationInfo(
    TargetDisposition target_disposition)
    : target_disposition(target_disposition) {}

DownloadItemImpl::DestinationInfo::DestinationInfo() = default;

DownloadItemImpl::DestinationInfo::DestinationInfo(
    const DownloadItemImpl::DestinationInfo& other) = default;

DownloadItemImpl::DestinationInfo::~DestinationInfo() = default;

// Constructor for reading from the history service.
DownloadItemImpl::DownloadItemImpl(
    DownloadItemImplDelegate* delegate,
    const std::string& guid,
    uint32_t download_id,
    const base::FilePath& current_path,
    const base::FilePath& target_path,
    const std::vector<GURL>& url_chain,
    const GURL& referrer_url,
    const std::string& serialized_embedder_download_data,
    const GURL& tab_url,
    const GURL& tab_refererr_url,
    const std::optional<url::Origin>& request_initiator,
    const std::string& mime_type,
    const std::string& original_mime_type,
    base::Time start_time,
    base::Time end_time,
    const std::string& etag,
    const std::string& last_modified,
    int64_t received_bytes,
    int64_t total_bytes,
    int32_t auto_resume_count,
    const std::string& hash,
    DownloadItem::DownloadState state,
    DownloadDangerType danger_type,
    DownloadInterruptReason interrupt_reason,
    bool paused,
    bool allow_metered,
    bool opened,
    base::Time last_access_time,
    bool transient,
    const std::vector<DownloadItem::ReceivedSlice>& received_slices,
    int64_t range_request_from,
    int64_t range_request_to,
    std::unique_ptr<DownloadEntry> download_entry)
    : request_info_(url_chain,
                    referrer_url,
                    serialized_embedder_download_data,
                    tab_url,
                    tab_refererr_url,
                    request_initiator,
                    std::string(),
                    base::FilePath(),
                    ui::PAGE_TRANSITION_LINK,
                    false,
                    std::string(),
                    start_time,
                    ::network::mojom::CredentialsMode::kInclude,
                    std::nullopt,
                    range_request_from,
                    range_request_to),
      guid_(guid),
      download_id_(download_id),
      mime_type_(mime_type),
      original_mime_type_(original_mime_type),
      total_bytes_(total_bytes),
      last_reason_(interrupt_reason),
      start_tick_(base::TimeTicks()),
      state_(ExternalToInternalState(state)),
      danger_type_(danger_type),
      delegate_(delegate),
      paused_(paused),
      allow_metered_(allow_metered),
      opened_(opened),
      last_access_time_(last_access_time),
      transient_(transient),
      destination_info_(target_path,
                        current_path,
                        received_bytes,
                        state == COMPLETE,
                        hash,
                        end_time),
      auto_resume_count_(auto_resume_count),
      last_modified_time_(last_modified),
      etag_(etag),
      received_slices_(received_slices),
      is_updating_observers_(false) {
  delegate_->Attach();
  DCHECK(state_ == COMPLETE_INTERNAL || state_ == INTERRUPTED_INTERNAL ||
         state_ == CANCELLED_INTERNAL);
  DCHECK(base::Uuid::ParseCaseInsensitive(guid_).is_valid());

  if (download_entry) {
    download_source_ = download_entry->download_source;
    fetch_error_body_ = download_entry->fetch_error_body;
    request_headers_ = download_entry->request_headers;
    ukm_download_id_ = download_entry->ukm_download_id;
    bytes_wasted_ = download_entry->bytes_wasted;
  } else {
    ukm_download_id_ = ukm::NoURLSourceId();
  }
  Init(false /* not actively downloading */, TYPE_HISTORY_IMPORT);
}

// Constructing for a regular download:
DownloadItemImpl::DownloadItemImpl(DownloadItemImplDelegate* delegate,
                                   uint32_t download_id,
                                   const DownloadCreateInfo& info)
    : request_info_(info.url_chain,
                    info.referrer_url,
                    info.serialized_embedder_download_data,
                    info.tab_url,
                    info.tab_referrer_url,
                    info.request_initiator,
                    base::UTF16ToUTF8(info.save_info->suggested_name),
                    info.save_info->file_path,
                    info.transition_type ? info.transition_type.value()
                                         : ui::PAGE_TRANSITION_LINK,
                    info.has_user_gesture,
                    info.remote_address,
                    info.start_time,
                    info.credentials_mode,
                    info.isolation_info,
                    info.save_info->range_request_from,
                    info.save_info->range_request_to),
      guid_(info.guid.empty()
                ? base::Uuid::GenerateRandomV4().AsLowercaseString()
                : info.guid),
      download_id_(download_id),
      response_headers_(info.response_headers),
      content_disposition_(info.content_disposition),
      mime_type_(info.mime_type),
      original_mime_type_(info.original_mime_type),
      total_bytes_(info.total_bytes),
      last_reason_(info.result),
      start_tick_(base::TimeTicks::Now()),
      state_(INITIAL_INTERNAL),
      delegate_(delegate),
      is_temporary_(!info.transient && !info.save_info->file_path.empty()),
      transient_(info.transient),
      require_safety_checks_(info.require_safety_checks),
      destination_info_(info.save_info->prompt_for_save_location
                            ? TARGET_DISPOSITION_PROMPT
                            : TARGET_DISPOSITION_OVERWRITE),
      last_modified_time_(info.last_modified),
      etag_(info.etag),
      is_updating_observers_(false),
      fetch_error_body_(info.fetch_error_body),
      request_headers_(info.request_headers),
      download_source_(info.download_source)
#if BUILDFLAG(IS_ANDROID)
      ,
      is_must_download_(info.is_must_download)
#endif  // BUILDFLAG(IS_ANDROID)
{
  delegate_->Attach();
  Init(true /* actively downloading */, TYPE_ACTIVE_DOWNLOAD);
  allow_metered_ |= delegate_->IsActiveNetworkMetered();

  TRACE_EVENT_INSTANT0("download", "DownloadStarted", TRACE_EVENT_SCOPE_THREAD);
}

// Constructing for the "Save Page As..." feature:
DownloadItemImpl::DownloadItemImpl(
    DownloadItemImplDelegate* delegate,
    uint32_t download_id,
    const base::FilePath& path,
    const GURL& url,
    const std::string& mime_type,
    DownloadJob::CancelRequestCallback cancel_request_callback)
    : request_info_(url),
      guid_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      download_id_(download_id),
      mime_type_(mime_type),
      original_mime_type_(mime_type),
      start_tick_(base::TimeTicks::Now()),
      state_(IN_PROGRESS_INTERNAL),
      delegate_(delegate),
      destination_info_(path, path, 0, false, std::string(), base::Time()),
      is_updating_observers_(false) {
  job_ = DownloadJobFactory::CreateJob(
      this, std::move(cancel_request_callback), DownloadCreateInfo(), true,
      URLLoaderFactoryProvider::GetNullPtr(),
      /*wake_lock_provider_binder*/ base::NullCallback());
  delegate_->Attach();
  Init(true /* actively downloading */, TYPE_SAVE_PAGE_AS);
}

DownloadItemImpl::~DownloadItemImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Should always have been nuked before now, at worst in
  // DownloadManager shutdown.
  DCHECK(!download_file_);
  CHECK(!is_updating_observers_);

  for (auto& observer : observers_)
    observer.OnDownloadDestroyed(this);
  delegate_->Detach();
}

void DownloadItemImpl::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  observers_.AddObserver(observer);
}

void DownloadItemImpl::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  observers_.RemoveObserver(observer);
}

void DownloadItemImpl::UpdateObservers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(20) << __func__ << "()";

  // Nested updates should not be allowed.
  DCHECK(!is_updating_observers_);

  is_updating_observers_ = true;
  for (auto& observer : observers_)
    observer.OnDownloadUpdated(this);
  is_updating_observers_ = false;
}

void DownloadItemImpl::ValidateDangerousDownload() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!IsDone());
  DCHECK(IsDangerous());

  DVLOG(20) << __func__ << "() download=" << DebugString(true);

  if (IsDone() || !IsDangerous())
    return;

  RecordDangerousDownloadAccept(GetDangerType(), GetTargetFilePath());

  danger_type_ = DOWNLOAD_DANGER_TYPE_USER_VALIDATED;

  TRACE_EVENT_INSTANT1("download", "DownloadItemSaftyStateUpdated",
                       TRACE_EVENT_SCOPE_THREAD, "danger_type",
                       GetDownloadDangerNames(danger_type_).c_str());

  UpdateObservers();  // TODO(asanka): This is potentially unsafe. The download
                      // may not be in a consistent state or around at all after
                      // invoking observers. http://crbug.com/586610

  MaybeCompleteDownload();
}

void DownloadItemImpl::ValidateInsecureDownload() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!IsDone());
  DCHECK(IsInsecure());

  DVLOG(20) << __func__ << "() download=" << DebugString(true);

  insecure_download_status_ = InsecureDownloadStatus::VALIDATED;

  UpdateObservers();  // TODO(asanka): This is potentially unsafe. The download
                      // may not be in a consistent state or around at all after
                      // invoking observers, but we keep it here because it is
                      // used in ValidateDangerousDownload(), too.
                      // http://crbug.com/586610

  MaybeCompleteDownload();
}

void DownloadItemImpl::CopyDownload(AcquireFileCallback callback) {
  DVLOG(20) << __func__ << "() download = " << DebugString(true);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(AllDataSaved());

  if (download_file_) {
    GetDownloadTaskRunner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&MakeCopyOfDownloadFile, download_file_.get()),
        std::move(callback));
  } else {
    std::move(callback).Run(GetFullPath());
  }
}

void DownloadItemImpl::Pause() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Ignore irrelevant states.
  if (IsPaused())
    return;

  switch (state_) {
    case CANCELLED_INTERNAL:
    case COMPLETE_INTERNAL:
    case COMPLETING_INTERNAL:
      return;
    case INITIAL_INTERNAL:
    case INTERRUPTED_INTERNAL:
    case INTERRUPTED_TARGET_PENDING_INTERNAL:
    case RESUMING_INTERNAL:
      // No active request.
      paused_ = true;
      UpdateObservers();
      return;

    case IN_PROGRESS_INTERNAL:
    case TARGET_PENDING_INTERNAL:
      paused_ = true;
      job_->Pause();
      UpdateObservers();
      return;

    case MAX_DOWNLOAD_INTERNAL_STATE:
    case TARGET_RESOLVED_INTERNAL:
      NOTREACHED_IN_MIGRATION();
  }
}

void DownloadItemImpl::Resume(bool user_resume) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(20) << __func__ << "() download = " << DebugString(true);

  switch (state_) {
    case CANCELLED_INTERNAL:  // Nothing to resume.
    case COMPLETE_INTERNAL:
    case COMPLETING_INTERNAL:
    case INITIAL_INTERNAL:
    case INTERRUPTED_TARGET_PENDING_INTERNAL:
    case RESUMING_INTERNAL:  // Resumption in progress.
      return;

    case TARGET_PENDING_INTERNAL:
    case IN_PROGRESS_INTERNAL:
      if (!IsPaused())
        return;
      paused_ = false;
      if (job_)
        job_->Resume(true);

      UpdateResumptionInfo(true);
      UpdateObservers();
      return;

    case INTERRUPTED_INTERNAL:
      UpdateResumptionInfo(paused_ || user_resume);
      paused_ = false;
      if (auto_resume_count_ >= kMaxAutoResumeAttempts) {
        UpdateObservers();
        return;
      }

      ResumeInterruptedDownload(user_resume
                                    ? ResumptionRequestSource::USER
                                    : ResumptionRequestSource::AUTOMATIC);
      UpdateObservers();
      return;

    case MAX_DOWNLOAD_INTERNAL_STATE:
    case TARGET_RESOLVED_INTERNAL:
      NOTREACHED_IN_MIGRATION();
  }
}

void DownloadItemImpl::UpdateResumptionInfo(bool user_resume) {
  if (user_resume) {
    allow_metered_ |= delegate_->IsActiveNetworkMetered();
    bytes_wasted_ = 0;
  }

  ++auto_resume_count_;
  if (user_resume)
    auto_resume_count_ = 0;
}

void DownloadItemImpl::Cancel(bool user_cancel) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(20) << __func__ << "() download = " << DebugString(true);
  InterruptAndDiscardPartialState(
      user_cancel ? DOWNLOAD_INTERRUPT_REASON_USER_CANCELED
                  : DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN);
  UpdateObservers();
}

void DownloadItemImpl::Remove() {
  DVLOG(20) << __func__ << "() download = " << DebugString(true);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  InterruptAndDiscardPartialState(DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);
  UpdateObservers();
  NotifyRemoved();
  delegate_->DownloadRemoved(this);
  // We have now been deleted.
}

void DownloadItemImpl::OpenDownload() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!IsDone()) {
    // We don't honor the open_when_complete_ flag for temporary
    // downloads. Don't set it because it shows up in the UI.
    if (!IsTemporary())
      open_when_complete_ = !open_when_complete_;
    return;
  }

  if (state_ != COMPLETE_INTERNAL || file_externally_removed_)
    return;

  // Ideally, we want to detect errors in opening and report them, but we
  // don't generally have the proper interface for that to the external
  // program that opens the file.  So instead we spawn a check to update
  // the UI if the file has been deleted in parallel with the open.
  delegate_->CheckForFileRemoval(this);
  opened_ = true;
  last_access_time_ = base::Time::Now();
  for (auto& observer : observers_)
    observer.OnDownloadOpened(this);

#if BUILDFLAG(IS_WIN)
  // On Windows, don't actually open the file if it has no extension, to prevent
  // Windows from interpreting it as the command for an executable of the same
  // name.
  if (destination_info_.current_path.Extension().empty()) {
    delegate_->ShowDownloadInShell(this);
    return;
  }
#endif
  delegate_->OpenDownload(this);
}

void DownloadItemImpl::ShowDownloadInShell() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Ideally, we want to detect errors in showing and report them, but we
  // don't generally have the proper interface for that to the external
  // program that opens the file.  So instead we spawn a check to update
  // the UI if the file has been deleted in parallel with the show.
  delegate_->CheckForFileRemoval(this);
  delegate_->ShowDownloadInShell(this);
}

void DownloadItemImpl::RenameDownloadedFileDone(
    RenameDownloadCallback callback,
    const base::FilePath& display_name,
    DownloadRenameResult result) {
  if (result == DownloadRenameResult::SUCCESS) {
    bool is_content_uri = false;
#if BUILDFLAG(IS_ANDROID)
    is_content_uri = GetFullPath().IsContentUri();
#endif  // BUILDFLAG(IS_ANDROID)
    if (is_content_uri) {
      SetDisplayName(display_name);
    } else {
      auto new_full_path =
          base::FilePath(GetFullPath().DirName()).Append(display_name);
      destination_info_.target_path = new_full_path;
      destination_info_.current_path = new_full_path;
    }
    UpdateObservers();
  }
  std::move(callback).Run(result);
}

void DownloadItemImpl::Rename(const base::FilePath& display_name,
                              DownloadItem::RenameDownloadCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (display_name.IsAbsolute()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&DownloadItemImpl::RenameDownloadedFileDone,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(callback), GetFullPath(),
                                  DownloadRenameResult::FAILURE_NAME_INVALID));
    return;
  }

  GetDownloadTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&download::RenameDownloadedFile, GetFullPath(),
                     display_name),
      base::BindOnce(&DownloadItemImpl::RenameDownloadedFileDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     display_name));
}

uint32_t DownloadItemImpl::GetId() const {
  return download_id_;
}

const std::string& DownloadItemImpl::GetGuid() const {
  return guid_;
}

DownloadItem::DownloadState DownloadItemImpl::GetState() const {
  return InternalToExternalState(state_);
}

DownloadInterruptReason DownloadItemImpl::GetLastReason() const {
  return last_reason_;
}

bool DownloadItemImpl::IsPaused() const {
  return paused_;
}

bool DownloadItemImpl::AllowMetered() const {
  return allow_metered_;
}

bool DownloadItemImpl::IsTemporary() const {
  return is_temporary_;
}

bool DownloadItemImpl::CanResume() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  switch (state_) {
    case INITIAL_INTERNAL:
    case COMPLETING_INTERNAL:
    case COMPLETE_INTERNAL:
    case CANCELLED_INTERNAL:
    case RESUMING_INTERNAL:
    case INTERRUPTED_TARGET_PENDING_INTERNAL:
      return false;

    case TARGET_PENDING_INTERNAL:
    case TARGET_RESOLVED_INTERNAL:
    case IN_PROGRESS_INTERNAL:
      return IsPaused();

    case INTERRUPTED_INTERNAL: {
      ResumeMode resume_mode = GetResumeMode();
      // Only allow Resume() calls if the resumption mode requires a user
      // action.
      return resume_mode == ResumeMode::USER_RESTART ||
             resume_mode == ResumeMode::USER_CONTINUE;
    }

    case MAX_DOWNLOAD_INTERNAL_STATE:
      NOTREACHED_IN_MIGRATION();
  }
  return false;
}

bool DownloadItemImpl::IsDone() const {
  return IsDownloadDone(GetURL(), GetState(), GetLastReason());
}

int64_t DownloadItemImpl::GetBytesWasted() const {
  return bytes_wasted_;
}

int32_t DownloadItemImpl::GetAutoResumeCount() const {
  return auto_resume_count_;
}

const GURL& DownloadItemImpl::GetURL() const {
  return request_info_.url_chain.empty() ? GURL::EmptyGURL()
                                         : request_info_.url_chain.back();
}

const std::vector<GURL>& DownloadItemImpl::GetUrlChain() const {
  return request_info_.url_chain;
}

const GURL& DownloadItemImpl::GetOriginalUrl() const {
  // Be careful about taking the front() of possibly-empty vectors!
  // http://crbug.com/190096
  return request_info_.url_chain.empty() ? GURL::EmptyGURL()
                                         : request_info_.url_chain.front();
}

const GURL& DownloadItemImpl::GetReferrerUrl() const {
  return request_info_.referrer_url;
}

const std::string& DownloadItemImpl::GetSerializedEmbedderDownloadData() const {
  return request_info_.serialized_embedder_download_data;
}

const GURL& DownloadItemImpl::GetTabUrl() const {
  if (IsSavePackageDownload()) {
    return GetURL();
  }
  return request_info_.tab_url;
}

const GURL& DownloadItemImpl::GetTabReferrerUrl() const {
  return request_info_.tab_referrer_url;
}

const std::optional<url::Origin>& DownloadItemImpl::GetRequestInitiator()
    const {
  return request_info_.request_initiator;
}

std::string DownloadItemImpl::GetSuggestedFilename() const {
  return request_info_.suggested_filename;
}

const scoped_refptr<const net::HttpResponseHeaders>&
DownloadItemImpl::GetResponseHeaders() const {
  return response_headers_;
}

std::string DownloadItemImpl::GetContentDisposition() const {
  return content_disposition_;
}

std::string DownloadItemImpl::GetMimeType() const {
  return mime_type_;
}

std::string DownloadItemImpl::GetOriginalMimeType() const {
  return original_mime_type_;
}

std::string DownloadItemImpl::GetRemoteAddress() const {
  return request_info_.remote_address;
}

bool DownloadItemImpl::HasUserGesture() const {
  return request_info_.has_user_gesture;
}

ui::PageTransition DownloadItemImpl::GetTransitionType() const {
  return request_info_.transition_type;
}

const std::string& DownloadItemImpl::GetLastModifiedTime() const {
  return last_modified_time_;
}

const std::string& DownloadItemImpl::GetETag() const {
  return etag_;
}

bool DownloadItemImpl::IsSavePackageDownload() const {
  return job_ && job_->IsSavePackageDownload();
}

DownloadSource DownloadItemImpl::GetDownloadSource() const {
  return download_source_;
}

const base::FilePath& DownloadItemImpl::GetFullPath() const {
  return destination_info_.current_path;
}

const base::FilePath& DownloadItemImpl::GetTargetFilePath() const {
  return destination_info_.target_path;
}

const base::FilePath& DownloadItemImpl::GetForcedFilePath() const {
  // TODO(asanka): Get rid of GetForcedFilePath(). We should instead just
  // require that clients respect GetTargetFilePath() if it is already set.
  return request_info_.forced_file_path;
}

base::FilePath DownloadItemImpl::GetTemporaryFilePath() const {
  if (state_ == TARGET_PENDING_INTERNAL ||
      state_ == INTERRUPTED_TARGET_PENDING_INTERNAL)
    return download_file_ ? download_file_->FullPath() : base::FilePath();
  return base::FilePath();
}

base::FilePath DownloadItemImpl::GetFileNameToReportUser() const {
  if (!display_name_.empty())
    return display_name_;
  return GetTargetFilePath().BaseName();
}

DownloadItem::TargetDisposition DownloadItemImpl::GetTargetDisposition() const {
  return destination_info_.target_disposition;
}

const std::string& DownloadItemImpl::GetHash() const {
  return destination_info_.hash;
}

bool DownloadItemImpl::GetFileExternallyRemoved() const {
  return file_externally_removed_;
}

void DownloadItemImpl::DeleteFile(base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (GetState() != DownloadItem::COMPLETE) {
    // Pass a null WeakPtr so it doesn't call OnDownloadedFileRemoved.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&DeleteDownloadedFileDone,
                                  base::WeakPtr<DownloadItemImpl>(),
                                  std::move(callback), false));
    return;
  }
  if (GetFullPath().empty() || file_externally_removed_) {
    // Pass a null WeakPtr so it doesn't call OnDownloadedFileRemoved.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&DeleteDownloadedFileDone,
                                  base::WeakPtr<DownloadItemImpl>(),
                                  std::move(callback), true));
    return;
  }
  GetDownloadTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DeleteDownloadedFile, GetFullPath()),
      base::BindOnce(&DeleteDownloadedFileDone, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

DownloadFile* DownloadItemImpl::GetDownloadFile() {
  return download_file_.get();
}

DownloadItemRenameHandler* DownloadItemImpl::GetRenameHandler() {
  return rename_handler_.get();
}

#if BUILDFLAG(IS_ANDROID)
bool DownloadItemImpl::IsFromExternalApp() {
  return is_from_external_app_;
}

bool DownloadItemImpl::IsMustDownload() {
  return is_must_download_;
}
#endif  // BUILDFLAG(IS_ANDROID)

bool DownloadItemImpl::IsDangerous() const {
  switch (danger_type_) {
    case DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
    case DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
    case DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
    case DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
    case DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
    case DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
    case DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
    case DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
    case DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
    case DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
    case DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
    case DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED:
      return true;
    case DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
    case DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
    case DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
    case DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
    case DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
      return false;
    case DOWNLOAD_DANGER_TYPE_MAX:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

bool DownloadItemImpl::IsInsecure() const {
  return insecure_download_status_ == InsecureDownloadStatus::WARN ||
         insecure_download_status_ == InsecureDownloadStatus::BLOCK ||
         insecure_download_status_ == InsecureDownloadStatus::SILENT_BLOCK;
}

DownloadDangerType DownloadItemImpl::GetDangerType() const {
  return danger_type_;
}

DownloadItem::InsecureDownloadStatus
DownloadItemImpl::GetInsecureDownloadStatus() const {
  return insecure_download_status_;
}

bool DownloadItemImpl::TimeRemaining(base::TimeDelta* remaining) const {
  if (total_bytes_ <= 0)
    return false;  // We never received the content_length for this download.

  int64_t speed = CurrentSpeed();
  if (speed == 0)
    return false;

  *remaining = base::Seconds((total_bytes_ - GetReceivedBytes()) / speed);
  return true;
}

int64_t DownloadItemImpl::CurrentSpeed() const {
  if (IsPaused())
    return 0;
  return bytes_per_sec_;
}

int DownloadItemImpl::PercentComplete() const {
  // If the delegate is delaying completion of the download, then we have no
  // idea how long it will take.
  if (delegate_delayed_complete_ || total_bytes_ <= 0)
    return -1;

  return static_cast<int>(GetReceivedBytes() * 100.0 / total_bytes_);
}

bool DownloadItemImpl::AllDataSaved() const {
  return destination_info_.all_data_saved;
}

int64_t DownloadItemImpl::GetTotalBytes() const {
  return total_bytes_;
}

int64_t DownloadItemImpl::GetReceivedBytes() const {
  return destination_info_.received_bytes;
}

const std::vector<DownloadItem::ReceivedSlice>&
DownloadItemImpl::GetReceivedSlices() const {
  return received_slices_;
}

int64_t DownloadItemImpl::GetUploadedBytes() const {
  return destination_info_.uploaded_bytes;
}

base::Time DownloadItemImpl::GetStartTime() const {
  return request_info_.start_time;
}

base::Time DownloadItemImpl::GetEndTime() const {
  return destination_info_.end_time;
}

bool DownloadItemImpl::CanShowInFolder() {
  // A download can be shown in the folder if the downloaded file is in a known
  // location.
  return CanOpenDownload() && !GetFullPath().empty();
}

bool DownloadItemImpl::CanOpenDownload() {
  // We can open the file or mark it for opening on completion if the download
  // is expected to complete successfully. Exclude temporary downloads, since
  // they aren't owned by the download system.
  const bool is_complete = GetState() == DownloadItem::COMPLETE;
  return (!IsDone() || is_complete) && !IsTemporary() &&
         !file_externally_removed_;
}

bool DownloadItemImpl::ShouldOpenFileBasedOnExtension() {
  return delegate_->ShouldAutomaticallyOpenFile(GetURL(), GetTargetFilePath());
}

bool DownloadItemImpl::ShouldOpenFileByPolicyBasedOnExtension() {
  return delegate_->ShouldAutomaticallyOpenFileByPolicy(GetURL(),
                                                        GetTargetFilePath());
}

bool DownloadItemImpl::GetOpenWhenComplete() const {
  return open_when_complete_;
}

bool DownloadItemImpl::GetAutoOpened() {
  return auto_opened_;
}

bool DownloadItemImpl::GetOpened() const {
  return opened_;
}

base::Time DownloadItemImpl::GetLastAccessTime() const {
  return last_access_time_;
}

bool DownloadItemImpl::IsTransient() const {
  return transient_;
}

bool DownloadItemImpl::RequireSafetyChecks() const {
  return require_safety_checks_;
}

bool DownloadItemImpl::IsParallelDownload() const {
  bool is_parallelizable = job_ ? job_->IsParallelizable() : false;
  return is_parallelizable && download::IsParallelDownloadEnabled();
}

DownloadItem::DownloadCreationType DownloadItemImpl::GetDownloadCreationType()
    const {
  return download_type_;
}

::network::mojom::CredentialsMode DownloadItemImpl::GetCredentialsMode() const {
  return request_info_.credentials_mode;
}

const std::optional<net::IsolationInfo>& DownloadItemImpl::GetIsolationInfo()
    const {
  return request_info_.isolation_info;
}

void DownloadItemImpl::OnContentCheckCompleted(DownloadDangerType danger_type,
                                               DownloadInterruptReason reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(AllDataSaved() || IsSavePackageDownload());

  // Danger type is only allowed to be set on an active download after all data
  // has been saved. This excludes all other states. In particular,
  // OnContentCheckCompleted() isn't allowed on an INTERRUPTED download since
  // such an interruption would need to happen between OnAllDataSaved() and
  // OnContentCheckCompleted() during which no disk or network activity
  // should've taken place.
  DCHECK_EQ(state_, IN_PROGRESS_INTERNAL);
  DVLOG(20) << __func__ << "() danger_type=" << danger_type
            << " download=" << DebugString(true);
  SetDangerType(danger_type);
  if (reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
    InterruptAndDiscardPartialState(reason);
    DCHECK_EQ(ResumeMode::INVALID, GetResumeMode());
  }
  UpdateObservers();
}

void DownloadItemImpl::OnAsyncScanningCompleted(
    DownloadDangerType danger_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(AllDataSaved() || IsSavePackageDownload());

  DVLOG(20) << __func__ << "() danger_type=" << danger_type
            << " download=" << DebugString(true);
  SetDangerType(danger_type);
  UpdateObservers();
}

void DownloadItemImpl::SetOpenWhenComplete(bool open) {
  if (open_when_complete_ != open) {
    open_when_complete_ = open;
    UpdateObservers();
  }
}

void DownloadItemImpl::SetOpened(bool opened) {
  opened_ = opened;
}

void DownloadItemImpl::SetLastAccessTime(base::Time last_access_time) {
  last_access_time_ = last_access_time;
  UpdateObservers();
}

void DownloadItemImpl::SetDisplayName(const base::FilePath& name) {
  display_name_ = name;
}

std::string DownloadItemImpl::DebugString(bool verbose) const {
  std::string description = base::StringPrintf(
      "{ id = %d"
      " state = %s",
      download_id_, DebugDownloadStateString(state_));

  // Construct a string of the URL chain.
  std::string url_list("<none>");
  if (!request_info_.url_chain.empty()) {
    auto iter = request_info_.url_chain.begin();
    auto last = request_info_.url_chain.end();
    url_list = (*iter).is_valid() ? (*iter).spec() : "<invalid>";
    ++iter;
    for (; verbose && (iter != last); ++iter) {
      url_list += " ->\n\t";
      const GURL& next_url = *iter;
      url_list += next_url.is_valid() ? next_url.spec() : "<invalid>";
    }
  }

  if (verbose) {
    description += base::StringPrintf(
        " total = %" PRId64 " received = %" PRId64
        " reason = %s"
        " paused = %c"
        " resume_mode = %s"
        " auto_resume_count = %d"
        " danger = %d"
        " all_data_saved = %c"
        " last_modified = '%s'"
        " etag = '%s'"
        " has_download_file = %s"
        " url_chain = \n\t\"%s\"\n\t"
        " current_path = \"%" PRFilePath
        "\"\n\t"
        " target_path = \"%" PRFilePath
        "\"\n\t"
        " referrer = \"%s\""
        " serialized_embedder_download_data = \"%s\"",
        GetTotalBytes(), GetReceivedBytes(),
        DownloadInterruptReasonToString(last_reason_).c_str(),
        IsPaused() ? 'T' : 'F', DebugResumeModeString(GetResumeMode()),
        auto_resume_count_, GetDangerType(), AllDataSaved() ? 'T' : 'F',
        GetLastModifiedTime().c_str(), GetETag().c_str(),
        download_file_ ? "true" : "false", url_list.c_str(),
        GetFullPath().value().c_str(), GetTargetFilePath().value().c_str(),
        GetReferrerUrl().spec().c_str(),
        GetSerializedEmbedderDownloadData().c_str());
  } else {
    description += base::StringPrintf(" url = \"%s\"", url_list.c_str());
  }

  description += " }";

  return description;
}

void DownloadItemImpl::SimulateErrorForTesting(DownloadInterruptReason reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  InterruptWithPartialState(GetReceivedBytes(), nullptr, reason);
  UpdateObservers();
}

ResumeMode DownloadItemImpl::GetResumeMode() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // We can't continue without a handle on the intermediate file.
  // We also can't continue if we don't have some verifier to make sure
  // we're getting the same file.
  bool restart_required =
      (GetFullPath().empty() ||
       (!HasStrongValidators() &&
        !base::FeatureList::IsEnabled(
            features::kAllowDownloadResumptionWithoutStrongValidators)));
  // We won't auto-restart if we've used up our attempts or the
  // download has been paused by user action.
  bool user_action_required =
      (auto_resume_count_ >= kMaxAutoResumeAttempts || IsPaused());

  return GetDownloadResumeMode(GetURL(), last_reason_, restart_required,
                               user_action_required);
}

bool DownloadItemImpl::HasStrongValidators() const {
  return !etag_.empty() || !last_modified_time_.empty();
}

void DownloadItemImpl::BindWakeLockProvider(
    mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) {
  if (delegate_)
    delegate_->BindWakeLockProvider(std::move(receiver));
}

void DownloadItemImpl::UpdateValidatorsOnResumption(
    const DownloadCreateInfo& new_create_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(RESUMING_INTERNAL, state_);
  DCHECK(!new_create_info.url_chain.empty());

  // We are going to tack on any new redirects to our list of redirects.
  // When a download is resumed, the URL used for the resumption request is the
  // one at the end of the previous redirect chain. Tacking additional redirects
  // to the end of this chain ensures that:
  // - If the download needs to be resumed again, the ETag/Last-Modified headers
  //   will be used with the last server that sent them to us.
  // - The redirect chain contains all the servers that were involved in this
  //   download since the initial request, in order.
  auto chain_iter = new_create_info.url_chain.begin();
  if (*chain_iter == request_info_.url_chain.back())
    ++chain_iter;

  // Record some stats. If the precondition failed (the server returned
  // HTTP_PRECONDITION_FAILED), then the download will automatically retried as
  // a full request rather than a partial. Full restarts clobber validators.
  if (etag_ != new_create_info.etag ||
      last_modified_time_ != new_create_info.last_modified) {
    received_slices_.clear();
    destination_info_.received_bytes = 0;
  }

  request_info_.url_chain.insert(request_info_.url_chain.end(), chain_iter,
                                 new_create_info.url_chain.end());
  etag_ = new_create_info.etag;
  last_modified_time_ = new_create_info.last_modified;
  response_headers_ = new_create_info.response_headers;
  content_disposition_ = new_create_info.content_disposition;
  // It is possible that the previous download attempt failed right before the
  // response is received. Need to reset the MIME type.
  mime_type_ = new_create_info.mime_type;

  // Don't update observers. This method is expected to be called just before a
  // DownloadFile is created and Start() is called. The observers will be
  // notified when the download transitions to the IN_PROGRESS state.
}

void DownloadItemImpl::NotifyRemoved() {
  for (auto& observer : observers_)
    observer.OnDownloadRemoved(this);
}

void DownloadItemImpl::OnDownloadedFileRemoved() {
  file_externally_removed_ = true;
  DVLOG(20) << __func__ << "() download=" << DebugString(true);
  UpdateObservers();
}

base::WeakPtr<DownloadDestinationObserver>
DownloadItemImpl::DestinationObserverAsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DownloadItemImpl::SetTotalBytes(int64_t total_bytes) {
  total_bytes_ = total_bytes;
}

void DownloadItemImpl::OnAllDataSaved(
    int64_t total_bytes,
    std::unique_ptr<crypto::SecureHash> hash_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!AllDataSaved());
  destination_info_.all_data_saved = true;
  SetTotalBytes(total_bytes);
  UpdateProgress(total_bytes, 0);
  received_slices_.clear();
  SetHashState(std::move(hash_state));
  hash_state_.reset();  // No need to retain hash_state_ since we are done with
                        // the download and don't expect to receive any more
                        // data.

  if (received_bytes_at_length_mismatch_ > 0) {
    if (total_bytes > received_bytes_at_length_mismatch_) {
      RecordDownloadCountWithSource(
          MORE_BYTES_RECEIVED_AFTER_CONTENT_LENGTH_MISMATCH_COUNT,
          download_source_);
    } else if (total_bytes == received_bytes_at_length_mismatch_) {
      RecordDownloadCountWithSource(
          NO_BYTES_RECEIVED_AFTER_CONTENT_LENGTH_MISMATCH_COUNT,
          download_source_);
    } else {
      // This could happen if the content changes on the server.
    }
  }
  DVLOG(20) << __func__ << "() download=" << DebugString(true);
  UpdateObservers();
}

void DownloadItemImpl::MarkAsComplete() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK(AllDataSaved());
  destination_info_.end_time = base::Time::Now();
  TransitionTo(COMPLETE_INTERNAL);
  UpdateObservers();
}

void DownloadItemImpl::DestinationUpdate(
    int64_t bytes_so_far,
    int64_t bytes_per_sec,
    const std::vector<DownloadItem::ReceivedSlice>& received_slices) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // If the download is in any other state we don't expect any
  // DownloadDestinationObserver callbacks. An interruption or a cancellation
  // results in a call to ReleaseDownloadFile which invalidates the weak
  // reference held by the DownloadFile and hence cuts off any pending
  // callbacks.
  DCHECK(state_ == TARGET_PENDING_INTERNAL || state_ == IN_PROGRESS_INTERNAL ||
         state_ == INTERRUPTED_TARGET_PENDING_INTERNAL);

  // There must be no pending deferred_interrupt_reason_.
  DCHECK(deferred_interrupt_reason_ == DOWNLOAD_INTERRUPT_REASON_NONE ||
         state_ == INTERRUPTED_TARGET_PENDING_INTERNAL);

  DVLOG(20) << __func__ << "() so_far=" << bytes_so_far
            << " per_sec=" << bytes_per_sec
            << " download=" << DebugString(true);

  UpdateProgress(bytes_so_far, bytes_per_sec);
  received_slices_ = received_slices;
  TRACE_EVENT_INSTANT1("download", "DownloadItemUpdated",
                       TRACE_EVENT_SCOPE_THREAD, "bytes_so_far",
                       GetReceivedBytes());

  if (IsPaused() && destination_info_.received_bytes == bytes_so_far)
    return;

  UpdateObservers();
}

void DownloadItemImpl::DestinationError(
    DownloadInterruptReason reason,
    int64_t bytes_so_far,
    std::unique_ptr<crypto::SecureHash> secure_hash) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // If the download is in any other state we don't expect any
  // DownloadDestinationObserver callbacks. An interruption or a cancellation
  // results in a call to ReleaseDownloadFile which invalidates the weak
  // reference held by the DownloadFile and hence cuts off any pending
  // callbacks.
  DCHECK(state_ == TARGET_PENDING_INTERNAL || state_ == IN_PROGRESS_INTERNAL);
  DVLOG(20) << __func__
            << "() reason:" << DownloadInterruptReasonToString(reason)
            << " this:" << DebugString(true);

  InterruptWithPartialState(bytes_so_far, std::move(secure_hash), reason);
  UpdateObservers();
}

void DownloadItemImpl::DestinationCompleted(
    int64_t total_bytes,
    std::unique_ptr<crypto::SecureHash> secure_hash) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // If the download is in any other state we don't expect any
  // DownloadDestinationObserver callbacks. An interruption or a cancellation
  // results in a call to ReleaseDownloadFile which invalidates the weak
  // reference held by the DownloadFile and hence cuts off any pending
  // callbacks.
  DCHECK(state_ == TARGET_PENDING_INTERNAL || state_ == IN_PROGRESS_INTERNAL ||
         state_ == INTERRUPTED_TARGET_PENDING_INTERNAL);
  DVLOG(20) << __func__ << "() download=" << DebugString(true);

  OnAllDataSaved(total_bytes, std::move(secure_hash));
  MaybeCompleteDownload();
}

void DownloadItemImpl::SetDelegate(DownloadItemImplDelegate* delegate) {
  delegate_->Detach();
  delegate_ = delegate;
  delegate_->Attach();
}

void DownloadItemImpl::SetDownloadId(uint32_t download_id) {
  download_id_ = download_id;
}

void DownloadItemImpl::SetAutoResumeCountForTesting(int32_t auto_resume_count) {
  auto_resume_count_ = auto_resume_count;
}

// **** Download progression cascade

void DownloadItemImpl::Init(bool active,
                            DownloadItem::DownloadCreationType download_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  download_type_ = download_type;
  std::string file_name;
  if (download_type == TYPE_HISTORY_IMPORT) {
    // target_path_ works for History and Save As versions.
    file_name = GetTargetFilePath().AsUTF8Unsafe();
  } else {
    // See if it's set programmatically.
    file_name = GetForcedFilePath().AsUTF8Unsafe();
    // Possibly has a 'download' attribute for the anchor.
    if (file_name.empty())
      file_name = GetSuggestedFilename();
    // From the URL file name.
    if (file_name.empty())
      file_name = GetURL().ExtractFileName();
  }

  auto active_data = std::make_unique<DownloadItemActivatedData>(
      download_type, GetId(), GetOriginalUrl(), GetURL(), file_name,
      GetDangerType(), GetReceivedBytes(), HasUserGesture());

  if (active) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
        "download", "DownloadItemActive",
        TRACE_ID_WITH_SCOPE("DownloadItemActive", download_id_),
        "download_item", std::move(active_data));
    ukm_download_id_ = ukm::NoURLSourceId();
  } else {
    TRACE_EVENT_INSTANT1("download", "DownloadItemActive",
                         TRACE_EVENT_SCOPE_THREAD, "download_item",
                         std::move(active_data));
  }

  DVLOG(20) << __func__ << "() " << DebugString(true);
}

// We're starting the download.
void DownloadItemImpl::Start(
    std::unique_ptr<DownloadFile> file,
    DownloadJob::CancelRequestCallback cancel_request_callback,
    const DownloadCreateInfo& new_create_info,
    URLLoaderFactoryProvider::URLLoaderFactoryProviderPtr
        url_loader_factory_provider) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(!download_file_) << "last interrupt reason: "
                         << DownloadInterruptReasonToString(last_reason_)
                         << ", state: " << DebugDownloadStateString(state_);
  DVLOG(20) << __func__ << "() this=" << DebugString(true);
  RecordDownloadCountWithSource(START_COUNT, download_source_);

  download_file_ = std::move(file);
  job_ = DownloadJobFactory::CreateJob(
      this, std::move(cancel_request_callback), new_create_info, false,
      std::move(url_loader_factory_provider),
      base::BindRepeating(&DownloadItemImpl::BindWakeLockProvider,
                          weak_ptr_factory_.GetWeakPtr()));
  if (job_->IsParallelizable()) {
    RecordParallelizableDownloadCount(START_COUNT, IsParallelDownloadEnabled());
  }

  deferred_interrupt_reason_ = DOWNLOAD_INTERRUPT_REASON_NONE;

  if (state_ == CANCELLED_INTERNAL) {
    // The download was in the process of resuming when it was cancelled. Don't
    // proceed.
    ReleaseDownloadFile(true);
    job_->Cancel(true);
    return;
  }

  // The state could be one of the following:
  //
  // INITIAL_INTERNAL: A normal download attempt.
  //
  // RESUMING_INTERNAL: A resumption attempt. May or may not have been
  //     successful.
  DCHECK(state_ == INITIAL_INTERNAL || state_ == RESUMING_INTERNAL);

  // If the state_ is INITIAL_INTERNAL, then the target path must be empty.
  DCHECK(state_ != INITIAL_INTERNAL || GetTargetFilePath().empty());

  // If a resumption attempted failed, or if the download was DOA, then the
  // download should go back to being interrupted.
  if (new_create_info.result != DOWNLOAD_INTERRUPT_REASON_NONE) {
    DCHECK(!download_file_);

    // Download requests that are interrupted by Start() should result in a
    // DownloadCreateInfo with an intact DownloadSaveInfo.
    DCHECK(new_create_info.save_info);

    std::unique_ptr<crypto::SecureHash> hash_state =
        new_create_info.save_info->hash_state
            ? new_create_info.save_info->hash_state->Clone()
            : nullptr;

    hash_state_ = std::move(hash_state);
    destination_info_.hash.clear();
    deferred_interrupt_reason_ = new_create_info.result;
    TransitionTo(INTERRUPTED_TARGET_PENDING_INTERNAL);
    // We're posting the call to DetermineDownloadTarget() instead of calling it
    // directly to ensure that OnDownloadTargetDetermined() is not called
    // synchronously. See crbug.com/1209856 for more details.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&DownloadItemImpl::DetermineDownloadTarget,
                                  weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (state_ == INITIAL_INTERNAL) {
    RecordNewDownloadStarted(net::NetworkChangeNotifier::GetConnectionType(),
                             download_source_);
    if (job_->IsParallelizable()) {
      RecordParallelizableDownloadCount(NEW_DOWNLOAD_COUNT,
                                        IsParallelDownloadEnabled());
    }
    RecordDownloadMimeType(mime_type_, transient_);
    DownloadContent file_type = DownloadContentFromMimeType(mime_type_, false);
    DownloadConnectionSecurity state = CheckDownloadConnectionSecurity(
        new_create_info.url(), new_create_info.url_chain);
    RecordDownloadValidationMetrics(DownloadMetricsCallsite::kDownloadItem,
                                    state, file_type);

    if (!delegate_->IsOffTheRecord()) {
      RecordDownloadCountWithSource(NEW_DOWNLOAD_COUNT_NORMAL_PROFILE,
                                    download_source_);
      RecordDownloadMimeTypeForNormalProfile(mime_type_, transient_);
    }
  }

  // Successful download start.
  DCHECK(download_file_);
  DCHECK(job_);

  if (state_ == RESUMING_INTERNAL) {
    if (total_bytes_ == 0 && new_create_info.total_bytes > 0)
      total_bytes_ = new_create_info.total_bytes;
    UpdateValidatorsOnResumption(new_create_info);
  }

  // If the download is not parallel, clear the |received_slices_|.
  if (!received_slices_.empty() && !job_->IsParallelizable()) {
    destination_info_.received_bytes =
        GetMaxContiguousDataBlockSizeFromBeginning(received_slices_);
    received_slices_.clear();
  }

  TransitionTo(TARGET_PENDING_INTERNAL);

  job_->Start(download_file_.get(),
              base::BindRepeating(&DownloadItemImpl::OnDownloadFileInitialized,
                                  weak_ptr_factory_.GetWeakPtr()),
              GetReceivedSlices());

  rename_handler_ = delegate_->GetRenameHandlerForDownload(this);
}

void DownloadItemImpl::OnDownloadFileInitialized(DownloadInterruptReason result,
                                                 int64_t bytes_wasted) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(state_ == TARGET_PENDING_INTERNAL ||
         state_ == INTERRUPTED_TARGET_PENDING_INTERNAL)
      << "Unexpected state: " << DebugDownloadStateString(state_);

  DVLOG(20) << __func__
            << "() result:" << DownloadInterruptReasonToString(result);

  if (bytes_wasted > 0) {
    bytes_wasted_ += bytes_wasted;
    delegate_->ReportBytesWasted(this);
  }

  // Handle download interrupt reason.
  if (result != DOWNLOAD_INTERRUPT_REASON_NONE) {
    ReleaseDownloadFile(true);
    InterruptAndDiscardPartialState(result);
  }

  DetermineDownloadTarget();
}

void DownloadItemImpl::DetermineDownloadTarget() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(20) << __func__ << "() " << DebugString(true);

  RecordDownloadCountWithSource(DETERMINE_DOWNLOAD_TARGET_COUNT,
                                download_source_);
  delegate_->DetermineDownloadTarget(
      this, base::BindOnce(&DownloadItemImpl::OnDownloadTargetDetermined,
                           weak_ptr_factory_.GetWeakPtr()));
}

// Called by delegate_ when the download target path has been determined.
void DownloadItemImpl::OnDownloadTargetDetermined(
    DownloadTargetInfo target_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (state_ == CANCELLED_INTERNAL)
    return;

  DCHECK(state_ == TARGET_PENDING_INTERNAL ||
         state_ == INTERRUPTED_TARGET_PENDING_INTERNAL);
  DVLOG(20) << __func__ << "() target_path:" << target_info.target_path.value()
            << " intermediate_path:" << target_info.intermediate_path.value()
            << " disposition:" << target_info.target_disposition
            << " danger_type:" << target_info.danger_type
            << " interrupt_reason:"
            << DownloadInterruptReasonToString(target_info.interrupt_reason)
            << " this:" << DebugString(true);

  RecordDownloadCountWithSource(DOWNLOAD_TARGET_DETERMINED_COUNT,
                                download_source_);

  if (IsCancellation(target_info.interrupt_reason) ||
      target_info.target_path.empty()) {
    Cancel(true);
    return;
  }

  // There were no other pending errors, and we just failed to determined the
  // download target. The target path, if it is non-empty, should be considered
  // suspect. The safe option here is to interrupt the download without doing an
  // intermediate rename. In the case of a new download, we'll lose the partial
  // data that may have been downloaded, but that should be a small loss.
  if (state_ == TARGET_PENDING_INTERNAL &&
      target_info.interrupt_reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
    deferred_interrupt_reason_ = target_info.interrupt_reason;
    insecure_download_status_ = target_info.insecure_download_status;
    TransitionTo(INTERRUPTED_TARGET_PENDING_INTERNAL);
    OnTargetResolved();
    return;
  }

  destination_info_.target_path = target_info.target_path;
  destination_info_.target_disposition = target_info.target_disposition;
  SetDangerType(target_info.danger_type);
  insecure_download_status_ = target_info.insecure_download_status;
  if (!target_info.display_name.empty()) {
    SetDisplayName(target_info.display_name);
  }
  if (!target_info.mime_type.empty()) {
    mime_type_ = target_info.mime_type;
  }
#if BUILDFLAG(IS_MAC)
  file_tags_ = target_info.file_tags;
#endif

  // This was an interrupted download that was looking for a filename. Resolve
  // early without performing the intermediate rename. If there is a
  // DownloadFile, then that should be renamed to the intermediate name before
  // we can interrupt the download. Otherwise we may lose intermediate state.
  if (state_ == INTERRUPTED_TARGET_PENDING_INTERNAL && !download_file_) {
    OnTargetResolved();
    return;
  }

  // We want the intermediate and target paths to refer to the same directory so
  // that they are both on the same device and subject to same
  // space/permission/availability constraints.
  DCHECK(target_info.intermediate_path.DirName() ==
         target_info.target_path.DirName());

  // During resumption, we may choose to proceed with the same intermediate
  // file. No rename is necessary if our intermediate file already has the
  // correct name.
  //
  // The intermediate name may change from its original value during filename
  // determination on resumption, for example if the reason for the interruption
  // was the download target running out space, resulting in a user prompt.
  if (target_info.intermediate_path == GetFullPath()) {
    OnDownloadRenamedToIntermediateName(DOWNLOAD_INTERRUPT_REASON_NONE,
                                        target_info.intermediate_path);
    return;
  }

  // Rename to intermediate name.
  // TODO(asanka): Skip this rename if AllDataSaved() is true. This avoids a
  //               spurious rename when we can just rename to the final
  //               filename. Unnecessary renames may cause bugs like
  //               http://crbug.com/74187.
  DCHECK(!IsSavePackageDownload());

  GetDownloadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DownloadFile::RenameAndUniquify,
          // Safe because we control download file lifetime.
          base::Unretained(download_file_.get()), target_info.intermediate_path,
          base::BindOnce(&DownloadItemImpl::OnDownloadRenamedToIntermediateName,
                         weak_ptr_factory_.GetWeakPtr())));
}

void DownloadItemImpl::OnDownloadRenamedToIntermediateName(
    DownloadInterruptReason reason,
    const base::FilePath& full_path) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(state_ == TARGET_PENDING_INTERNAL ||
         state_ == INTERRUPTED_TARGET_PENDING_INTERNAL);
  DCHECK(download_file_);
  DVLOG(20) << __func__ << "() download=" << DebugString(true);

  if (DOWNLOAD_INTERRUPT_REASON_NONE == reason) {
    SetFullPath(full_path);
  } else {
    // TODO(asanka): Even though the rename failed, it may still be possible to
    // recover the partial state from the 'before' name.
    deferred_interrupt_reason_ = reason;
    TransitionTo(INTERRUPTED_TARGET_PENDING_INTERNAL);
  }
  OnTargetResolved();
}

void DownloadItemImpl::OnTargetResolved() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(20) << __func__ << "() download=" << DebugString(true);
  DCHECK((state_ == TARGET_PENDING_INTERNAL &&
          deferred_interrupt_reason_ == DOWNLOAD_INTERRUPT_REASON_NONE) ||
         (state_ == INTERRUPTED_TARGET_PENDING_INTERNAL &&
          deferred_interrupt_reason_ != DOWNLOAD_INTERRUPT_REASON_NONE))
      << " deferred_interrupt_reason_:"
      << DownloadInterruptReasonToString(deferred_interrupt_reason_)
      << " this:" << DebugString(true);

  // This transition is here to ensure that the DownloadItemImpl state machine
  // doesn't transition to INTERRUPTED or IN_PROGRESS from
  // TARGET_PENDING_INTERNAL directly. Doing so without passing through
  // OnTargetResolved() can result in an externally visible state where the
  // download is interrupted but doesn't have a target path associated with it.
  //
  // While not terrible, this complicates the DownloadItem<->Observer
  // relationship since an observer that needs a target path in order to respond
  // properly to an interruption will need to wait for another OnDownloadUpdated
  // notification.  This requirement currently affects all of our UIs.
  TransitionTo(TARGET_RESOLVED_INTERNAL);

  if (DOWNLOAD_INTERRUPT_REASON_NONE != deferred_interrupt_reason_) {
    InterruptWithPartialState(GetReceivedBytes(), std::move(hash_state_),
                              deferred_interrupt_reason_);
    deferred_interrupt_reason_ = DOWNLOAD_INTERRUPT_REASON_NONE;
    UpdateObservers();
    return;
  }

  TransitionTo(IN_PROGRESS_INTERNAL);
  // TODO(asanka): Calling UpdateObservers() prior to MaybeCompleteDownload() is
  // not safe. The download could be in an underminate state after invoking
  // observers. http://crbug.com/586610
  UpdateObservers();
  MaybeCompleteDownload();
}

// When SavePackage downloads MHTML to GData (see
// SavePackageFilePickerChromeOS), GData calls MaybeCompleteDownload() like it
// does for non-SavePackage downloads, but SavePackage downloads never satisfy
// IsDownloadReadyForCompletion(). GDataDownloadObserver manually calls
// DownloadItem::UpdateObservers() when the upload completes so that
// SavePackage notices that the upload has completed and runs its normal
// Finish() pathway. MaybeCompleteDownload() is never the mechanism by which
// SavePackage completes downloads. SavePackage always uses its own Finish() to
// mark downloads complete.
void DownloadItemImpl::MaybeCompleteDownload() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!IsDownloadReadyForCompletion(
          base::BindRepeating(&DownloadItemImpl::MaybeCompleteDownload,
                              weak_ptr_factory_.GetWeakPtr())))
    return;
  // Confirm we're in the proper set of states to be here; have all data, have a
  // history handle, (validated or safe).
  DCHECK_EQ(IN_PROGRESS_INTERNAL, state_);
  DCHECK(!IsDangerous());
  DCHECK(AllDataSaved());

  OnDownloadCompleting();
}

// Called by MaybeCompleteDownload() when it has determined that the download
// is ready for completion.
void DownloadItemImpl::OnDownloadCompleting() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (state_ != IN_PROGRESS_INTERNAL)
    return;

  DVLOG(20) << __func__ << "() " << DebugString(true);
  DCHECK(!GetTargetFilePath().empty());
  DCHECK(!IsDangerous());

  DCHECK(download_file_);

  // Unilaterally rename; even if it already has the right name,
  // we need the annotation.
  DownloadFile::RenameCompletionCallback rename_callback =
      base::BindOnce(&DownloadItemImpl::OnRenameAndAnnotateDone,
                     weak_ptr_factory_.GetWeakPtr());

#if BUILDFLAG(IS_ANDROID)
  if (GetTargetFilePath().IsContentUri()) {
    GetDownloadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&DownloadFile::PublishDownload,
                       // Safe because we control download file lifetime.
                       base::Unretained(download_file_.get()),
                       std::move(rename_callback)));
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  mojo::PendingRemote<quarantine::mojom::Quarantine> quarantine;
  auto quarantine_callback = delegate_->GetQuarantineConnectionCallback();
  if (quarantine_callback)
    quarantine_callback.Run(quarantine.InitWithNewPipeAndPassReceiver());

  GetDownloadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DownloadFile::RenameAndAnnotate,
          base::Unretained(download_file_.get()), GetTargetFilePath(),
          delegate_->GetApplicationClientIdForFileScanning(),
          delegate_->IsOffTheRecord() ? GURL() : GetURL(),
          delegate_->IsOffTheRecord() ? GURL() : GetReferrerUrl(),
          delegate_->IsOffTheRecord() ? std::nullopt : GetRequestInitiator(),
          std::move(quarantine), std::move(rename_callback)));
}

void DownloadItemImpl::OnRenameAndAnnotateDone(
    DownloadInterruptReason reason,
    const base::FilePath& full_path) {
  DownloadFile::RenameCompletionCallback rename_callback =
      base::BindOnce(&DownloadItemImpl::OnDownloadRenamedToFinalName,
                     weak_ptr_factory_.GetWeakPtr());
  if (rename_handler_) {
    renaming_ = true;

    rename_handler_->Start(
        base::BindRepeating(&DownloadItemImpl::UpdateRenameProgress,
                            weak_ptr_factory_.GetWeakPtr()),
        std::move(rename_callback));
    return;
  }

  OnDownloadRenamedToFinalName(reason, full_path);
}

void DownloadItemImpl::OnDownloadRenamedToFinalName(
    DownloadInterruptReason reason,
    const base::FilePath& full_path) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!IsSavePackageDownload());

  renaming_ = false;

  // If a cancel or interrupt hit, we'll cancel the DownloadFile, which
  // will result in deleting the file on the file thread.  So we don't
  // care about the name having been changed.
  if (state_ != IN_PROGRESS_INTERNAL)
    return;

  DVLOG(20) << __func__ << "() full_path = \"" << full_path.value() << "\" "
            << DebugString(false);

  if (DOWNLOAD_INTERRUPT_REASON_NONE != reason) {
    // Failure to perform the final rename is considered fatal. TODO(asanka): It
    // may not be, in which case we should figure out whether we can recover the
    // state.
    InterruptAndDiscardPartialState(reason);
    UpdateObservers();
    return;
  }

  DCHECK_EQ(GetTargetFilePath(), full_path);

  if (full_path != GetFullPath()) {
    // full_path is now the current and target file path.
    DCHECK(!full_path.empty());
    SetFullPath(full_path);
  }

#if BUILDFLAG(IS_MAC)
  base::mac::SetFileTags(full_path, file_tags_);
#endif

  // Complete the download and release the DownloadFile.
  DCHECK(download_file_);
  ReleaseDownloadFile(false);

  // We're not completely done with the download item yet, but at this
  // point we're committed to complete the download.  Cancels (or Interrupts,
  // though it's not clear how they could happen) after this point will be
  // ignored.
  TransitionTo(COMPLETING_INTERNAL);

  if (delegate_->ShouldOpenDownload(
          this, base::BindOnce(&DownloadItemImpl::DelayedDownloadOpened,
                               weak_ptr_factory_.GetWeakPtr()))) {
    Completed();
  } else {
    delegate_delayed_complete_ = true;
    UpdateObservers();
  }
}

void DownloadItemImpl::DelayedDownloadOpened(bool auto_opened) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto_opened_ = auto_opened;
  Completed();
}

void DownloadItemImpl::Completed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DVLOG(20) << __func__ << "() " << DebugString(false);

  DCHECK(AllDataSaved());
  destination_info_.end_time = base::Time::Now();
  TransitionTo(COMPLETE_INTERNAL);

  bool is_parallelizable = job_ && job_->IsParallelizable();
  RecordDownloadCompleted(GetReceivedBytes(), is_parallelizable,
                          net::NetworkChangeNotifier::GetConnectionType(),
                          download_source_);
  if (!delegate_->IsOffTheRecord()) {
    RecordDownloadCountWithSource(COMPLETED_COUNT_NORMAL_PROFILE,
                                  download_source_);
  }
  if (is_parallelizable) {
    RecordParallelizableDownloadCount(COMPLETED_COUNT,
                                      IsParallelDownloadEnabled());
    int64_t content_length = -1;
    if (response_headers_->response_code() != net::HTTP_PARTIAL_CONTENT) {
      content_length = response_headers_->GetContentLength();
    } else {
      int64_t first_byte = -1;
      int64_t last_byte = -1;
      response_headers_->GetContentRangeFor206(&first_byte, &last_byte,
                                               &content_length);
    }
  }

  if (auto_opened_) {
    // If it was already handled by the delegate, do nothing.
  } else if (GetOpenWhenComplete() || ShouldOpenFileBasedOnExtension() ||
             IsTemporary()) {
    // If the download is temporary, like in drag-and-drop, do not open it but
    // we still need to set it auto-opened so that it can be removed from the
    // download shelf.
    if (!IsTemporary())
      OpenDownload();

    auto_opened_ = true;
  }

  base::TimeDelta time_since_start = GetEndTime() - GetStartTime();

  // If all data is saved, the number of received bytes is resulting file size.
  int resulting_file_size = GetReceivedBytes();

  DownloadUkmHelper::RecordDownloadCompleted(
      ukm_download_id_, resulting_file_size, time_since_start, bytes_wasted_);

  // After all of the records are done, then update the observers.
  UpdateObservers();
}

// **** End of Download progression cascade

void DownloadItemImpl::InterruptAndDiscardPartialState(
    DownloadInterruptReason reason) {
  InterruptWithPartialState(0, nullptr, reason);
}

void DownloadItemImpl::InterruptWithPartialState(
    int64_t bytes_so_far,
    std::unique_ptr<crypto::SecureHash> hash_state,
    DownloadInterruptReason reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(DOWNLOAD_INTERRUPT_REASON_NONE, reason);
  DVLOG(20) << __func__
            << "() reason:" << DownloadInterruptReasonToString(reason)
            << " bytes_so_far:" << bytes_so_far
            << " hash_state:" << (hash_state ? "Valid" : "Invalid")
            << " this=" << DebugString(true);

  // Somewhat counter-intuitively, it is possible for us to receive an
  // interrupt after we've already been interrupted.  The generation of
  // interrupts from the file thread Renames and the generation of
  // interrupts from disk writes go through two different mechanisms (driven
  // by rename requests from UI thread and by write requests from IO thread,
  // respectively), and since we choose not to keep state on the File thread,
  // this is the place where the races collide.  It's also possible for
  // interrupts to race with cancels.
  switch (state_) {
    case CANCELLED_INTERNAL:
    // If the download is already cancelled, then there's no point in
    // transitioning out to interrupted.
    case COMPLETING_INTERNAL:
    case COMPLETE_INTERNAL:
      // Already complete.
      return;

    case INITIAL_INTERNAL:
    case MAX_DOWNLOAD_INTERNAL_STATE:
      NOTREACHED_IN_MIGRATION();
      return;

    case TARGET_PENDING_INTERNAL:
    case INTERRUPTED_TARGET_PENDING_INTERNAL:
      // Postpone recognition of this error until after file name determination
      // has completed and the intermediate file has been renamed to simplify
      // resumption conditions. The target determination logic is much simpler
      // if the state of the download remains constant until that stage
      // completes.
      //
      // current_path_ may be empty because it is possible for
      // DownloadItem to receive a DestinationError prior to the
      // download file initialization complete callback.
      if (!IsCancellation(reason)) {
        UpdateProgress(bytes_so_far, 0);
        SetHashState(std::move(hash_state));
        deferred_interrupt_reason_ = reason;
        TransitionTo(INTERRUPTED_TARGET_PENDING_INTERNAL);
        return;
      }
      // else - Fallthrough for cancellation handling which is equivalent to the
      // IN_PROGRESS state.
      [[fallthrough]];

    case IN_PROGRESS_INTERNAL:
    case TARGET_RESOLVED_INTERNAL: {
      // last_reason_ needs to be set for GetResumeMode() to work.
      last_reason_ = reason;

      ResumeMode resume_mode = GetResumeMode();
      ReleaseDownloadFile(resume_mode != ResumeMode::IMMEDIATE_CONTINUE &&
                          resume_mode != ResumeMode::USER_CONTINUE);
    } break;

    case RESUMING_INTERNAL:
    case INTERRUPTED_INTERNAL:
      DCHECK(!download_file_);
      // The first non-cancel interrupt reason wins in cases where multiple
      // things go wrong.
      if (!IsCancellation(reason))
        return;

      last_reason_ = reason;
      // There is no download file and this is transitioning from INTERRUPTED
      // to CANCELLED. The intermediate file is no longer usable, and should
      // be deleted.
      DeleteDownloadFile();
      break;
  }

  // Reset all data saved, as even if we did save all the data we're going to go
  // through another round of downloading when we resume. There's a potential
  // problem here in the abstract, as if we did download all the data and then
  // run into a continuable error, on resumption we won't download any more
  // data.  However, a) there are currently no continuable errors that can occur
  // after we download all the data, and b) if there were, that would probably
  // simply result in a null range request, which would generate a
  // DestinationCompleted() notification from the DownloadFile, which would
  // behave properly with setting all_data_saved_ to false here.
  destination_info_.all_data_saved = false;

  if (GetFullPath().empty()) {
    hash_state_.reset();
    destination_info_.hash.clear();
    destination_info_.received_bytes = 0;
    received_slices_.clear();
  } else {
    UpdateProgress(bytes_so_far, 0);
    SetHashState(std::move(hash_state));
  }

  if (job_)
    job_->Cancel(false);

  if (IsCancellation(reason)) {
    RecordDownloadCountWithSource(CANCELLED_COUNT, download_source_);
    if (job_ && job_->IsParallelizable()) {
      RecordParallelizableDownloadCount(CANCELLED_COUNT,
                                        IsParallelDownloadEnabled());
    }
    DCHECK_EQ(last_reason_, reason);
    TransitionTo(CANCELLED_INTERNAL);
    return;
  }

  RecordDownloadInterrupted(reason, GetReceivedBytes(), total_bytes_,
                            job_ && job_->IsParallelizable(),
                            IsParallelDownloadEnabled(), download_source_);

  base::TimeDelta time_since_start = base::Time::Now() - GetStartTime();
  int resulting_file_size = GetReceivedBytes();
  std::optional<int> change_in_file_size;
  if (total_bytes_ >= 0) {
    change_in_file_size = total_bytes_ - resulting_file_size;
  }

  DownloadUkmHelper::RecordDownloadInterrupted(
      ukm_download_id_, change_in_file_size, reason, resulting_file_size,
      time_since_start, bytes_wasted_);
  if (reason == DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH) {
    received_bytes_at_length_mismatch_ = GetReceivedBytes();
  }

  // TODO(asanka): This is not good. We can transition to interrupted from
  // target-pending, which is something we don't want to do. Perhaps we should
  // explicitly transition to target-resolved prior to switching to interrupted.
  DCHECK_EQ(last_reason_, reason);
  TransitionTo(INTERRUPTED_INTERNAL);
  delegate_->DownloadInterrupted(this);
  AutoResumeIfValid();
}

void DownloadItemImpl::UpdateProgress(int64_t bytes_so_far,
                                      int64_t bytes_per_sec) {
  destination_info_.received_bytes = bytes_so_far;
  bytes_per_sec_ = bytes_per_sec;

  // If we've received more data than we were expecting (bad server info?),
  // revert to 'unknown size mode'.
  if (bytes_so_far > total_bytes_)
    total_bytes_ = 0;
}

void DownloadItemImpl::SetHashState(
    std::unique_ptr<crypto::SecureHash> hash_state) {
  hash_state_ = std::move(hash_state);
  if (!hash_state_) {
    destination_info_.hash.clear();
    return;
  }

  std::unique_ptr<crypto::SecureHash> clone_of_hash_state(hash_state_->Clone());
  std::vector<char> hash_value(clone_of_hash_state->GetHashLength());
  clone_of_hash_state->Finish(&hash_value.front(), hash_value.size());
  destination_info_.hash.assign(hash_value.begin(), hash_value.end());
}

void DownloadItemImpl::ReleaseDownloadFile(bool destroy_file) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(20) << __func__ << "() destroy_file:" << destroy_file;

  if (destroy_file) {
    if (download_file_) {
      GetDownloadTaskRunner()->PostTask(
          FROM_HERE,
          // Will be deleted at end of task execution.
          base::BindOnce(&DownloadFileCancel, std::move(download_file_)));
    } else {
      DeleteDownloadFile();
    }
    // Avoid attempting to reuse the intermediate file by clearing out
    // current_path_ and received slices.
    destination_info_.current_path.clear();
    received_slices_.clear();
  } else if (download_file_) {
    GetDownloadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(base::IgnoreResult(&DownloadFileDetach),
                                  // Will be deleted at end of task execution.
                                  std::move(download_file_)));
  }
  // Don't accept any more messages from the DownloadFile, and null
  // out any previous "all data received".  This also breaks links to
  // other entities we've given out weak pointers to.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void DownloadItemImpl::DeleteDownloadFile() {
  if (GetFullPath().empty())
    return;
  GetDownloadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&DeleteDownloadedFile), GetFullPath()));
  destination_info_.current_path.clear();
}

bool DownloadItemImpl::IsDownloadReadyForCompletion(
    base::OnceClosure state_change_notification) {
  // If the download hasn't progressed to the IN_PROGRESS state, then it's not
  // ready for completion.
  if (state_ != IN_PROGRESS_INTERNAL)
    return false;

  // If we don't have all the data, the download is not ready for
  // completion.
  if (!AllDataSaved())
    return false;

  // If the download is dangerous, but not yet validated, it's not ready for
  // completion.
  if (IsDangerous())
    return false;

  // If the download is insecure, but not yet validated, it's not ready for
  // completion.
  if (IsInsecure())
    return false;

  // Check for consistency before invoking delegate. Since there are no pending
  // target determination calls and the download is in progress, both the target
  // and current paths should be non-empty and they should point to the same
  // directory.
  DCHECK(!GetTargetFilePath().empty());
  DCHECK(!GetFullPath().empty());
  DCHECK(GetTargetFilePath().DirName() == GetFullPath().DirName());

  // Give the delegate a chance to hold up a stop sign.  It'll call
  // use back through the passed callback if it does and that state changes.
  if (!delegate_->ShouldCompleteDownload(this,
                                         std::move(state_change_notification)))
    return false;

  return true;
}

void DownloadItemImpl::TransitionTo(DownloadInternalState new_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (state_ == new_state)
    return;

  DownloadInternalState old_state = state_;
  state_ = new_state;

  DCHECK(IsSavePackageDownload()
             ? IsValidSavePackageStateTransition(old_state, new_state)
             : IsValidStateTransition(old_state, new_state))
      << "Invalid state transition from:" << DebugDownloadStateString(old_state)
      << " to:" << DebugDownloadStateString(new_state);

  switch (state_) {
    case INITIAL_INTERNAL:
      NOTREACHED_IN_MIGRATION();
      break;

    case TARGET_PENDING_INTERNAL:
    case TARGET_RESOLVED_INTERNAL:
      break;

    case INTERRUPTED_TARGET_PENDING_INTERNAL:
      DCHECK_NE(DOWNLOAD_INTERRUPT_REASON_NONE, deferred_interrupt_reason_)
          << "Interrupt reason must be set prior to transitioning into "
             "TARGET_PENDING";
      break;

    case IN_PROGRESS_INTERNAL:
      DCHECK(!GetFullPath().empty()) << "Current output path must be known.";
      DCHECK(!GetTargetFilePath().empty()) << "Target path must be known.";
      DCHECK(GetFullPath().DirName() == GetTargetFilePath().DirName())
          << "Current output directory must match target directory.";
      DCHECK(download_file_) << "Output file must be owned by download item.";
      DCHECK(job_) << "Must have active download job.";
      DCHECK(!job_->is_paused())
          << "At the time a download enters IN_PROGRESS state, "
             "it must not be paused.";
      break;

    case COMPLETING_INTERNAL:
      DCHECK(AllDataSaved()) << "All data must be saved prior to completion.";
      DCHECK(!download_file_)
          << "Download file must be released prior to completion.";
      DCHECK(!GetTargetFilePath().empty()) << "Target path must be known.";
      DCHECK(GetFullPath() == GetTargetFilePath())
          << "Current output path must match target path.";

      TRACE_EVENT_INSTANT2("download", "DownloadItemCompleting",
                           TRACE_EVENT_SCOPE_THREAD, "bytes_so_far",
                           GetReceivedBytes(), "final_hash",
                           destination_info_.hash);
      break;

    case COMPLETE_INTERNAL:
      TRACE_EVENT_INSTANT1("download", "DownloadItemFinished",
                           TRACE_EVENT_SCOPE_THREAD, "auto_opened",
                           auto_opened_ ? "yes" : "no");
      break;

    case INTERRUPTED_INTERNAL:
      DCHECK(!download_file_)
          << "Download file must be released prior to interruption.";
      DCHECK_NE(last_reason_, DOWNLOAD_INTERRUPT_REASON_NONE);
      TRACE_EVENT_INSTANT2("download", "DownloadItemInterrupted",
                           TRACE_EVENT_SCOPE_THREAD, "interrupt_reason",
                           DownloadInterruptReasonToString(last_reason_),
                           "bytes_so_far", GetReceivedBytes());
      break;

    case RESUMING_INTERNAL:
      TRACE_EVENT_INSTANT2("download", "DownloadItemResumed",
                           TRACE_EVENT_SCOPE_THREAD, "interrupt_reason",
                           DownloadInterruptReasonToString(last_reason_),
                           "bytes_so_far", GetReceivedBytes());
      break;

    case CANCELLED_INTERNAL:
      TRACE_EVENT_INSTANT1("download", "DownloadItemCancelled",
                           TRACE_EVENT_SCOPE_THREAD, "bytes_so_far",
                           GetReceivedBytes());
      break;

    case MAX_DOWNLOAD_INTERNAL_STATE:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  DVLOG(20) << __func__ << "() from:" << DebugDownloadStateString(old_state)
            << " to:" << DebugDownloadStateString(state_)
            << " this = " << DebugString(true);
  bool is_done =
      (state_ == COMPLETE_INTERNAL || state_ == INTERRUPTED_INTERNAL ||
       state_ == RESUMING_INTERNAL || state_ == CANCELLED_INTERNAL);
  bool was_done =
      (old_state == COMPLETE_INTERNAL || old_state == INTERRUPTED_INTERNAL ||
       old_state == RESUMING_INTERNAL || old_state == CANCELLED_INTERNAL);

  // Termination
  if (is_done && !was_done)
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "download", "DownloadItemActive",
        TRACE_ID_WITH_SCOPE("DownloadItemActive", download_id_));

  // Resumption
  if (was_done && !is_done) {
    std::string file_name(GetTargetFilePath().BaseName().AsUTF8Unsafe());
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
        "download", "DownloadItemActive",
        TRACE_ID_WITH_SCOPE("DownloadItemActive", download_id_),
        "download_item",
        std::make_unique<DownloadItemActivatedData>(
            TYPE_ACTIVE_DOWNLOAD, GetId(), GetOriginalUrl(), GetURL(),
            file_name, GetDangerType(), GetReceivedBytes(), HasUserGesture()));
  }
}

void DownloadItemImpl::SetDangerType(DownloadDangerType danger_type) {
  if (danger_type != danger_type_) {
    TRACE_EVENT_INSTANT1("download", "DownloadItemSaftyStateUpdated",
                         TRACE_EVENT_SCOPE_THREAD, "danger_type",
                         GetDownloadDangerNames(danger_type).c_str());
  }
  danger_type_ = danger_type;
}

void DownloadItemImpl::SetFullPath(const base::FilePath& new_path) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(20) << __func__ << "() new_path = \"" << new_path.value() << "\" "
            << DebugString(true);
  DCHECK(!new_path.empty());

  TRACE_EVENT_INSTANT2("download", "DownloadItemRenamed",
                       TRACE_EVENT_SCOPE_THREAD, "old_filename",
                       destination_info_.current_path.AsUTF8Unsafe(),
                       "new_filename", new_path.AsUTF8Unsafe());

  destination_info_.current_path = new_path;
}

void DownloadItemImpl::AutoResumeIfValid() {
  DVLOG(20) << __func__ << "() " << DebugString(true);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ResumeMode mode = GetResumeMode();
  if (mode != ResumeMode::IMMEDIATE_RESTART &&
      mode != ResumeMode::IMMEDIATE_CONTINUE) {
    return;
  }

  auto_resume_count_++;

  ResumeInterruptedDownload(ResumptionRequestSource::AUTOMATIC);
}

void DownloadItemImpl::ResumeInterruptedDownload(
    ResumptionRequestSource source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // If we're not interrupted, ignore the request; our caller is drunk.
  if (state_ != INTERRUPTED_INTERNAL)
    return;

  // We are starting a new request. Shake off all pending operations.
  CHECK(!download_file_);
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Reset the appropriate state if restarting.
  ResumeMode mode = GetResumeMode();
  if (mode == ResumeMode::IMMEDIATE_RESTART ||
      mode == ResumeMode::USER_RESTART) {
    LOG_IF(ERROR, !GetFullPath().empty())
        << "Download full path should be empty before resumption";
    destination_info_.received_bytes = 0;
    last_modified_time_.clear();
    etag_.clear();
    destination_info_.hash.clear();
    hash_state_.reset();
    received_slices_.clear();
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("download_manager_resume", R"(
        semantics {
          sender: "Download Manager"
          description:
            "When user resumes downloading a file, a network request is made "
            "to fetch it."
          trigger:
            "User resumes a download."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disabled in settings, but it is activated "
            "by direct user action."
          chrome_policy {
            DownloadRestrictions {
              DownloadRestrictions: 3
            }
          }
        })");
  // Avoid using the WebContents even if it's still around. Resumption requests
  // are consistently routed through the no-renderer code paths so that the
  // request will not be dropped if the WebContents (and by extension, the
  // associated renderer) goes away before a response is received.
  std::unique_ptr<DownloadUrlParameters> download_params(
      new DownloadUrlParameters(GetURL(), traffic_annotation));
  download_params->set_file_path(GetFullPath());
  int64_t offset = 0;
  if (received_slices_.size() > 0) {
    std::vector<DownloadItem::ReceivedSlice> slices_to_download =
        FindSlicesToDownload(received_slices_);
    offset = slices_to_download[0].offset;
  } else {
    offset = GetReceivedBytes();
  }

  download_params->set_offset(offset);
  download_params->set_last_modified(GetLastModifiedTime());
  download_params->set_etag(GetETag());
  download_params->set_hash_of_partial_file(GetHash());
  download_params->set_hash_state(std::move(hash_state_));
  download_params->set_guid(guid_);
  if (!HasStrongValidators() &&
      base::FeatureList::IsEnabled(
          features::kAllowDownloadResumptionWithoutStrongValidators)) {
    int64_t validation_length = GetDownloadValidationLengthConfig();
    if (download_params->offset() > validation_length) {
      // There is enough data for validation, set the file_offset so
      // DownloadFileImpl will validate the data between offset to
      // file_offset.
      download_params->set_use_if_range(false);
      download_params->set_file_offset(download_params->offset());
      download_params->set_offset(download_params->offset() -
                                  validation_length);
    } else {
      // There is not enough data for validation, simply overwrites the
      // existing data from the beginning.
      download_params->set_offset(0);
    }
  }

  // TODO(xingliu): Read |fetch_error_body| and |request_headers_| from the
  // cache, and don't copy them into DownloadItemImpl.
  download_params->set_fetch_error_body(fetch_error_body_);
  for (const auto& header : request_headers_) {
    download_params->add_request_header(header.first, header.second);
  }

  if (request_info_.range_request_from != kInvalidRange ||
      request_info_.range_request_to != kInvalidRange) {
    // Arbitrary range request doesn't use If-Range header and resumption
    // invalidation.
    download_params->set_range_request_offset(request_info_.range_request_from,
                                              request_info_.range_request_to);
    download_params->set_use_if_range(false);
    download_params->set_offset(offset);
    download_params->set_file_offset(offset);
  }

  // The offset is calculated after decompression, so the range request cannot
  // involve any compression,
  download_params->add_request_header("Accept-Encoding", "identity");

  // Note that resumed downloads disallow redirects. Hence the referrer URL
  // (which is the contents of the Referer header for the last download request)
  // will only be sent to the URL returned by GetURL().
  download_params->set_referrer(GetReferrerUrl());
  download_params->set_referrer_policy(net::ReferrerPolicy::NEVER_CLEAR);
  download_params->set_cross_origin_redirects(
      network::mojom::RedirectMode::kError);

  TransitionTo(RESUMING_INTERNAL);
  RecordDownloadCountWithSource(source == ResumptionRequestSource::USER
                                    ? MANUAL_RESUMPTION_COUNT
                                    : AUTO_RESUMPTION_COUNT,
                                download_source_);

  base::TimeDelta time_since_start = base::Time::Now() - GetStartTime();
  DownloadUkmHelper::RecordDownloadResumed(ukm_download_id_, GetResumeMode(),
                                           time_since_start);

  delegate_->ResumeInterruptedDownload(
      std::move(download_params),
      request_info_.serialized_embedder_download_data);

  if (job_)
    job_->Resume(false);
}

// static
DownloadItem::DownloadState DownloadItemImpl::InternalToExternalState(
    DownloadInternalState internal_state) {
  switch (internal_state) {
    case INITIAL_INTERNAL:
    case TARGET_PENDING_INTERNAL:
    case TARGET_RESOLVED_INTERNAL:
    case INTERRUPTED_TARGET_PENDING_INTERNAL:
    // TODO(asanka): Introduce an externally visible state to distinguish
    // between the above states and IN_PROGRESS_INTERNAL. The latter (the
    // state where the download is active and has a known target) is the state
    // that most external users are interested in.
    case IN_PROGRESS_INTERNAL:
      return IN_PROGRESS;
    case COMPLETING_INTERNAL:
      return IN_PROGRESS;
    case COMPLETE_INTERNAL:
      return COMPLETE;
    case CANCELLED_INTERNAL:
      return CANCELLED;
    case INTERRUPTED_INTERNAL:
      return INTERRUPTED;
    case RESUMING_INTERNAL:
      return IN_PROGRESS;
    case MAX_DOWNLOAD_INTERNAL_STATE:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return MAX_DOWNLOAD_STATE;
}

// static
DownloadItemImpl::DownloadInternalState
DownloadItemImpl::ExternalToInternalState(DownloadState external_state) {
  switch (external_state) {
    case IN_PROGRESS:
      return IN_PROGRESS_INTERNAL;
    case COMPLETE:
      return COMPLETE_INTERNAL;
    case CANCELLED:
      return CANCELLED_INTERNAL;
    case INTERRUPTED:
      return INTERRUPTED_INTERNAL;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return MAX_DOWNLOAD_INTERNAL_STATE;
}

// static
bool DownloadItemImpl::IsValidSavePackageStateTransition(
    DownloadInternalState from,
    DownloadInternalState to) {
#if DCHECK_IS_ON()
  switch (from) {
    case INITIAL_INTERNAL:
    case TARGET_PENDING_INTERNAL:
    case INTERRUPTED_TARGET_PENDING_INTERNAL:
    case TARGET_RESOLVED_INTERNAL:
    case COMPLETING_INTERNAL:
    case COMPLETE_INTERNAL:
    case INTERRUPTED_INTERNAL:
    case RESUMING_INTERNAL:
    case CANCELLED_INTERNAL:
      return false;

    case IN_PROGRESS_INTERNAL:
      return to == CANCELLED_INTERNAL || to == COMPLETE_INTERNAL ||
             to == INTERRUPTED_INTERNAL;

    case MAX_DOWNLOAD_INTERNAL_STATE:
      NOTREACHED_IN_MIGRATION();
  }
  return false;
#else
  return true;
#endif
}

// static
bool DownloadItemImpl::IsValidStateTransition(DownloadInternalState from,
                                              DownloadInternalState to) {
#if DCHECK_IS_ON()
  switch (from) {
    case INITIAL_INTERNAL:
      return to == TARGET_PENDING_INTERNAL ||
             to == INTERRUPTED_TARGET_PENDING_INTERNAL;

    case TARGET_PENDING_INTERNAL:
      return to == INTERRUPTED_TARGET_PENDING_INTERNAL ||
             to == TARGET_RESOLVED_INTERNAL || to == CANCELLED_INTERNAL;

    case INTERRUPTED_TARGET_PENDING_INTERNAL:
      return to == TARGET_RESOLVED_INTERNAL || to == CANCELLED_INTERNAL;

    case TARGET_RESOLVED_INTERNAL:
      return to == IN_PROGRESS_INTERNAL || to == INTERRUPTED_INTERNAL ||
             to == CANCELLED_INTERNAL;

    case IN_PROGRESS_INTERNAL:
      return to == COMPLETING_INTERNAL || to == CANCELLED_INTERNAL ||
             to == INTERRUPTED_INTERNAL;

    case COMPLETING_INTERNAL:
      return to == COMPLETE_INTERNAL;

    case COMPLETE_INTERNAL:
      return false;

    case INTERRUPTED_INTERNAL:
      return to == RESUMING_INTERNAL || to == CANCELLED_INTERNAL;

    case RESUMING_INTERNAL:
      return to == TARGET_PENDING_INTERNAL ||
             to == INTERRUPTED_TARGET_PENDING_INTERNAL ||
             to == TARGET_RESOLVED_INTERNAL || to == CANCELLED_INTERNAL;

    case CANCELLED_INTERNAL:
      return false;

    case MAX_DOWNLOAD_INTERNAL_STATE:
      NOTREACHED_IN_MIGRATION();
  }
  return false;
#else
  return true;
#endif  // DCHECK_IS_ON()
}

const char* DownloadItemImpl::DebugDownloadStateString(
    DownloadInternalState state) {
  switch (state) {
    case INITIAL_INTERNAL:
      return "INITIAL";
    case TARGET_PENDING_INTERNAL:
      return "TARGET_PENDING";
    case INTERRUPTED_TARGET_PENDING_INTERNAL:
      return "INTERRUPTED_TARGET_PENDING";
    case TARGET_RESOLVED_INTERNAL:
      return "TARGET_RESOLVED";
    case IN_PROGRESS_INTERNAL:
      return "IN_PROGRESS";
    case COMPLETING_INTERNAL:
      return "COMPLETING";
    case COMPLETE_INTERNAL:
      return "COMPLETE";
    case CANCELLED_INTERNAL:
      return "CANCELLED";
    case INTERRUPTED_INTERNAL:
      return "INTERRUPTED";
    case RESUMING_INTERNAL:
      return "RESUMING";
    case MAX_DOWNLOAD_INTERNAL_STATE:
      break;
  }
  NOTREACHED_IN_MIGRATION() << "Unknown download state " << state;
  return "unknown";
}

const char* DownloadItemImpl::DebugResumeModeString(ResumeMode mode) {
  switch (mode) {
    case ResumeMode::INVALID:
      return "INVALID";
    case ResumeMode::IMMEDIATE_CONTINUE:
      return "IMMEDIATE_CONTINUE";
    case ResumeMode::IMMEDIATE_RESTART:
      return "IMMEDIATE_RESTART";
    case ResumeMode::USER_CONTINUE:
      return "USER_CONTINUE";
    case ResumeMode::USER_RESTART:
      return "USER_RESTART";
  }
  NOTREACHED_IN_MIGRATION() << "Unknown resume mode " << static_cast<int>(mode);
  return "unknown";
}

std::pair<int64_t, int64_t> DownloadItemImpl::GetRangeRequestOffset() const {
  return std::make_pair(request_info_.range_request_from,
                        request_info_.range_request_to);
}

void DownloadItemImpl::UpdateRenameProgress(int64_t bytes_so_far,
                                            int64_t bytes_per_sec) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(renaming_);

  destination_info_.uploaded_bytes = bytes_so_far;

  UpdateObservers();
}

}  // namespace download
