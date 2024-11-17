// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Each download is represented by a DownloadItem, and all DownloadItems
// are owned by the DownloadManager which maintains a global list of all
// downloads. DownloadItems are created when a user initiates a download,
// and exist for the duration of the browser life time.
//
// Download observers:
//   DownloadItem::Observer:
//     - allows observers to receive notifications about one download from start
//       to completion
// Use AddObserver() / RemoveObserver() on the appropriate download object to
// receive state updates.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list_types.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_source.h"
#include "net/base/isolation_info.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

class GURL;

namespace base {
class FilePath;
class Time;
class TimeDelta;
}  // namespace base

namespace net {
class HttpResponseHeaders;
}

namespace download {
class DownloadFile;
class DownloadItemRenameHandler;

// One DownloadItem per download. This is the model class that stores all the
// state for a download.
class COMPONENTS_DOWNLOAD_EXPORT DownloadItem : public base::SupportsUserData {
 public:
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.download
  enum DownloadState {
    // Download is actively progressing.
    IN_PROGRESS = 0,

    // Download is completely finished.
    COMPLETE,

    // Download has been cancelled.
    CANCELLED,

    // This state indicates that the download has been interrupted.
    INTERRUPTED,

    // Maximum value.
    MAX_DOWNLOAD_STATE
  };

  // How the final target path should be used.
  enum TargetDisposition {
    TARGET_DISPOSITION_OVERWRITE,  // Overwrite if the target already exists.
    TARGET_DISPOSITION_PROMPT      // Prompt the user for the actual
                                   // target. Implies
                                   // TARGET_DISPOSITION_OVERWRITE.
  };

  // How download item is created. Used for trace event.
  enum DownloadCreationType {
    TYPE_ACTIVE_DOWNLOAD,
    TYPE_HISTORY_IMPORT,
    TYPE_SAVE_PAGE_AS
  };

  // Result of a rename attempt for a download item.
  enum DownloadRenameResult {
    SUCCESS = 0,
    FAILURE_NAME_CONFLICT = 1,
    FAILURE_NAME_TOO_LONG = 2,
    FAILURE_NAME_INVALID = 3,
    FAILURE_UNAVAILABLE = 4,
    FAILURE_UNKNOWN = 5,
    RESULT_MAX = FAILURE_UNKNOWN
  };

  // The insecure status for a download item.
  enum InsecureDownloadStatus {
    // Target not yet determined, so status not yet available.
    UNKNOWN = 0,
    // Download is not insecure.
    SAFE = 1,
    // Download has been explicitly OK'd by the user. Only used on Desktop.
    VALIDATED = 2,
    // Download is insecure, and the user should be warned.
    WARN = 3,
    // Download is insecure, and the user should see an error.
    BLOCK = 4,
    // Download is insecure, and it should be silently dropped.
    SILENT_BLOCK = 5,
  };

  // Callback used with AcquireFileAndDeleteDownload().
  using AcquireFileCallback = base::OnceCallback<void(const base::FilePath&)>;
  using RenameDownloadCallback = base::OnceCallback<void(DownloadRenameResult)>;
  // Used to represent an invalid download ID.
  static const uint32_t kInvalidId;

  // Interface that observers of a particular download must implement in order
  // to receive updates to the download's status.
  class COMPONENTS_DOWNLOAD_EXPORT Observer : public base::CheckedObserver {
   public:
    virtual void OnDownloadUpdated(DownloadItem* download) {}
    virtual void OnDownloadOpened(DownloadItem* download) {}
    virtual void OnDownloadRemoved(DownloadItem* download) {}

    // Called when the download is being destroyed. This happens after
    // every OnDownloadRemoved() as well as when the DownloadManager is going
    // down.
    virtual void OnDownloadDestroyed(DownloadItem* download) {}

    ~Observer() override;
  };

  // A slice of the target file that has been received so far, used when
  // parallel downloading is enabled. Slices should have different offsets
  // so that they don't overlap. |finished| will be marked as true when the
  // download stream is successfully completed.
  struct ReceivedSlice {
    ReceivedSlice(int64_t offset, int64_t received_bytes)
        : offset(offset), received_bytes(received_bytes), finished(false) {}

