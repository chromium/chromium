// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_UTILS_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_UTILS_H_

#include <memory>

#include "components/download/database/download_db_entry.h"
#include "components/download/database/in_progress/download_entry.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_item_impl.h"
#include "components/download/public/common/download_source.h"
#include "components/download/public/common/resume_mode.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"

namespace net {
class HttpRequestHeaders;
}

namespace network {
struct ResourceRequest;
}

namespace download {
struct DownloadCreateInfo;
struct DownloadSaveInfo;
class DownloadUrlParameters;

// Used to check if the URL is safe. For most cases, this is
// ChildProcessSecurityPolicy::CanRequestURL.
using URLSecurityPolicy =
    base::RepeatingCallback<bool(int /* render_process_id */, const GURL& url)>;

// Handle the url request completion status and return the interrupt reasons.
// |cert_status| is ignored if error_code is not net::ERR_ABORTED.
COMPONENTS_DOWNLOAD_EXPORT DownloadInterruptReason
HandleRequestCompletionStatus(net::Error error_code,
                              bool has_strong_validators,
                              net::CertStatus cert_status,
                              bool is_partial_request,
                              DownloadInterruptReason abort_reason);

// Parse the HTTP server response code.
// If |fetch_error_body| is true, most of HTTP response codes will be accepted
// as successful response.
COMPONENTS_DOWNLOAD_EXPORT DownloadInterruptReason
HandleSuccessfulServerResponse(const net::HttpResponseHeaders& http_headers,
                               DownloadSaveInfo* save_info,
                               bool fetch_error_body);

// Parse response headers and update |create_info| accordingly.
COMPONENTS_DOWNLOAD_EXPORT void HandleResponseHeaders(
    const net::HttpResponseHeaders* headers,
    DownloadCreateInfo* create_info);

// Create a ResourceRequest from |params|.
COMPONENTS_DOWNLOAD_EXPORT std::unique_ptr<network::ResourceRequest>
CreateResourceRequest(DownloadUrlParameters* params);

// Gets LoadFlags from |params|.
COMPONENTS_DOWNLOAD_EXPORT int GetLoadFlags(DownloadUrlParameters* params,
                                            bool has_upload_data);

// Gets addtional request headers from |params|.
COMPONENTS_DOWNLOAD_EXPORT std::unique_ptr<net::HttpRequestHeaders>
GetAdditionalRequestHeaders(DownloadUrlParameters* params);

// Helper functions for DownloadItem -> DownloadDBEntry for DownloadDB.
COMPONENTS_DOWNLOAD_EXPORT DownloadDBEntry
CreateDownloadDBEntryFromItem(const DownloadItemImpl& item);

// Helper function to convert DownloadDBEntry to DownloadEntry.
// TODO(qinmin): remove this function after DownloadEntry is deprecated.
COMPONENTS_DOWNLOAD_EXPORT std::unique_ptr<DownloadEntry>
CreateDownloadEntryFromDownloadDBEntry(base::Optional<DownloadDBEntry> entry);

COMPONENTS_DOWNLOAD_EXPORT uint64_t GetUniqueDownloadId();

// Given the interrupt reason, and whether restart and user action are required,
// determine the final ResomeMode.
COMPONENTS_DOWNLOAD_EXPORT ResumeMode
GetDownloadResumeMode(const GURL& url,
                      DownloadInterruptReason reason,
                      bool restart_required,
                      bool user_action_required);

// Check if a download is in terminal state given its url, state and interrupt
// reason.
COMPONENTS_DOWNLOAD_EXPORT bool IsDownloadDone(
    const GURL& url,
    DownloadItem::DownloadState state,
    DownloadInterruptReason reason);

COMPONENTS_DOWNLOAD_EXPORT bool DeleteDownloadedFile(
    const base::FilePath& path);

// Rename downloaded file |from_path| to a new |display_name|.
COMPONENTS_DOWNLOAD_EXPORT DownloadItem::DownloadRenameResult
RenameDownloadedFile(const base::FilePath& from_path,
                     const base::FilePath& display_name);

// Finch parameter key value for number of bytes used for content validation
// during resumption.
constexpr char kDownloadContentValidationLengthFinchKey[] =
    "download_validation_length";

// Get the number of bytes to validate from finch configuration.
int64_t GetDownloadValidationLengthConfig();

// Finch parameter key value for the time to delete expired downloads in days.
constexpr char kExpiredDownloadDeleteTimeFinchKey[] =
    "expired_download_delete_days";

// Returns the time to delete expired downloads.
COMPONENTS_DOWNLOAD_EXPORT base::TimeDelta GetExpiredDownloadDeleteTime();

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_UTILS_H_
