// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_stopped_reporter.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "components/sync/protocol/sync.pb.h"
#include "google_apis/credentials_mode.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace syncer {

namespace {

const char kEventEndpoint[] = "event";

// The request is tiny, so even on poor connections 10 seconds should be
// plenty of time. Since sync is off when this request is started, we don't
// want anything sync-related hanging around for very long from a human
// perspective either. This seems like a good compromise.
constexpr base::TimeDelta kRequestTimeout = base::Seconds(10);

void LogSyncStoppedRequestTimeout(bool timed_out) {
  base::UmaHistogramBoolean("Sync.SyncStoppedURLFetchTimedOut", timed_out);
}

void LogSyncStoppedRequestResult(const network::SimpleURLLoader& url_loader) {
  int http_status_code = -1;
  if (url_loader.ResponseInfo() && url_loader.ResponseInfo()->headers) {
    http_status_code = url_loader.ResponseInfo()->headers->response_code();
  }
  const int net_error_code = url_loader.NetError();
  const bool request_succeeded =
      net_error_code == net::OK && http_status_code != -1;
  if (request_succeeded) {
    LogSyncStoppedRequestTimeout(/*timed_out=*/false);
  }
  base::UmaHistogramSparse(
      "Sync.SyncStoppedURLFetchResponse",
      request_succeeded ? http_status_code : net_error_code);
}

}  // namespace

SyncStoppedReporter::SyncStoppedReporter(
    const GURL& sync_service_url,
    const std::string& user_agent,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : sync_event_url_(GetSyncEventURL(sync_service_url)),
      user_agent_(user_agent),
      url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK(!sync_service_url.is_empty());
  DCHECK(!user_agent_.empty());
  DCHECK(url_loader_factory_);
}

SyncStoppedReporter::~SyncStoppedReporter() = default;

void SyncStoppedReporter::ReportSyncStopped(const std::string& access_token,
                                            const std::string& cache_guid,
                                            const std::string& birthday) {
  DCHECK(!access_token.empty());
  DCHECK(!cache_guid.empty());
  DCHECK(!birthday.empty());

  // Make the request proto with the GUID identifying this client.
  sync_pb::EventRequest event_request;
  sync_pb::SyncDisabledEvent* sync_disabled_event =
      event_request.mutable_sync_disabled();
  sync_disabled_event->set_cache_guid(cache_guid);
  sync_disabled_event->set_store_birthday(birthday);

  std::string msg;
  event_request.SerializeToString(&msg);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("sync_stop_reporter", R"(
        semantics {
          sender: "Chrome Sync"
          description:
            "A network request to inform Chrome Sync that sync has been "
            "disabled for this device."
          trigger: "User disables sync."
          data: "Sync device identifier and metadata."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          chrome_policy {
            SyncDisabled {
              policy_options {mode: MANDATORY}
              SyncDisabled: true
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = sync_event_url_;
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode =
      google_apis::GetOmitCredentialsModeForGaiaRequests();
  resource_request->method = "POST";
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf("Bearer %s", access_token.c_str()));
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                                      user_agent_);
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->AttachStringForUpload(msg, "application/octet-stream");
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&SyncStoppedReporter::OnSimpleLoaderComplete,
                     base::Unretained(this)));
  timer_.Start(FROM_HERE, kRequestTimeout, this,
               &SyncStoppedReporter::OnTimeout);
}

void SyncStoppedReporter::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK(simple_url_loader_);
  LogSyncStoppedRequestResult(*simple_url_loader_);
  simple_url_loader_.reset();
  timer_.Stop();
}

void SyncStoppedReporter::OnTimeout() {
  LogSyncStoppedRequestTimeout(/*timed_out=*/true);
  simple_url_loader_.reset();
}

// Static.
GURL SyncStoppedReporter::GetSyncEventURL(const GURL& sync_service_url) {
  std::string path = sync_service_url.path();
  if (path.empty() || *path.rbegin() != '/') {
    path += '/';
  }
  path += kEventEndpoint;
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return sync_service_url.ReplaceComponents(replacements);
}

}  // namespace syncer