    ReceivedSlice(int64_t offset, int64_t received_bytes, bool finished)
        : offset(offset), received_bytes(received_bytes), finished(finished) {}

    bool operator==(const ReceivedSlice& rhs) const {
      return offset == rhs.offset && received_bytes == rhs.received_bytes &&
             finished == rhs.finished;
    }

    int64_t offset;
    int64_t received_bytes;
    bool finished;
  };

  using ReceivedSlices = std::vector<DownloadItem::ReceivedSlice>;

  ~DownloadItem() override = default;

  // Observation ---------------------------------------------------------------

  virtual void AddObserver(DownloadItem::Observer* observer) = 0;
  virtual void RemoveObserver(DownloadItem::Observer* observer) = 0;
  virtual void UpdateObservers() = 0;

  // User Actions --------------------------------------------------------------

  // Called when the user has validated the download of a dangerous file.
  virtual void ValidateDangerousDownload() = 0;

  // Called when the user has validated the download of an insecure file.
  virtual void ValidateInsecureDownload() = 0;

  // Called to acquire a dangerous download. Mmakes a temp copy of the
  // download file, and invokes |callback| with the path to the temp
  // copy. The caller is responsible for cleanup.  Note: It is important
  // for |callback| to be valid since the downloaded file will not be
  // cleaned up if the callback fails.
  virtual void CopyDownload(AcquireFileCallback callback) = 0;

  // Pause a download.  Will have no effect if the download is already
  // paused.
  virtual void Pause() = 0;

  // Resume a download that has been paused or interrupted. Will have no effect
  // if the download is neither. Only does something if CanResume() returns
  // true.
  virtual void Resume(bool user_resume) = 0;

  // Cancel the download operation.
  //
  // Set |user_cancel| to true if the cancellation was triggered by an explicit
  // user action. Non-user-initiated cancels typically happen when the browser
  // is being closed with in-progress downloads.
  virtual void Cancel(bool user_cancel) = 0;

  // Removes the download from the views and history. If the download was
  // in-progress or interrupted, then the intermediate file will also be
  // deleted.
  virtual void Remove() = 0;

  // Open the file associated with this download.  If the download is
  // still in progress, marks the download to be opened when it is complete.
  virtual void OpenDownload() = 0;

  // Show the download via the OS shell.
  virtual void ShowDownloadInShell() = 0;

  // Rename a downloaded item to |new_name|, implementer should post and reply
  // the result. Do not pass the full file path, just pass the file name portion
  // instead.
  virtual void Rename(const base::FilePath& new_name,
                      RenameDownloadCallback callback) = 0;

  // State accessors -----------------------------------------------------------

  // Retrieve the ID for this download. The ID is provided by the owner of the
  // DownloadItem and is expected to uniquely identify the download within the
  // context of its container during the lifetime of the download. A valid
  // download will never return |kInvalidId|.
  virtual uint32_t GetId() const = 0;

  // Retrieve the GUID for this download. The returned string is never empty and
  // will satisfy `base::Uuid::ParseCaseInsensitive().is_valid()` and uniquely
  // identifies the download during its lifetime.
  virtual const std::string& GetGuid() const = 0;

  // Get the current state of the download. See DownloadState for descriptions
  // of each download state.
  virtual DownloadState GetState() const = 0;

  // Returns the most recent interrupt reason for this download. Returns
  // |DOWNLOAD_INTERRUPT_REASON_NONE| if there is no previous interrupt reason.
  // Interrupted downloads and resumed downloads return the last known interrupt
  // reason.
  virtual DownloadInterruptReason GetLastReason() const = 0;

  // Returns whether download is currently paused explicitly by the user. The
  // download state should be checked in conjunction with this method to
  // determine whether the download was truly paused. Calling Resume() will
  // transition out of this paused state.
  virtual bool IsPaused() const = 0;

  // Whether the download should be allowed to proceed in a metered network.
  virtual bool AllowMetered() const = 0;

  // DEPRECATED. True if this is a temporary download and should not be
  // persisted.
  virtual bool IsTemporary() const = 0;

  // Returns true if the download can be resumed. A download can be resumed if
  // an in-progress download was paused or if an interrupted download requires
  // user-interaction to resume.
  virtual bool CanResume() const = 0;

