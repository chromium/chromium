// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_channel_status_request.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/gcm_driver/gcm_backoff_policy.h"
#include "components/sync/protocol/experiment_status.pb.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace gcm {

namespace {

const char kRequestContentType[] = "application/octet-stream";
const char kGCMChannelTag[] = "gcm_channel";
const int kDefaultPollIntervalSeconds = 60 * 60;  // 60 minutes.
const int kMinPollIntervalSeconds = 30 * 60;  // 30 minutes.

}  // namespace

GCMChannelStatusRequest::GCMChannelStatusRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& channel_status_request_url,
    const std::string& user_agent,
    const GCMChannelStatusRequestCallback& callback)
    : url_loader_factory_(url_loader_factory),
      channel_status_request_url_(channel_status_request_url),
      user_agent_(user_agent),
      callback_(callback),
      backoff_entry_(&(GetGCMBackoffPolicy())) {}

GCMChannelStatusRequest::~GCMChannelStatusRequest() {
}

// static
int GCMChannelStatusRequest::default_poll_interval_seconds() {
  return kDefaultPollIntervalSeconds;
}

// static
int GCMChannelStatusRequest::min_poll_interval_seconds() {
  return kMinPollIntervalSeconds;
}

void GCMChannelStatusRequest::Start() {
  // url_loader_factory_ can be null for tests.
  if (!url_loader_factory_)
    return;

  DCHECK(!simple_url_loader_);

  GURL request_url(channel_status_request_url_);

  sync_pb::ExperimentStatusRequest proto_data;
  proto_data.add_experiment_name(kGCMChannelTag);
  std::string upload_data;
  if (!proto_data.SerializeToString(&upload_data)) {
     NOTREACHED();
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gcm_channel_status_request", R"(
        semantics {
          sender: "GCM Driver"
          description:
            "Google Chrome interacts with Google Cloud Messaging to receive "
            "push messages for various browser features, as well as on behalf "
            "of websites and extensions. The channel status request "
            "periodically confirms with Google servers whether the feature "
            "should be enabled."
          trigger:
            "Periodically when Chrome has established an active Google Cloud "
            "Messaging subscription. The first request will be issued a minute "
            "after the first subscription activates. Subsequent requests will "
            "be issued each hour with a jitter of 15 minutes. Google can "
            "adjust this interval when it deems necessary."
          data:
            "A user agent string containing the Chrome version, channel and "
            "platform will be sent to the server. No user identifier is sent "
            "along with the request."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Support for interacting with Google Cloud Messaging is enabled by "
            "default, and there is no configuration option to completely "
            "disable it. Websites wishing to receive push messages must "
            "acquire express permission from the user for the 'Notification' "
            "permission."
          policy_exception_justification:
            "Not implemented, considered not useful."
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();

  resource_request->url = request_url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                                      user_agent_);
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->AttachStringForUpload(upload_data, kRequestContentType);
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&GCMChannelStatusRequest::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

void GCMChannelStatusRequest::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  if (ParseResponse(std::move(response_body)))
    return;

  RetryWithBackoff(true);
}

bool GCMChannelStatusRequest::ParseResponse(
    std::unique_ptr<std::string> response_body) {
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    int response_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
    if (response_code != net::HTTP_OK) {
      LOG(ERROR) << "GCM channel request failed. HTTP status: "
                 << response_code;
      return false;
    }
  }

  if (!response_body) {
    LOG(ERROR) << "GCM channel request failed.";
    return false;
  }

  // Empty response means to keep the existing values.
  if (response_body->empty()) {
    callback_.Run(false, false, 0);
    return true;
  }

  sync_pb::ExperimentStatusResponse response_proto;
  if (!response_proto.ParseFromString(*response_body)) {
    LOG(ERROR) << "GCM channel response failed to be parsed as proto.";
    return false;
  }

  ParseResponseProto(response_proto);

  return true;
}

void GCMChannelStatusRequest::ParseResponseProto(
    sync_pb::ExperimentStatusResponse response_proto) {
  bool enabled = true;
  if (response_proto.experiment_size() == 1 &&
      response_proto.experiment(0).has_gcm_channel() &&
      response_proto.experiment(0).gcm_channel().has_enabled()) {
    enabled = response_proto.experiment(0).gcm_channel().enabled();
  }

  int poll_interval_seconds;
  if (response_proto.has_poll_interval_seconds())
    poll_interval_seconds = response_proto.poll_interval_seconds();
  else
    poll_interval_seconds = kDefaultPollIntervalSeconds;
  if (poll_interval_seconds < kMinPollIntervalSeconds)
    poll_interval_seconds = kMinPollIntervalSeconds;

  callback_.Run(true, enabled, poll_interval_seconds);
}

void GCMChannelStatusRequest::RetryWithBackoff(bool update_backoff) {
  if (update_backoff) {
    simple_url_loader_.reset();
    backoff_entry_.InformOfRequest(false);
  }

  if (backoff_entry_.ShouldRejectRequest()) {
    DVLOG(1) << "Delaying GCM channel request for "
             << backoff_entry_.GetTimeUntilRelease().InMilliseconds()
             << " ms.";
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GCMChannelStatusRequest::RetryWithBackoff,
                       weak_ptr_factory_.GetWeakPtr(), false),
        backoff_entry_.GetTimeUntilRelease());
    return;
  }

  Start();
}

}  // namespace gcm
