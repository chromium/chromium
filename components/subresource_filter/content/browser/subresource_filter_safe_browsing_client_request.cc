// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client_request.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace subresource_filter {

constexpr base::TimeDelta
    SubresourceFilterSafeBrowsingClientRequest::kCheckURLTimeout;

SubresourceFilterSafeBrowsingClientRequest::
    SubresourceFilterSafeBrowsingClientRequest(
        size_t request_id,
        base::TimeTicks start_time,
        scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
            database_manager,
        SubresourceFilterSafeBrowsingClient* client)
    : request_id_(request_id),
      start_time_(start_time),
      database_manager_(std::move(database_manager)),
      client_(client) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M129);
}

SubresourceFilterSafeBrowsingClientRequest::
    ~SubresourceFilterSafeBrowsingClientRequest() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M129);
  if (!request_completed_)
    database_manager_->CancelCheck(this);
  timer_.Stop();
}

void SubresourceFilterSafeBrowsingClientRequest::Start(const GURL& url) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M129);
  // Just return SAFE if the database is not supported.
  bool synchronous_finish =
      database_manager_->CheckUrlForSubresourceFilter(url, this);
  if (synchronous_finish) {
    request_completed_ = true;
    SendCheckResultToClient(false /* served_from_network */,
                            safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE,
                            safe_browsing::ThreatMetadata());
    return;
  }
  timer_.Start(
      FROM_HERE, kCheckURLTimeout,
      base::BindOnce(
          &SubresourceFilterSafeBrowsingClientRequest::OnCheckUrlTimeout,
          base::Unretained(this)));
}

void SubresourceFilterSafeBrowsingClientRequest::OnCheckBrowseUrlResult(
    const GURL& url,
    safe_browsing::SBThreatType threat_type,
    const safe_browsing::ThreatMetadata& metadata) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M129);
  request_completed_ = true;
  SendCheckResultToClient(true /* served_from_network */, threat_type,
                          metadata);
}

void SubresourceFilterSafeBrowsingClientRequest::OnCheckUrlTimeout() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M129);
  SendCheckResultToClient(true /* served_from_network */,
                          safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE,
                          safe_browsing::ThreatMetadata());
}

void SubresourceFilterSafeBrowsingClientRequest::SendCheckResultToClient(
    bool served_from_network,
    safe_browsing::SBThreatType threat_type,
    const safe_browsing::ThreatMetadata& metadata) {
  SubresourceFilterSafeBrowsingClient::CheckResult result;
  result.request_id = request_id_;
  result.threat_type = threat_type;
  result.threat_metadata = metadata;
  result.start_time = start_time_;

  // This memeber is separate from |request_completed_|, in that it just
  // indicates that this request is done processing (due to completion or
  // timeout).
  result.finished = true;

  // Will delete |this|.
  client_->OnCheckBrowseUrlResult(this, result);
}

}  // namespace subresource_filter