  // Returns true if the download is in a terminal state. This includes
  // completed downloads, cancelled downloads, and interrupted downloads that
  // can't be resumed.
  virtual bool IsDone() const = 0;

  // Returns the calculated number of bytes wasted (if any).
  virtual int64_t GetBytesWasted() const = 0;

  // Returns the number of times the download has been auto-resumed since last
  // user triggered resumption.
  virtual int32_t GetAutoResumeCount() const = 0;

  //    Origin State accessors -------------------------------------------------

  // Final URL. The primary resource being downloaded is from this URL. This is
  // the tail of GetUrlChain(). May return an empty GURL if there is no valid
  // download URL.
  virtual const GURL& GetURL() const = 0;

  // The complete URL chain including redirects. URL at index i redirected to
  // URL at index i+1.
  virtual const std::vector<GURL>& GetUrlChain() const = 0;

  // The URL that the download request originally attempted to fetch. This may
  // differ from GetURL() if there were redirects. The return value from this
  // accessor is the same as the head of GetUrlChain().
  virtual const GURL& GetOriginalUrl() const = 0;

  // URL of document that is considered the referrer for the original URL.
  virtual const GURL& GetReferrerUrl() const = 0;

  // The serialized EmbedderDownloadData string. This is used by the embedder
  // for placing extra download data, such as the appropriate storage partition
  // for this download.
  virtual const std::string& GetSerializedEmbedderDownloadData() const = 0;

  // URL of the top level frame at the time the download was initiated.
  virtual const GURL& GetTabUrl() const = 0;

  // Referrer URL for top level frame.
  virtual const GURL& GetTabReferrerUrl() const = 0;

  // Origin of the original originator of this download, before redirects, etc.
  virtual const std::optional<url::Origin>& GetRequestInitiator() const = 0;

  // For downloads initiated via <a download>, this is the suggested download
  // filename from the download attribute.
  virtual std::string GetSuggestedFilename() const = 0;

  // Returns the HTTP response headers. This contains a nullptr when the
  // response has not yet been received, and, because the headers are not being
  // persisted, only capture responses received during the lifetime of the
  // current process and profile. Only for consuming headers.
  virtual const scoped_refptr<const net::HttpResponseHeaders>&
  GetResponseHeaders() const = 0;

  // Content-Disposition header value from HTTP response.
  virtual std::string GetContentDisposition() const = 0;

  // Effective MIME type of downloaded content.
  virtual std::string GetMimeType() const = 0;

  // Content-Type header value from HTTP response. May be different from
  // GetMimeType() if a different effective MIME type was chosen after MIME
  // sniffing.
  virtual std::string GetOriginalMimeType() const = 0;

  // Remote address of server serving download contents.
  virtual std::string GetRemoteAddress() const = 0;

  // Whether the download request was initiated in response to a user gesture.
  virtual bool HasUserGesture() const = 0;

  // The page transition type associated with the download request.
  virtual ui::PageTransition GetTransitionType() const = 0;

  // Last-Modified header value.
  virtual const std::string& GetLastModifiedTime() const = 0;

  // ETag header value.
  virtual const std::string& GetETag() const = 0;

  // Whether this download is a SavePackage download.
  virtual bool IsSavePackageDownload() const = 0;

  // DownloadSource prompting this download.
  virtual DownloadSource GetDownloadSource() const = 0;

  // The credentials mode of the request.
  virtual ::network::mojom::CredentialsMode GetCredentialsMode() const = 0;

  // The isolation mode of the request.
  virtual const std::optional<net::IsolationInfo>& GetIsolationInfo() const = 0;

  //    Destination State accessors --------------------------------------------

  // Full path to the downloaded or downloading file. This is the path to the
  // physical file, if one exists. It should be considered a hint; changes to
  // this value and renames of the file on disk are not atomic with each other.
  // May be empty if the in-progress path hasn't been determined yet or if the
  // download was interrupted.
  //
  // DO NOT USE THIS METHOD to access the target path of the DownloadItem. Use
  // GetTargetFilePath() instead. While the download is in progress, the
  // intermediate file named by GetFullPath() may be renamed or disappear
  // completely on the download sequence. The path may also be reset to empty
  // when the download is interrupted.
  virtual const base::FilePath& GetFullPath() const = 0;

