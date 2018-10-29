// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the ThreatDetails class.

#include "components/safe_browsing/browser/threat_details.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/md5.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/safe_browsing/browser/threat_details_cache.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

using content::BrowserThread;

// Only send small files for now, a better strategy would use the size
// of the whole report and the user's bandwidth.
static const uint32_t kMaxBodySizeBytes = 1024;

namespace safe_browsing {

ThreatDetailsCacheCollector::ThreatDetailsCacheCollector()
    : resources_(nullptr), result_(nullptr), has_started_(false) {}

void ThreatDetailsCacheCollector::StartCacheCollection(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    ResourceMap* resources,
    bool* result,
    const base::Closure& callback) {
  // Start the data collection from the HTTP cache. We use a URLFetcher
  // and set the right flags so we only hit the cache.
  DVLOG(1) << "Getting cache data for all urls...";
  url_loader_factory_ = url_loader_factory;
  resources_ = resources;
  resources_it_ = resources_->begin();
  result_ = result;
  callback_ = callback;
  has_started_ = true;

  // Post a task in the message loop, so the callers don't need to
  // check if we call their callback immediately.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&ThreatDetailsCacheCollector::OpenEntry, this));
}

bool ThreatDetailsCacheCollector::HasStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return has_started_;
}

ThreatDetailsCacheCollector::~ThreatDetailsCacheCollector() {}

// Fetch a URL and advance to the next one when done.
void ThreatDetailsCacheCollector::OpenEntry() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(1) << "OpenEntry";

  if (resources_it_ == resources_->end()) {
    AllDone(true);
    return;
  }

  if (!url_loader_factory_) {
    DVLOG(1) << "Missing URLLoaderFactory";
    AllDone(false);
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("safe_browsing_cache_collector", R"(
        semantics {
          sender: "Threat Details Cache Collector"
          description:
            "This request fetches different items from safe browsing cache "
            "and DOES NOT make an actual network request."
          trigger:
            "When safe browsing extended report is collecting data."
          data:
            "None"
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable this feature by stopping sending "
            "security incident reports to Google via disabling 'Automatically "
            "report details of possible security incidents to Google.' in "
            "Chrome's settings under Advanced Settings, Privacy. The feature "
            "is disabled by default."
          chrome_policy {
            SafeBrowsingExtendedReportingOptInAllowed {
              policy_options {mode: MANDATORY}
              SafeBrowsingExtendedReportingOptInAllowed: false
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(resources_it_->first);
  // Only from cache, and don't save cookies.
  resource_request->load_flags = net::LOAD_ONLY_FROM_CACHE |
                                 net::LOAD_SKIP_CACHE_VALIDATION |
                                 net::LOAD_DO_NOT_SAVE_COOKIES;
  current_load_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                   traffic_annotation);
  current_load_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ThreatDetailsCacheCollector::OnURLLoaderComplete,
                     base::Unretained(this)));
}

ClientSafeBrowsingReportRequest::Resource*
ThreatDetailsCacheCollector::GetResource(const GURL& url) {
  auto it = resources_->find(url.spec());
  if (it != resources_->end()) {
    return it->second.get();
  }
  return nullptr;
}

void ThreatDetailsCacheCollector::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  DVLOG(1) << "OnURLLoaderComplete";
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(current_load_);
  if (current_load_->NetError() == net::ERR_CACHE_MISS) {
    // Cache miss, skip this resource.
    DVLOG(1) << "Cache miss for url: " << current_load_->GetFinalURL();
    AdvanceEntry();
    return;
  }

  if (current_load_->NetError() != net::OK) {
    // Some other error occurred, e.g. the request could have been cancelled.
    DVLOG(1) << "Unsuccessful fetch: " << current_load_->GetFinalURL();
    AdvanceEntry();
    return;
  }

  // Set the response headers and body to the right resource, which
  // might not be the same as the one we asked for.
  // For redirects, resources_it_->first != url.spec().
  ClientSafeBrowsingReportRequest::Resource* resource =
      GetResource(current_load_->GetFinalURL());
  if (!resource) {
    DVLOG(1) << "Cannot find resource for url:" << current_load_->GetFinalURL();
    AdvanceEntry();
    return;
  }

  ReadResponse(resource);
  std::string data;
  if (response_body)
    data = *response_body;
  ReadData(resource, data);
  AdvanceEntry();
}

void ThreatDetailsCacheCollector::ReadResponse(
    ClientSafeBrowsingReportRequest::Resource* pb_resource) {
  DVLOG(1) << "ReadResponse";
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!current_load_->ResponseInfo() ||
      !current_load_->ResponseInfo()->headers) {
    DVLOG(1) << "Missing response headers.";
    return;
  }
  net::HttpResponseHeaders* headers =
      current_load_->ResponseInfo()->headers.get();

  ClientSafeBrowsingReportRequest::HTTPResponse* pb_response =
      pb_resource->mutable_response();
  pb_response->mutable_firstline()->set_code(headers->response_code());
  size_t iter = 0;
  std::string name, value;
  while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
    // Strip any Set-Cookie headers.
    if (base::LowerCaseEqualsASCII(name, "set-cookie"))
      continue;
    ClientSafeBrowsingReportRequest::HTTPHeader* pb_header =
        pb_response->add_headers();
    pb_header->set_name(name);
    pb_header->set_value(value);
  }

  if (!current_load_->ResponseInfo()->was_fetched_via_proxy) {
    pb_response->set_remote_ip(
        current_load_->ResponseInfo()->socket_address.ToString());
  }
}

void ThreatDetailsCacheCollector::ReadData(
    ClientSafeBrowsingReportRequest::Resource* pb_resource,
    const std::string& data) {
  DVLOG(1) << "ReadData";
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ClientSafeBrowsingReportRequest::HTTPResponse* pb_response =
      pb_resource->mutable_response();
  if (data.size() <= kMaxBodySizeBytes) {  // Only send small bodies for now.
    pb_response->set_body(data);
  }
  pb_response->set_bodylength(data.size());
  pb_response->set_bodydigest(base::MD5String(data));
}

void ThreatDetailsCacheCollector::AdvanceEntry() {
  DVLOG(1) << "AdvanceEntry";
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Advance to the next resource.
  ++resources_it_;
  current_load_.reset();

  // Create a task so we don't take over the UI thread for too long.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&ThreatDetailsCacheCollector::OpenEntry, this));
}

void ThreatDetailsCacheCollector::AllDone(bool success) {
  DVLOG(1) << "AllDone";
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  *result_ = success;
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, callback_);
  callback_.Reset();
}

}  // namespace safe_browsing
