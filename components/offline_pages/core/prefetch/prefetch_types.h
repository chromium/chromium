// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_TYPES_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_TYPES_H_

#include <array>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/offline_pages/core/client_id.h"
#include "url/gurl.h"

namespace offline_pages {

// Controls how to reschedule a background task.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offlinepages
enum class PrefetchBackgroundTaskRescheduleType {
  // No reschedule.
  NO_RESCHEDULE,
  // Reschedules the task in the next available WiFi window after 15 minutes
  // have passed.
  RESCHEDULE_WITHOUT_BACKOFF,
  // Reschedules the task with backoff included.
  RESCHEDULE_WITH_BACKOFF,
  // Reschedules the task due to the fact that it is killed due to the system
  // constraint.
  RESCHEDULE_DUE_TO_SYSTEM,
  // Reschedules the task after 1 day.
  SUSPEND
};

// Status for sending prefetch request to the server. This has a matching type
// in enums.xml which must be adjusted if we add any new values here.
enum class PrefetchRequestStatus {
  // Request completed successfully.
  kSuccess = 0,
  // Request failed due to to local network problem, unrelated to server load
  // levels. The caller will simply reschedule the retry in the next available
  // WiFi window after 15 minutes have passed.
  kShouldRetryWithoutBackoff = 1,
  // Request failed probably related to transient server problems. The caller
  // will reschedule the retry with backoff included.
  kShouldRetryWithBackoff = 2,
  // Request failed with error indicating that the server no longer knows how
  // to service a request. The caller will prevent network requests for the
  // period of 1 day.
  kShouldSuspendNotImplemented = 3,
  // Request failed with error indicating that the client is forbidden. The
  // caller will prevent network requests for the period of 1 day.
  kShouldSuspendForbidden = 4,
  // The request failed because the service URL was blocked by the domain
  // administrator.
  kShouldSuspendBlockedByAdministrator = 5,
  // The request was answered with a 403 Forbidden response including a message
  // indicating that the request was forbidden specifically by an OPS request
  // filtering rule.
  kShouldSuspendForbiddenByOPS = 6,
  // The server responded with 403 forbidden due to a filter rule though the
  // last request was successful.
  kShouldSuspendNewlyForbiddenByOPS = 7,
  // A request for no URLs (i.e. server-enabled check) completed successfully.
  kEmptyRequestSuccess = 8,
  // kMaxValue should always be the last type.
  kMaxValue = kEmptyRequestSuccess
};

// Status indicating the page rendering status in the server.
enum class RenderStatus {
  // The page is rendered.
  RENDERED,
  // The page is still being processed.
  PENDING,
  // The page failed to render.
  FAILED,
  // Failed due to bundle size limits.
  EXCEEDED_LIMIT
};

// Information about the page rendered in the server.
struct RenderPageInfo {
  RenderPageInfo();
  RenderPageInfo(const RenderPageInfo& other);

