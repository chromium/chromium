// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/uploader.h"

#include <utility>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/supports_user_data.h"
#include "components/domain_reliability/util.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/isolation_info.h"
#include "net/base/net_errors.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

namespace domain_reliability {

namespace {

const char kJsonMimeType[] = "application/json; charset=utf-8";

// Each DR upload is tagged with an instance of this class, which identifies the
// depth of the upload. This is to prevent infinite loops of DR uploads about DR
// uploads, leading to an unbounded number of requests.
// Deeper requests will still generate a report beacon, but they won't trigger a
// separate upload. See DomainReliabilityContext::kMaxUploadDepthToSchedule.
struct UploadDepthData : public base::SupportsUserData::Data {
  explicit UploadDepthData(int depth) : depth(depth) {}

  // Key that identifies this data within SupportsUserData's map of data.
  static const void* const kUserDataKey;

  // This is 0 if the report being uploaded does not contain a beacon about a
  // DR upload request. Otherwise, it is 1 + the depth of the deepest DR upload
  // described in the report.
  int depth;
};

const void* const UploadDepthData::kUserDataKey =
    &UploadDepthData::kUserDataKey;

}  // namespace

class DomainReliabilityUploaderImpl : public DomainReliabilityUploader,
                                      public net::URLRequest::Delegate {
 public:
  DomainReliabilityUploaderImpl(MockableTime* time,
                                net::URLRequestContext* url_request_context)
      : time_(time),
        url_request_context_(url_request_context),
        discard_uploads_(true),
        shutdown_(false),
        discarded_upload_count_(0u) {
    DCHECK(url_request_context_);
  }

  ~DomainReliabilityUploaderImpl() override {
    DCHECK(shutdown_);
  }

  // DomainReliabilityUploader implementation:
  void UploadReport(
      const std::string& report_json,
      int max_upload_depth,
      const GURL& upload_url,
      const net::IsolationInfo& isolation_info,
      DomainReliabilityUploader::UploadCallback callback) override {
    DVLOG(1) << "Uploading report to " << upload_url;
    DVLOG(2) << "Report JSON: " << report_json;

    if (discard_uploads_)
      discarded_upload_count_++;

    if (discard_uploads_ || shutdown_) {
      DVLOG(1) << "Discarding report instead of uploading.";
      UploadResult result;
      result.status = UploadResult::SUCCESS;
      std::move(callback).Run(result);
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
            chrome_policy {
              DomainReliabilityAllowed {
                policy_options {mode: MANDATORY}
                DomainReliabilityAllowed: false
              }
            }
            chrome_policy {
              MetricsReportingEnabled {
                policy_options {mode: MANDATORY}
                MetricsReportingEnabled: false
              }
            }
          })");
    std::unique_ptr<net::URLRequest> request =
        url_request_context_->CreateRequest(
            upload_url, net::RequestPriority::IDLE, this /* delegate */,
            traffic_annotation);
    request->set_method("POST");
    request->set_allow_credentials(false);
    request->SetExtraRequestHeaderByName(net::HttpRequestHeaders::kContentType,
                                         kJsonMimeType, true /* overwrite */);
    CHECK_EQ(isolation_info.request_type(),
             net::IsolationInfo::RequestType::kOther);
    CHECK(isolation_info.site_for_cookies().IsNull());
    request->set_isolation_info(isolation_info);
    // Since this is a POST with an upload body and no identifier, these
    // requests automatically bypass the cache, but for consistency set the
    // load flags such that caching is explicitly disabled. This does mean we
    // also disable the cache if we're redirected and the request becomes a GET,
    // but these shouldn't be redirected.
    request->SetLoadFlags(request->load_flags() | net::LOAD_DISABLE_CACHE);
    std::vector<char> report_data(report_json.begin(), report_json.end());
    auto upload_reader =
        std::make_unique<net::UploadOwnedBytesElementReader>(&report_data);
    request->set_upload(net::ElementsUploadDataStream::CreateWithReader(
        std::move(upload_reader)));
    request->SetUserData(
        UploadDepthData::kUserDataKey,
        std::make_unique<UploadDepthData>(max_upload_depth + 1));

    auto [it, inserted] = uploads_.insert(
        std::make_pair(std::move(request), std::move(callback)));
    DCHECK(inserted);
    it->first->Start();
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

  // net::URLRequest::Delegate implementation:
  void OnResponseStarted(net::URLRequest* request, int net_error) override {
    DCHECK(!shutdown_);

    auto request_it = uploads_.find(request);
    CHECK(request_it != uploads_.end(), base::NotFatalUntil::M130);

    int http_response_code = -1;
    base::TimeDelta retry_after;
    if (net_error == net::OK) {
      http_response_code = request->GetResponseCode();
      std::string retry_after_string;
      if (request->response_headers() &&
          request->response_headers()->EnumerateHeader(nullptr, "Retry-After",
                                                       &retry_after_string)) {
        net::HttpUtil::ParseRetryAfterHeader(retry_after_string, time_->Now(),
                                             &retry_after);
      }
    }

    DVLOG(1) << "Upload finished with net error " << net_error
             << ", response code " << http_response_code << ", retry after "
             << retry_after;

    std::move(request_it->second)
        .Run(GetUploadResultFromResponseDetails(net_error, http_response_code,
                                                retry_after));
    uploads_.erase(request_it);
  }

  // Requests are cancelled in OnResponseStarted() once response headers are
  // read, without reading the body, so this is not needed.
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override {
    NOTREACHED_IN_MIGRATION();
  }

 private:
  raw_ptr<MockableTime> time_;
  raw_ptr<net::URLRequestContext> url_request_context_;
  // Stores each in-flight upload request with the callback to notify its
  // initiating DRContext of its completion.
  using UploadMap = std::map<std::unique_ptr<net::URLRequest>,
                             UploadCallback,
                             base::UniquePtrComparator>;
  UploadMap uploads_;
  bool discard_uploads_;
  bool shutdown_;
  int discarded_upload_count_;
};

DomainReliabilityUploader::DomainReliabilityUploader() = default;
DomainReliabilityUploader::~DomainReliabilityUploader() = default;

// static
std::unique_ptr<DomainReliabilityUploader> DomainReliabilityUploader::Create(
    MockableTime* time,
    net::URLRequestContext* url_request_context) {
  return std::make_unique<DomainReliabilityUploaderImpl>(time,
                                                         url_request_context);
}

// static
int DomainReliabilityUploader::GetURLRequestUploadDepth(
    const net::URLRequest& request) {
  UploadDepthData* data = static_cast<UploadDepthData*>(
      request.GetUserData(UploadDepthData::kUserDataKey));
  return data ? data->depth : 0;
}

}  // namespace domain_reliability
