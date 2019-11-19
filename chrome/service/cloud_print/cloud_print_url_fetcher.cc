// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_url_fetcher.h"

#include <stddef.h>

#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "chrome/common/cloud_print/cloud_print_helpers.h"
#include "chrome/service/cloud_print/cloud_print_service_helpers.h"
#include "chrome/service/cloud_print/cloud_print_token_store.h"
#include "chrome/service/net/service_url_request_context_getter.h"
#include "chrome/service/service_process.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace cloud_print {

namespace {

void ReportRequestTime(CloudPrintURLFetcher::RequestType type,
                       base::TimeDelta time) {
  if (type == CloudPrintURLFetcher::REQUEST_REGISTER) {
    UMA_HISTOGRAM_TIMES("CloudPrint.UrlFetcherRequestTime.Register", time);
  } else if (type == CloudPrintURLFetcher::REQUEST_UPDATE_PRINTER) {
    UMA_HISTOGRAM_TIMES("CloudPrint.UrlFetcherRequestTime.UpdatePrinter", time);
  } else if (type == CloudPrintURLFetcher::REQUEST_DATA) {
    UMA_HISTOGRAM_TIMES("CloudPrint.UrlFetcherRequestTime.DownloadData", time);
  } else {
    UMA_HISTOGRAM_TIMES("CloudPrint.UrlFetcherRequestTime.Other", time);
  }
}

void ReportRetriesCount(CloudPrintURLFetcher::RequestType type,
                        int retries) {
  if (type == CloudPrintURLFetcher::REQUEST_REGISTER) {
    UMA_HISTOGRAM_COUNTS_100("CloudPrint.UrlFetcherRetries.Register", retries);
  } else if (type == CloudPrintURLFetcher::REQUEST_UPDATE_PRINTER) {
    UMA_HISTOGRAM_COUNTS_100("CloudPrint.UrlFetcherRetries.UpdatePrinter",
                             retries);
  } else if (type == CloudPrintURLFetcher::REQUEST_DATA) {
    UMA_HISTOGRAM_COUNTS_100("CloudPrint.UrlFetcherRetries.DownloadData",
                             retries);
  } else {
    UMA_HISTOGRAM_COUNTS_100("CloudPrint.UrlFetcherRetries.Other", retries);
  }
}

void ReportDownloadSize(CloudPrintURLFetcher::RequestType type, size_t size) {
  if (type == CloudPrintURLFetcher::REQUEST_REGISTER) {
    UMA_HISTOGRAM_MEMORY_KB("CloudPrint.UrlFetcherDownloadSize.Register", size);
  } else if (type == CloudPrintURLFetcher::REQUEST_UPDATE_PRINTER) {
    UMA_HISTOGRAM_MEMORY_KB("CloudPrint.UrlFetcherDownloadSize.UpdatePrinter",
                            size);
  } else if (type == CloudPrintURLFetcher::REQUEST_DATA) {
    UMA_HISTOGRAM_MEMORY_KB("CloudPrint.UrlFetcherDownloadSize.DownloadData",
                            size);
  } else {
    UMA_HISTOGRAM_MEMORY_KB("CloudPrint.UrlFetcherDownloadSize.Other", size);
  }
}

void ReportUploadSize(CloudPrintURLFetcher::RequestType type, size_t size) {
  if (type == CloudPrintURLFetcher::REQUEST_REGISTER) {
    UMA_HISTOGRAM_MEMORY_KB("CloudPrint.UrlFetcherUploadSize.Register", size);
  } else if (type == CloudPrintURLFetcher::REQUEST_UPDATE_PRINTER) {
    UMA_HISTOGRAM_MEMORY_KB("CloudPrint.UrlFetcherUploadSize.UpdatePrinter",
                            size);
  } else if (type == CloudPrintURLFetcher::REQUEST_DATA) {
    UMA_HISTOGRAM_MEMORY_KB("CloudPrint.UrlFetcherUploadSize.DownloadData",
                            size);
  } else {
    UMA_HISTOGRAM_MEMORY_KB("CloudPrint.UrlFetcherUploadSize.Other", size);
  }
}

CloudPrintURLFetcherFactory* g_test_factory = nullptr;

}  // namespace