  // The URL of the page that was rendered.
  std::string url;
  // The final URL after redirects. Empty if the final URL is url.
  std::string redirect_url;
  // Status of the render attempt.
  RenderStatus status = RenderStatus::FAILED;
  // Resource name for the body which can be read via the ByteStream API.
  // Set only when |status| is RENDERED.
  std::string body_name;
  // Length of the body in bytes. Set only when |status| is RENDERED.
  int64_t body_length = 0LL;
  // Time the page was rendered. Set only when |status| is RENDERED.
  base::Time render_time;
};

// Contains all states an item can be at during its progress through the
// prefetching pipeline. State progression will normally follow the order below,
// but some states might be skipped and there might be backtracking when
// retrying a failed operation.
//
// Changes to this enum must be reflected in the respective metrics enum named
// OfflinePrefetchItemState in enums.xml. Use the exact same integer value for
// each mirrored entry. Existing elements should never have their assigned
// values changed. Changes should also be reflected in
// |PrefetchTaskTestBase::kOrderedPrefetchItemStates|.
enum class PrefetchItemState : int {
  // New request just received from the client.
  NEW_REQUEST = 0,
  // The item has been included in a GeneratePageBundle RPC requesting the
  // creation of an archive for its URL.
  SENT_GENERATE_PAGE_BUNDLE = 10,
  // The archive was not immediately available (cached) upon the request and
  // is now waiting for a GCM message notifying of its archiving operation
  // completion.
  AWAITING_GCM = 20,
  // The GCM message notifying of the archiving operation completion was
  // received for this item.
  RECEIVED_GCM = 30,
  // A GetOperation RPC was sent for this item to query for the final results
  // of its archiving request.
  SENT_GET_OPERATION = 40,
  // Information was received about a successfully created archive for this
  // item that can now be downloaded.
  RECEIVED_BUNDLE = 50,
  // This item's archive is currently being downloaded.
  DOWNLOADING = 60,
  // The archive has been downloaded, waiting to be imported into offline pages
  // model.
  DOWNLOADED = 70,
  // The archive is being imported into offline pages model.
  IMPORTING = 80,
  // Item has finished processing, successfully or otherwise, and is waiting to
  // be processed for stats reporting to UMA.
  FINISHED = 90,
  // UMA stats have been reported and the item is being kept just long enough
  // to confirm that the same URL is not being repeatedly requested by its
  // client.
  ZOMBIE = 100,
  // Max item state, needed for histograms
  kMaxValue = ZOMBIE
};

base::Optional<PrefetchItemState> ToPrefetchItemState(int value);

// Error codes used to identify the reason why a prefetch entry has finished
// processing in the pipeline. This values are only meaningful for entries in
// the "finished" state.
//
// New entries can be added anywhere as long as they are assigned unique values
// and kept in ascending order. Deprecated entries should be labeled as such but
// never removed. Assigned values should never be reused. Remember to update the
// kMaxValue value if adding a new trailing item.
//
// Changes to this enum must be reflected in the respective metrics enum named
// OflinePrefetchItemErrorCode in enums.xml. Use the exact same integer value
// for each mirrored entry.
enum class PrefetchItemErrorCode : int {
  // The entry had gone through the pipeline and successfully completed
  // prefetching. Explicitly setting to 0 as that is the default value for the
  // respective SQLite column.
  SUCCESS = 0,
  // Got too many URLs from suggestions, canceled this one. See kMaxUrlsToSend
  // defined in GeneratePageBundleTask.
  TOO_MANY_NEW_URLS = 100,
  // An error happened while attempting to download the archive file.
  DOWNLOAD_ERROR = 200,
  // An error happened while importing the downloaded archive file int the
  // Offline Pages system.
  IMPORT_ERROR = 300,
  // Got a failure result from GetOperation (or the GeneratePageBundle
  // metadata).
  ARCHIVING_FAILED = 400,
  // Got a failure result from GetOperation or GeneratePageBundle that a
  // server-side limit on the page was exceeded.
  ARCHIVING_LIMIT_EXCEEDED = 500,
  // These next STALE_AT_* values identify entries that stayed for too long in
  // the same pipeline bucket so that their "freshness date" was considered too
  // old for what was allowed for that bucket. See StaleEntryFinalizerTask for
  // more details.
  STALE_AT_NEW_REQUEST = 600,
  STALE_AT_AWAITING_GCM = 700,
  STALE_AT_RECEIVED_GCM = 800,
  STALE_AT_RECEIVED_BUNDLE = 900,
  STALE_AT_DOWNLOADING = 1000,
  STALE_AT_IMPORTING = 1050,
  STALE_AT_UNKNOWN = 1100,
  // The item was terminated because it remained in the pipeline for more than
  // 7 days.
  STUCK = 1150,
  // The item had some invalid data, probably due to database corruption.
  INVALID_ITEM = 1175,
  // Exceeded maximum retries for get operation request.
  GET_OPERATION_MAX_ATTEMPTS_REACHED = 1200,
  // Exceeded maximum retries limit for generate page bundle request.
  GENERATE_PAGE_BUNDLE_REQUEST_MAX_ATTEMPTS_REACHED = 1300,
  // Exceeded maximum retries for download.
  DOWNLOAD_MAX_ATTEMPTS_REACHED = 1400,
  // Clock was set back too far in time.
  MAXIMUM_CLOCK_BACKWARD_SKEW_EXCEEDED = 1500,
  // The archive importing was not completed probably due to that Chrome was
  // killed before everything finishes.
  IMPORT_LOST = 1600,
  // The page suggestion is no longer valid, so the item no longer needs to be
  // downloaded.
  SUGGESTION_INVALIDATED = 1700,
  // Note: Must always have the same value as the last actual entry.
  kMaxValue = SUGGESTION_INVALIDATED
};

base::Optional<PrefetchItemErrorCode> ToPrefetchItemErrorCode(int value);

// Callback invoked upon completion of a prefetch request.
using PrefetchRequestFinishedCallback =
    base::OnceCallback<void(PrefetchRequestStatus status,
                            const std::string& operation_name,
                            const std::vector<RenderPageInfo>& pages)>;

// Holds information about a suggested URL to be prefetched.
struct PrefetchURL {
  PrefetchURL(const std::string& id,
              const GURL& url,
              const base::string16& title);
  PrefetchURL(const std::string& id,
              const GURL& url,
              const base::string16& title,
              const GURL& thumbnail_url,
              const GURL& favicon_url,
              const std::string& snippet,
              const std::string& attribution);
  ~PrefetchURL();
  PrefetchURL(const PrefetchURL& other);

