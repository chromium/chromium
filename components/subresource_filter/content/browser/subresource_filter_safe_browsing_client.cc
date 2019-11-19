// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_activation_throttle.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client_request.h"
#include "content/public/browser/browser_thread.h"

namespace subresource_filter {

std::unique_ptr<base::trace_event::TracedValue>
SubresourceFilterSafeBrowsingClient::CheckResult::ToTracedValue() const {
  auto value = std::make_unique<base::trace_event::TracedValue>();
  value->SetInteger("request_id", request_id);
  value->SetInteger("threat_type", threat_type);
  value->SetValue("threat_metadata", threat_metadata.ToTracedValue().get());
  value->SetInteger("duration (us)",
                    (base::TimeTicks::Now() - start_time).InMicroseconds());
  value->SetBoolean("finished", finished);
  return value;
}

SubresourceFilterSafeBrowsingClient::SubresourceFilterSafeBrowsingClient(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    base::WeakPtr<SubresourceFilterSafeBrowsingActivationThrottle> throttle,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> throttle_task_runner)
    : database_manager_(std::move(database_manager)),
      throttle_(std::move(throttle)),
      io_task_runner_(std::move(io_task_runner)),
      throttle_task_runner_(std::move(throttle_task_runner)) {
  DCHECK(database_manager_);
}

SubresourceFilterSafeBrowsingClient::~SubresourceFilterSafeBrowsingClient() {}

void SubresourceFilterSafeBrowsingClient::CheckUrlOnIO(
    const GURL& url,
    size_t request_id,
    base::TimeTicks start_time) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!url.is_empty());

  auto request = std::make_unique<SubresourceFilterSafeBrowsingClientRequest>(
      request_id, start_time, database_manager_, io_task_runner_, this);
  auto* raw_request = request.get();
  DCHECK(requests_.find(raw_request) == requests_.end());
  requests_[raw_request] = std::move(request);
  TRACE_EVENT_ASYNC_BEGIN1(TRACE_DISABLED_BY_DEFAULT("loading"),
                           "SubresourceFilterSBCheck", raw_request,
                           "check_result",
                           std::make_unique<base::trace_event::TracedValue>());
  raw_request->Start(url);
  // Careful, |raw_request| can be destroyed after this line.
}

void SubresourceFilterSafeBrowsingClient::OnCheckBrowseUrlResult(
    SubresourceFilterSafeBrowsingClientRequest* request,
    const CheckResult& check_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  TRACE_EVENT_ASYNC_END1(TRACE_DISABLED_BY_DEFAULT("loading"),
                         "SubresourceFilterSBCheck", request, "check_result",
                         check_result.ToTracedValue());
  throttle_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SubresourceFilterSafeBrowsingActivationThrottle::
                         OnCheckUrlResultOnUI,
                     throttle_, check_result));

  DCHECK(requests_.find(request) != requests_.end());
  requests_.erase(request);
}

}  // namespace subresource_filter
