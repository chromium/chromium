// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/uploader.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/supports_user_data.h"
#include "components/domain_reliability/util.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

namespace domain_reliability {

namespace {

const char kJsonMimeType[] = "application/json; charset=utf-8";

class UploadUserData : public base::SupportsUserData::Data {
 public:
  static net::URLFetcher::CreateDataCallback CreateCreateDataCallback(
      int depth) {
    return base::Bind(&UploadUserData::CreateUploadUserData, depth);
  }

  static const void* const kUserDataKey;

  int depth() const { return depth_; }

 private:
  UploadUserData(int depth) : depth_(depth) {}

  static std::unique_ptr<base::SupportsUserData::Data> CreateUploadUserData(
      int depth) {
    return base::WrapUnique(new UploadUserData(depth));
  }

  int depth_;
};

const void* const UploadUserData::kUserDataKey =
    &UploadUserData::kUserDataKey;

class DomainReliabilityUploaderImpl
    : public DomainReliabilityUploader, net::URLFetcherDelegate {
 public:
  DomainReliabilityUploaderImpl(
      MockableTime* time,
      const scoped_refptr<net::URLRequestContextGetter>&
          url_request_context_getter)
      : time_(time),
        url_request_context_getter_(url_request_context_getter),
        discard_uploads_(true),
        shutdown_(false),
        discarded_upload_count_(0u) {}

  ~DomainReliabilityUploaderImpl() override {
    DCHECK(shutdown_);
  }

  // DomainReliabilityUploader implementation:
  void UploadReport(
      const std::string& report_json,
      int max_upload_depth,
      const GURL& upload_url,
      const DomainReliabilityUploader::UploadCallback& callback) override {
    DVLOG(1) << "Uploading report to " << upload_url;
    DVLOG(2) << "Report JSON: " << report_json;

    if (discard_uploads_)
      discarded_upload_count_++;

    if (discard_uploads_ || shutdown_) {
      DVLOG(1) << "Discarding report instead of uploading.";
      UploadResult result;
      result.status = UploadResult::SUCCESS;
      callback.Run(result);
      return;
    }

    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("domain_reliability_report_upload",
                                            R"(
          semantics {
            sender: "Domain Reliability"
            description:
              "If Chromium has trouble reaching certain Google sites or "
              "services, Domain Reliability may report the problems back to "
              "Google."
            trigger: "Failure to load certain Google sites or services."
            data:
              "Details of the failed request, including the URL, any IP "
              "addresses the browser tried to connect to, error(s) "
              "encountered loading the resource, and other connection details."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "Users can enable or disable Domain Reliability on desktop, via "
              "toggling 'Automatically send usage statistics and crash reports "
              "to Google' in Chromium's settings under Privacy. On ChromeOS, "
              "the setting is named 'Automatically send diagnostic and usage "
              "data to Google'."
            policy_exception_justification: "Not implemented."
          })");
    std::unique_ptr<net::URLFetcher> owned_fetcher = net::URLFetcher::Create(
        0, upload_url, net::URLFetcher::POST, this, traffic_annotation);
    net::URLFetcher* fetcher = owned_fetcher.get();
    fetcher->SetRequestContext(url_request_context_getter_.get());
    fetcher->SetAllowCredentials(false);
    fetcher->SetUploadData(kJsonMimeType, report_json);
    fetcher->SetAutomaticallyRetryOn5xx(false);
    fetcher->SetURLRequestUserData(
        UploadUserData::kUserDataKey,
        UploadUserData::CreateCreateDataCallback(max_upload_depth + 1));
    fetcher->Start();

    uploads_[fetcher] = {std::move(owned_fetcher), callback};
  }

  void SetDiscardUploads(bool discard_uploads) override {
    discard_uploads_ = discard_uploads;
    DVLOG(1) << "Setting discard_uploads to " << discard_uploads;
  }

  void Shutdown() override {
    DCHECK(!shutdown_);
    shutdown_ = true;
    uploads_.clear();
  }

  int GetDiscardedUploadCount() const override {
    return discarded_upload_count_;
  }

  // net::URLFetcherDelegate implementation:
  void OnURLFetchComplete(const net::URLFetcher* fetcher) override {
    DCHECK(fetcher);

    auto callback_it = uploads_.find(fetcher);
    DCHECK(callback_it != uploads_.end());

    int net_error = GetNetErrorFromURLRequestStatus(fetcher->GetStatus());
    int http_response_code = fetcher->GetResponseCode();
    base::TimeDelta retry_after;
    {
      std::string retry_after_string;
      if (fetcher->GetResponseHeaders() &&
          fetcher->GetResponseHeaders()->EnumerateHeader(nullptr,
                                                         "Retry-After",
                                                         &retry_after_string)) {
        net::HttpUtil::ParseRetryAfterHeader(retry_after_string,
                                             time_->Now(),
                                             &retry_after);
      }
    }

    DVLOG(1) << "Upload finished with net error " << net_error
             << ", response code " << http_response_code << ", retry after "
             << retry_after;

    base::UmaHistogramSparse("DomainReliability.UploadResponseCode",
                             http_response_code);
    base::UmaHistogramSparse("DomainReliability.UploadNetError", -net_error);

    UploadResult result;
    GetUploadResultFromResponseDetails(net_error,
                                       http_response_code,
                                       retry_after,
                                       &result);
    callback_it->second.second.Run(result);

    uploads_.erase(callback_it);
  }

 private:
  using DomainReliabilityUploader::UploadCallback;

  MockableTime* time_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  std::map<const net::URLFetcher*,
           std::pair<std::unique_ptr<net::URLFetcher>, UploadCallback>>
      uploads_;
  bool discard_uploads_;
  bool shutdown_;
  int discarded_upload_count_;
};

}  // namespace

DomainReliabilityUploader::DomainReliabilityUploader() {}
DomainReliabilityUploader::~DomainReliabilityUploader() {}

// static
std::unique_ptr<DomainReliabilityUploader> DomainReliabilityUploader::Create(
    MockableTime* time,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter) {
  return std::unique_ptr<DomainReliabilityUploader>(
      new DomainReliabilityUploaderImpl(time, url_request_context_getter));
}

// static
int DomainReliabilityUploader::GetURLRequestUploadDepth(
    const net::URLRequest& request) {
  UploadUserData* data = static_cast<UploadUserData*>(
      request.GetUserData(UploadUserData::kUserDataKey));
  if (!data)
    return 0;
  return data->depth();
}

void DomainReliabilityUploader::Shutdown() {}

}  // namespace domain_reliability