  // Target path of an in-progress download. We may be downloading to a
  // temporary or intermediate file (specified by GetFullPath()); this is the
  // name we will use once the download completes.
  // May be empty if the target path hasn't yet been determined.
  virtual const base::FilePath& GetTargetFilePath() const = 0;

  // If the download forced a path rather than requesting name determination,
  // return the path requested.
  virtual const base::FilePath& GetForcedFilePath() const = 0;

  // Path to the temporary file. This could be empty if full path is already
  // determined.
  // TODO(qinmin): merge this with GetFullPath().
  virtual base::FilePath GetTemporaryFilePath() const = 0;

  // Returns the file-name that should be reported to the user. If a display
  // name has been explicitly set using SetDisplayName(), this function returns
  // that display name. Otherwise returns the final target filename.
  virtual base::FilePath GetFileNameToReportUser() const = 0;

  // See TargetDisposition.
  virtual TargetDisposition GetTargetDisposition() const = 0;

  // Final hash of completely downloaded file, or partial hash of an interrupted
  // download; only valid if GetState() == COMPLETED or INTERRUPTED. If
  // non-empty the returned string contains a raw SHA-256 hash (i.e. not hex
  // encoded).
  virtual const std::string& GetHash() const = 0;

  // True if the file associated with the download has been removed by
  // external action.
  virtual bool GetFileExternallyRemoved() const = 0;

  // If the file is successfully deleted, then GetFileExternallyRemoved() will
  // become true, GetFullPath() will become empty, and
  // DownloadItem::OnDownloadUpdated() will be called. Does nothing if
  // GetState() != COMPLETE or GetFileExternallyRemoved() is already true or
  // GetFullPath() is already empty. The callback is always run, and it is
  // always run asynchronously. It will be passed true if the file is
  // successfully deleted or if GetFilePath() was already empty or if
  // GetFileExternallyRemoved() was already true. The callback will be passed
  // false if the DownloadItem was not yet complete or if the file could not be
  // deleted for any reason.
  virtual void DeleteFile(base::OnceCallback<void(bool)> callback) = 0;

  // True if the file that will be written by the download is dangerous
  // and we will require a call to ValidateDangerousDownload() to complete.
  // False if the download is safe or that function has been called.
  virtual bool IsDangerous() const = 0;

  // True if the file that will be written by the download is insecurely
  // delivered and we will require a call to ValidateInsecureDownload() to
  // complete.  False if not insecure or that function has been called.
  virtual bool IsInsecure() const = 0;

  // Why |safety_state_| is not SAFE.
  virtual DownloadDangerType GetDangerType() const = 0;

  // Returns the insecure download status of the download, indicating whether
  // the download should be blocked or the user warned. This may be UNKNOWN if
  // the download target hasn't been determined.
  virtual InsecureDownloadStatus GetInsecureDownloadStatus() const = 0;

  // Gets the pointer to the DownloadFile owned by this object.
  virtual DownloadFile* GetDownloadFile() = 0;

  // Gets a handler to perform the rename for a download item. Returns nullptr
  // if no special rename handling is required.
  virtual DownloadItemRenameHandler* GetRenameHandler() = 0;

#if BUILDFLAG(IS_ANDROID)
  // Gets whether the download is triggered from external app.
  virtual bool IsFromExternalApp() = 0;

  // Whether the original URL must be downloded, e.g. triggered by context
  // menu or from the download service, or has "content-disposition: attachment"
  // in header.
  virtual bool IsMustDownload() = 0;
#endif  // BUILDFLAG(IS_ANDROID)

  //    Progress State accessors -----------------------------------------------

  // Simple calculation of the amount of time remaining to completion. Fills
  // |*remaining| with the amount of time remaining if successful. Fails and
  // returns false if we do not have the number of bytes or the speed so can
  // not estimate.
  virtual bool TimeRemaining(base::TimeDelta* remaining) const = 0;

  // Simple speed estimate in bytes/s
  virtual int64_t CurrentSpeed() const = 0;

  // Rough percent complete. Returns -1 if progress is unknown. 100 if the
  // download is already complete.
  virtual int PercentComplete() const = 0;