  // Client provided ID to allow the matching of provided URLs to the respective
  // work item in the prefetching system within that client's assigned
  // namespace. It can be any string value and it will not be used internally
  // for de-duplication.
  std::string id;

  // This URL will be prefetched by the service.
  GURL url;

  // The title of the page.
  base::string16 title;

  // URL for a thumbnail that represents the page. May be empty if no thumbnail
  // is available.
  GURL thumbnail_url;

  // URL for page's favicon.
  GURL favicon_url;

  // Article snippet.
  std::string snippet;

  // Identifies the page's publisher.
  std::string attribution;
};

// Result of a completed download.
struct PrefetchDownloadResult {
  PrefetchDownloadResult();
  PrefetchDownloadResult(const std::string& download_id,
                         const base::FilePath& file_path,
                         int64_t file_size);
  PrefetchDownloadResult(const PrefetchDownloadResult& other);
  bool operator==(const PrefetchDownloadResult& other) const;

  std::string download_id;
  bool success = false;
  base::FilePath file_path;
  int64_t file_size = 0;
};

// Describes all the info needed to import an archive.
struct PrefetchArchiveInfo {
  PrefetchArchiveInfo();
  PrefetchArchiveInfo(const PrefetchArchiveInfo& other);
  ~PrefetchArchiveInfo();
  bool empty() const;

  int64_t offline_id = 0;  // Defaults to invalid id.
  ClientId client_id;
  GURL url;
  GURL final_archived_url;
  base::string16 title;
  base::FilePath file_path;
  int64_t file_size = 0;
  GURL favicon_url;
  std::string snippet;
  std::string attribution;
};

// These operators are implemented for testing only, see test_util.cc.
// They are provided here to avoid ODR problems.
std::ostream& operator<<(std::ostream& out,
                         PrefetchBackgroundTaskRescheduleType value);
std::ostream& operator<<(std::ostream& out, PrefetchRequestStatus value);
std::ostream& operator<<(std::ostream& out, RenderStatus value);
std::ostream& operator<<(std::ostream& out, const PrefetchItemState& value);
std::ostream& operator<<(std::ostream& out, PrefetchItemErrorCode value);

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_TYPES_H_