// virtual
CloudPrintURLFetcherFactory::~CloudPrintURLFetcherFactory() {}

// static
CloudPrintURLFetcher* CloudPrintURLFetcher::Create(
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  return g_test_factory ? g_test_factory->CreateCloudPrintURLFetcher()
                        : new CloudPrintURLFetcher(partial_traffic_annotation);
}

// static
void CloudPrintURLFetcher::set_test_factory(
    CloudPrintURLFetcherFactory* factory) {
  g_test_factory = factory;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcher::Delegate::HandleRawResponse(
    const net::URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const std::string& data) {
  return CONTINUE_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcher::Delegate::HandleRawData(
    const net::URLFetcher* source,
    const GURL& url,
    const std::string& data) {
  return CONTINUE_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintURLFetcher::Delegate::HandleJSONData(const net::URLFetcher* source,
                                               const GURL& url,
                                               const base::Value& json_data,
                                               bool succeeded) {
  return CONTINUE_PROCESSING;
}

CloudPrintURLFetcher::CloudPrintURLFetcher(
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
    : delegate_(NULL),
      num_retries_(0),
      type_(REQUEST_MAX),
      partial_traffic_annotation_(partial_traffic_annotation) {}

bool CloudPrintURLFetcher::IsSameRequest(const net::URLFetcher* source) {
  return (request_.get() == source);
}

void CloudPrintURLFetcher::StartGetRequest(
    RequestType type,
    const GURL& url,
    Delegate* delegate,
    int max_retries,
    const std::string& additional_headers) {
  StartRequestHelper(type, url, net::URLFetcher::GET, delegate, max_retries,
                     std::string(), std::string(), additional_headers);
}

void CloudPrintURLFetcher::StartPostRequest(
    RequestType type,
    const GURL& url,
    Delegate* delegate,
    int max_retries,
    const std::string& post_data_mime_type,
    const std::string& post_data,
    const std::string& additional_headers) {
  StartRequestHelper(type, url, net::URLFetcher::POST, delegate, max_retries,
                     post_data_mime_type, post_data, additional_headers);
}

void CloudPrintURLFetcher::OnURLFetchComplete(
    const net::URLFetcher* source) {
  VLOG(1) << "CP_PROXY: OnURLFetchComplete, url: " << source->GetURL()
          << ", response code: " << source->GetResponseCode();
  // Make sure we stay alive through the body of this function.
  scoped_refptr<CloudPrintURLFetcher> keep_alive(this);
  std::string data;
  source->GetResponseAsString(&data);
  ReportRequestTime(type_, base::Time::Now() - start_time_);
  ReportDownloadSize(type_, data.size());
  ResponseAction action = delegate_->HandleRawResponse(
      source,
      source->GetURL(),
      source->GetStatus(),
      source->GetResponseCode(),
      data);

  // If we get auth error, notify delegate and check if it wants to proceed.
  if (action == CONTINUE_PROCESSING &&
      source->GetResponseCode() == net::HTTP_FORBIDDEN) {
    action = delegate_->OnRequestAuthError();
  }

  if (action == CONTINUE_PROCESSING) {
    // We need to retry on all network errors.
    if (!source->GetStatus().is_success() || (source->GetResponseCode() != 200))
      action = RETRY_REQUEST;
    else
      action = delegate_->HandleRawData(source, source->GetURL(), data);

    if (action == CONTINUE_PROCESSING) {
      // If the delegate is not interested in handling the raw response data,
      // we assume that a JSON response is expected. If we do not get a JSON
      // response, we will retry (to handle the case where we got redirected
      // to a non-cloudprint-server URL eg. for authentication).
      bool succeeded = false;
      base::Value response_dict = ParseResponseJSON(data, &succeeded);

      if (response_dict.is_dict()) {
        action = delegate_->HandleJSONData(source, source->GetURL(),
                                           response_dict, succeeded);
      } else {
        action = RETRY_REQUEST;
      }
    }
  }
  // Retry the request if needed.
  if (action == RETRY_REQUEST) {
    // Explicitly call ReceivedContentWasMalformed() to ensure the current
    // request gets counted as a failure for calculation of the back-off
    // period.  If it was already a failure by status code, this call will
    // be ignored.
    request_->ReceivedContentWasMalformed();

    // If we receive error code from the server "Media Type Not Supported",
    // there is no reason to retry, request will never succeed.
    // In that case we should call OnRequestGiveUp() right away.
    if (source->GetResponseCode() == net::HTTP_UNSUPPORTED_MEDIA_TYPE)
      num_retries_ = source->GetMaxRetriesOn5xx();

    ++num_retries_;
    if (source->GetMaxRetriesOn5xx() != -1 &&
        num_retries_ > source->GetMaxRetriesOn5xx()) {
      // Retry limit reached. Give up.
      delegate_->OnRequestGiveUp();
      action = STOP_PROCESSING;
    } else {
      // Either no retry limit specified or retry limit has not yet been
      // reached. Try again. Set up the request headers again because the token
      // may have changed.
      SetupRequestHeaders();
      request_->SetRequestContext(GetRequestContextGetter());
      start_time_ = base::Time::Now();
      request_->Start();
    }
  }
  if (action != RETRY_REQUEST) {
    ReportRetriesCount(type_, num_retries_);
  }
}

void CloudPrintURLFetcher::StartRequestHelper(
    RequestType type,
    const GURL& url,
    net::URLFetcher::RequestType request_type,
    Delegate* delegate,
    int max_retries,
    const std::string& post_data_mime_type,
    const std::string& post_data,
    const std::string& additional_headers) {
  DCHECK(delegate);
  type_ = type;
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.UrlFetcherRequestType", type,
                            REQUEST_MAX);
  // Persist the additional headers in case we need to retry the request.
  additional_headers_ = additional_headers;
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::CompleteNetworkTrafficAnnotation("cloud_print",
                                            partial_traffic_annotation_,
                                            R"(
          semantics {
            sender: "Cloud Print"
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "This feature cannot be disabled by settings."
            chrome_policy {
              CloudPrintProxyEnabled {
                policy_options {mode: MANDATORY}
                CloudPrintProxyEnabled: false
              }
            }
          })");
  request_ =
      net::URLFetcher::Create(0, url, request_type, this, traffic_annotation);
  request_->SetRequestContext(GetRequestContextGetter());
  // Since we implement our own retry logic, disable the retry in URLFetcher.
  request_->SetAutomaticallyRetryOn5xx(false);
  request_->SetMaxRetriesOn5xx(max_retries);
  delegate_ = delegate;
  SetupRequestHeaders();
  request_->SetAllowCredentials(false);
  if (request_type == net::URLFetcher::POST) {
    request_->SetUploadData(post_data_mime_type, post_data);
    ReportUploadSize(type_, post_data.size());
  }
  start_time_ = base::Time::Now();
  request_->Start();
}

void CloudPrintURLFetcher::SetupRequestHeaders() {
  std::string headers = delegate_->GetAuthHeader();
  if (!headers.empty())
    headers += "\r\n";
  headers += kChromeCloudPrintProxyHeader;
  if (!additional_headers_.empty()) {
    headers += "\r\n";
    headers += additional_headers_;
  }
  request_->SetExtraRequestHeaders(headers);
}

CloudPrintURLFetcher::~CloudPrintURLFetcher() {}

net::URLRequestContextGetter* CloudPrintURLFetcher::GetRequestContextGetter() {
  ServiceURLRequestContextGetter* getter =
      g_service_process->GetServiceURLRequestContextGetter();
  // Now set up the user agent for cloudprint.
  std::string user_agent = getter->user_agent();
  base::StringAppendF(&user_agent, " %s", kCloudPrintUserAgent);
  getter->set_user_agent(user_agent);
  return getter;
}

}  // namespace cloud_print
