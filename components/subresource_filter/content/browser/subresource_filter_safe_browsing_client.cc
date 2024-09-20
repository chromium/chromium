// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/not_fatal_until.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/subresource_filter/content/browser/safe_browsing_page_activation_throttle.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client_request.h"
#include "content/public/browser/browser_thread.h"

namespace subresource_filter {

std::unique_ptr<base::trace_event::TracedValue>
SubresourceFilterSafeBrowsingClient::CheckResult::ToTracedValue() const {
  auto value = std::make_unique<base::trace_event::TracedValue>();
  value->SetInteger("request_id", request_id);
  value->SetInteger("threat_type", static_cast<int>(threat_type));
  value->SetValue("threat_metadata", threat_metadata.ToTracedValue().get());
  value->SetInteger("duration (us)",
                    (base::TimeTicks::Now() - start_time).InMicroseconds());
  value->SetBoolean("finished", finished);
  return value;
}

SubresourceFilterSafeBrowsingClient::SubresourceFilterSafeBrowsingClient(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    SafeBrowsingPageActivationThrottle* throttle,
    scoped_refptr<base::SingleThreadTaskRunner> throttle_task_runner)
    : database_manager_(std::move(database_manager)),
      throttle_(throttle),
      throttle_task_runner_(std::move(throttle_task_runner)) {
  CHECK(database_manager_, base::NotFatalUntil::M129);
}

SubresourceFilterSafeBrowsingClient::~SubresourceFilterSafeBrowsingClient() =
    default;

void SubresourceFilterSafeBrowsingClient::CheckUrl(const GURL& url,
                                                   size_t request_id,
                                                   base::TimeTicks start_time) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M129);
  CHECK(!url.is_empty(), base::NotFatalUntil::M129);

  auto request = std::make_unique<SubresourceFilterSafeBrowsingClientRequest>(
      request_id, start_time, database_manager_, this);
  auto* raw_request = request.get();
  CHECK(requests_.find(raw_request) == requests_.end(),
        base::NotFatalUntil::M129);
  requests_[raw_request] = std::move(request);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      TRACE_DISABLED_BY_DEFAULT("loading"), "SubresourceFilterSBCheck",
      TRACE_ID_LOCAL(raw_request), "check_result",
      std::make_unique<base::trace_event::TracedValue>());
  raw_request->Start(url);
  // Careful, |raw_request| can be destroyed after this line.
}

void SubresourceFilterSafeBrowsingClient::OnCheckBrowseUrlResult(
    SubresourceFilterSafeBrowsingClientRequest* request,
    const CheckResult& check_result) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M129);
  TRACE_EVENT_NESTABLE_ASYNC_END1(
      TRACE_DISABLED_BY_DEFAULT("loading"), "SubresourceFilterSBCheck",
      TRACE_ID_LOCAL(request), "check_result", check_result.ToTracedValue());
  CHECK(requests_.find(request) != requests_.end(), base::NotFatalUntil::M129);
  requests_.erase(request);
  if (throttle_) {
    throttle_->OnCheckUrlResultOnUI(check_result);
    // `this` may be deleted now. The only safe thing to do is return.
    return;
  }
}

}  // namespace subresource_filter