  // Returns true if this download has saved all of its data. A download may
  // have saved all its data but still be waiting for some other process to
  // complete before the download is considered complete. E.g. A dangerous
  // download needs to be accepted by the user before the file is renamed to its
  // final name.
  virtual bool AllDataSaved() const = 0;

  // Total number of expected bytes. Returns 0 if the total size is unknown.
  virtual int64_t GetTotalBytes() const = 0;

  // Total number of bytes that have been received and written to the download
  // file.
  virtual int64_t GetReceivedBytes() const = 0;

  // Return the slices that have been received so far, ordered by their offset.
  // This is only used when parallel downloading is enabled.
  virtual const std::vector<ReceivedSlice>& GetReceivedSlices() const = 0;

  // Total number of bytes that have been uploaded to the cloud.
  virtual int64_t GetUploadedBytes() const = 0;

  // Time the download was first started. This timestamp is always valid and
  // doesn't change.
  virtual base::Time GetStartTime() const = 0;

  // Time the download was marked as complete. Returns base::Time() if the
  // download hasn't reached a completion state yet.
  virtual base::Time GetEndTime() const = 0;

  //    Open/Show State accessors ----------------------------------------------

  // Returns true if it is OK to open a folder which this file is inside.
  virtual bool CanShowInFolder() = 0;

  // Returns true if it is OK to open the download.
  virtual bool CanOpenDownload() = 0;

  // Tests if a file type should be opened automatically.
  virtual bool ShouldOpenFileBasedOnExtension() = 0;

  // Tests if a file type should be opened automatically by policy.
  virtual bool ShouldOpenFileByPolicyBasedOnExtension() = 0;

  // Returns true if the download will be auto-opened when complete.
  virtual bool GetOpenWhenComplete() const = 0;

  // Returns true if the download has been auto-opened by the system.
  virtual bool GetAutoOpened() = 0;

  // Returns true if the download has been opened.
  virtual bool GetOpened() const = 0;

  // Time the download was last accessed. Returns NULL if the download has never
  // been opened.
  virtual base::Time GetLastAccessTime() const = 0;

  // Returns whether the download item is transient. Transient items are cleaned
  // up after completion and not shown in the UI, and will not prompt to user
  // for target file path determination.
  virtual bool IsTransient() const = 0;

  // Returns whether the download requires safety checks. Only downloads
  // triggered by Chrome itself are excluded from safety checks.
  virtual bool RequireSafetyChecks() const = 0;

  // Returns whether the download item corresponds to a parallel download. This
  // usually means parallel download has been enabled and the download job is
  // parallelizable.
  virtual bool IsParallelDownload() const = 0;

  // Gets the DownloadCreationType of this item.
  virtual DownloadCreationType GetDownloadCreationType() const = 0;

  // External state transitions/setters ----------------------------------------

  // TODO(rdsmith): These should all be removed; the download item should
  // control its own state transitions.

  // Called if a check of the download contents was performed and the results of
  // the test are available. This should only be called after AllDataSaved() is
  // true. If |reason| is not DOWNLOAD_INTERRUPT_REASON_NONE, then the download
  // file should be blocked.
  // TODO(crbug.com/40525770): Move DownloadInterruptReason out of here and add
  // a new  Interrupt method instead. Same for other methods supporting
  // interruptions.
  virtual void OnContentCheckCompleted(DownloadDangerType danger_type,
                                       DownloadInterruptReason reason) = 0;

  // Called when async scanning completes with the given |danger_type|.
  virtual void OnAsyncScanningCompleted(DownloadDangerType danger_type) = 0;

  // Mark the download to be auto-opened when completed.
  virtual void SetOpenWhenComplete(bool open) = 0;

  // Mark the download as having been opened (without actually opening it).
  virtual void SetOpened(bool opened) = 0;

  // Updates the last access time of the download.
  virtual void SetLastAccessTime(base::Time last_access_time) = 0;

  // Set a display name for the download that will be independent of the target
  // filename. If |name| is not empty, then GetFileNameToReportUser() will
  // return |name|. Has no effect on the final target filename.
  virtual void SetDisplayName(const base::FilePath& name) = 0;

  // Debug/testing -------------------------------------------------------------
  virtual std::string DebugString(bool verbose) const = 0;
  virtual void SimulateErrorForTesting(DownloadInterruptReason reason) = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_ITEM_H_
