// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/net_metrics_log_uploader.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/encrypted_messages/encrypted_message.pb.h"
#include "components/encrypted_messages/message_encrypter.h"
#include "components/metrics/metrics_log_uploader.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/metrics_proto/reporting_info.pb.h"
#include "url/gurl.h"

namespace {

const base::Feature kHttpRetryFeature{"UMAHttpRetry",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Run ablation on UMA collector connectivity to client. This study will
// ablate a clients upload of all logs that use |metrics::ReportingService|
// to upload logs. This include |metrics::MetricsReportingService| for uploading
// UMA logs. |ukm::UKMReportionService| for uploading UKM logs.
// Rappor service use |rappor::LogUploader| which is not a
// |metrics::ReportingService| so, it won't be ablated.
// similar frequency.
// To restrict the study to UMA or UKM, set the "service-affected" param.
const base::Feature kAblateMetricsLogUploadFeature{
    "AblateMetricsLogUpload", base::FEATURE_DISABLED_BY_DEFAULT};

// Fraction of Collector uploads that should be failed artificially.
constexpr base::FeatureParam<int> kParamFailureRate{
    &kAblateMetricsLogUploadFeature, "failure-rate", 100};

// HTTP Error code to pass when artificially failing uploads.
constexpr base::FeatureParam<int> kParamErrorCode{
    &kAblateMetricsLogUploadFeature, "error-code", 503};

// Service type to ablate. Can be "UMA" or "UKM". Leave it empty to ablate all.
constexpr base::FeatureParam<std::string> kParamAblateServiceType{
    &kAblateMetricsLogUploadFeature, "service-type", ""};

// Constants used for encrypting logs that are sent over HTTP. The
// corresponding private key is used by the metrics server to decrypt logs.
const char kEncryptedMessageLabel[] = "metrics log";

const uint8_t kServerPublicKey[] = {
    0x51, 0xcc, 0x52, 0x67, 0x42, 0x47, 0x3b, 0x10, 0xe8, 0x63, 0x18,
    0x3c, 0x61, 0xa7, 0x96, 0x76, 0x86, 0x91, 0x40, 0x71, 0x39, 0x5f,
    0x31, 0x1a, 0x39, 0x5b, 0x76, 0xb1, 0x6b, 0x3d, 0x6a, 0x2b};

const uint32_t kServerPublicKeyVersion = 1;

net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotation(
    const metrics::MetricsLogUploader::MetricServiceType& service_type) {
  // The code in this function should remain so that we won't need a default
  // case that does not have meaningful annotation.
  if (service_type == metrics::MetricsLogUploader::UMA) {
    return net::DefineNetworkTrafficAnnotation("metrics_report_uma", R"(
        semantics {
          sender: "Metrics UMA Log Uploader"
          description:
            "Report of usage statistics and crash-related data about Chromium. "
            "Usage statistics contain information such as preferences, button "
            "clicks, and memory usage and do not include web page URLs or "
            "personal information. See more at "
            "https://www.google.com/chrome/browser/privacy/ under 'Usage "
            "statistics and crash reports'. Usage statistics are tied to a "
            "pseudonymous machine identifier and not to your email address."
          trigger:
            "Reports are automatically generated on startup and at intervals "
            "while Chromium is running."
          data:
            "A protocol buffer with usage statistics and crash related data."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable this feature by disabling "
            "'Automatically send usage statistics and crash reports to Google' "
            "in Chromium's settings under Advanced Settings, Privacy. The "
            "feature is enabled by default."
          chrome_policy {
            MetricsReportingEnabled {
              policy_options {mode: MANDATORY}
              MetricsReportingEnabled: false
            }
          }
        })");
  }
  DCHECK_EQ(service_type, metrics::MetricsLogUploader::UKM);
  return net::DefineNetworkTrafficAnnotation("metrics_report_ukm", R"(
      semantics {
        sender: "Metrics UKM Log Uploader"
        description:
          "Report of usage statistics that are keyed by URLs to Chromium. This "
          "includes information about the web pages you visit and your usage "
          "of them, such as page load speed. This will also include URLs and "
          "statistics related to downloaded files. These statistics may also "
          "include information about the extensions that have been installed "
          "from Chrome Web Store. Google only stores usage statistics "
          "associated with published extensions, and URLs that are known by "
          "Google’s search index. Usage statistics are tied to a "
          "pseudonymous machine identifier and not to your email address."
        trigger:
          "Reports are automatically generated on startup and at intervals "
          "while Chromium is running with Sync enabled."
        data:
          "A protocol buffer with usage statistics and associated URLs."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users can enable or disable this feature by disabling 'Make "
          "searches and browsing better' in Chrome's settings under Advanced "
          "Settings, Privacy. This has to be enabled for all active profiles. "
          "This is only enabled if the user has 'Help improve Chrome's "
          "features and performance' enabled in the same settings menu. "
          "Information about the installed extensions is sent only if "
          "Extension Sync is enabled."
        chrome_policy {
          MetricsReportingEnabled {
            policy_options {mode: MANDATORY}
            MetricsReportingEnabled: false
          }
        }
      })");
}

std::string SerializeReportingInfo(
    const metrics::ReportingInfo& reporting_info) {
  std::string result;
  std::string bytes;
  bool success = reporting_info.SerializeToString(&bytes);
  DCHECK(success);
  base::Base64Encode(bytes, &result);
  return result;
}

// Encrypts a |plaintext| string, using the encrypted_messages component,
// returns |encrypted| which is a serialized EncryptedMessage object. Returns
// false if there was a problem encrypting.
bool EncryptString(const std::string& plaintext, std::string* encrypted) {
  encrypted_messages::EncryptedMessage encrypted_message;
  if (!encrypted_messages::EncryptSerializedMessage(
          kServerPublicKey, kServerPublicKeyVersion, kEncryptedMessageLabel,
          plaintext, &encrypted_message)) {
    NOTREACHED() << "Error encrypting string.";
    return false;
  }
  if (!encrypted_message.SerializeToString(encrypted)) {
    NOTREACHED() << "Error serializing encrypted string.";
    return false;
  }
  return true;
}

// Encrypts a |plaintext| string and returns |encoded|, which is a base64
// encoded serialized EncryptedMessage object. Returns false if there was a
// problem encrypting or serializing.
bool EncryptAndBase64EncodeString(const std::string& plaintext,
                                  std::string* encoded) {
  std::string encrypted_text;
  if (!EncryptString(plaintext, &encrypted_text))
    return false;

  base::Base64Encode(encrypted_text, encoded);
  return true;
}

}  // namespace

namespace metrics {

NetMetricsLogUploader::NetMetricsLogUploader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& server_url,
    base::StringPiece mime_type,
    MetricsLogUploader::MetricServiceType service_type,
    const MetricsLogUploader::UploadCallback& on_upload_complete)
    : url_loader_factory_(std::move(url_loader_factory)),
      server_url_(server_url),
      mime_type_(mime_type.data(), mime_type.size()),
      service_type_(service_type),
      on_upload_complete_(on_upload_complete) {}

NetMetricsLogUploader::NetMetricsLogUploader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& server_url,
    const GURL& insecure_server_url,
    base::StringPiece mime_type,
    MetricsLogUploader::MetricServiceType service_type,
    const MetricsLogUploader::UploadCallback& on_upload_complete)
    : url_loader_factory_(std::move(url_loader_factory)),
      server_url_(server_url),
      insecure_server_url_(insecure_server_url),
      mime_type_(mime_type.data(), mime_type.size()),
      service_type_(service_type),
      on_upload_complete_(on_upload_complete) {}

NetMetricsLogUploader::~NetMetricsLogUploader() {
}

void NetMetricsLogUploader::UploadLog(const std::string& compressed_log_data,
                                      const std::string& log_hash,
                                      const std::string& log_signature,
                                      const ReportingInfo& reporting_info) {
  // If this attempt is a retry, there was a network error, the last attempt was
  // over https, and there is an insecure url set, attempt this upload over
  // HTTP.
  // Currently we only retry over HTTP if the retry-uma-over-http flag is set.
  if (!insecure_server_url_.is_empty() && reporting_info.attempt_count() > 1 &&
      reporting_info.last_error_code() != 0 &&
      reporting_info.last_attempt_was_https() &&
      base::FeatureList::IsEnabled(kHttpRetryFeature)) {
    UploadLogToURL(compressed_log_data, log_hash, log_signature, reporting_info,
                   insecure_server_url_);
    return;
  }
  UploadLogToURL(compressed_log_data, log_hash, log_signature, reporting_info,
                 server_url_);
}

void NetMetricsLogUploader::UploadLogToURL(
    const std::string& compressed_log_data,
    const std::string& log_hash,
    const std::string& log_signature,
    const ReportingInfo& reporting_info,
    const GURL& url) {
  DCHECK(!log_hash.empty());

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  // Drop cookies and auth data.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";

  std::string reporting_info_string = SerializeReportingInfo(reporting_info);
  // If we are not using HTTPS for this upload, encrypt it. We do not encrypt
  // requests to localhost to allow testing with a local collector that doesn't
  // have decryption enabled.
  bool should_encrypt =
      !url.SchemeIs(url::kHttpsScheme) && !net::IsLocalhost(url);
  if (should_encrypt) {
    std::string base64_encoded_hash;
    if (!EncryptAndBase64EncodeString(log_hash, &base64_encoded_hash)) {
      HTTPFallbackAborted();
      return;
    }
    resource_request->headers.SetHeader("X-Chrome-UMA-Log-SHA1",
                                        base64_encoded_hash);

    std::string base64_encoded_signature;
    if (!EncryptAndBase64EncodeString(log_signature,
                                      &base64_encoded_signature)) {
      HTTPFallbackAborted();
      return;
    }
    resource_request->headers.SetHeader("X-Chrome-UMA-Log-HMAC-SHA256",
                                        base64_encoded_signature);

    std::string base64_reporting_info;
    if (!EncryptAndBase64EncodeString(reporting_info_string,
                                      &base64_reporting_info)) {
      HTTPFallbackAborted();
      return;
    }
    resource_request->headers.SetHeader("X-Chrome-UMA-ReportingInfo",
                                        base64_reporting_info);
  } else {
    resource_request->headers.SetHeader("X-Chrome-UMA-Log-SHA1", log_hash);
    resource_request->headers.SetHeader("X-Chrome-UMA-Log-HMAC-SHA256",
                                        log_signature);
    resource_request->headers.SetHeader("X-Chrome-UMA-ReportingInfo",
                                        reporting_info_string);
    // Tell the server that we're uploading gzipped protobufs only if we are not
    // encrypting, since encrypted messages have to be decrypted server side
    // after decryption, not before.
    resource_request->headers.SetHeader("content-encoding", "gzip");
  }

  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), GetNetworkTrafficAnnotation(service_type_));

  if (should_encrypt) {
    std::string encrypted_message;
    if (!EncryptString(compressed_log_data, &encrypted_message)) {
      url_loader_.reset();
      HTTPFallbackAborted();
      return;
    }
    url_loader_->AttachStringForUpload(encrypted_message, mime_type_);
  } else {
    url_loader_->AttachStringForUpload(compressed_log_data, mime_type_);
  }

  if (base::FeatureList::IsEnabled(kAblateMetricsLogUploadFeature)) {
    int failure_rate = kParamFailureRate.Get();
    std::string service_restrict = kParamAblateServiceType.Get();
    bool should_ablate =
        service_restrict.empty() ||
        (service_type_ == MetricsLogUploader::UMA &&
         service_restrict == "UMA") ||
        (service_type_ == MetricsLogUploader::UKM && service_restrict == "UKM");
    if (should_ablate && base::RandInt(0, 99) < failure_rate) {
      // Simulate collector outage by not actually trying to upload the
      // logs but instead call on_upload_complete_ immediately.
      bool was_https = url.SchemeIs(url::kHttpsScheme);
      url_loader_.reset();
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(on_upload_complete_, kParamErrorCode.Get(),
                                    net::ERR_FAILED, was_https));
      return;
    }
  }
  // It's safe to use |base::Unretained(this)| here, because |this| owns
  // the |url_loader_|, and the callback will be cancelled if the |url_loader_|
  // is destroyed.
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&NetMetricsLogUploader::OnURLLoadComplete,
                     base::Unretained(this)));
}

void NetMetricsLogUploader::HTTPFallbackAborted() {
  // The callbark is called with: a response code of 0 to indicate no upload was
  // attempted, a generic net error, and false to indicate it wasn't a secure
  // connection.
  on_upload_complete_.Run(0, net::ERR_FAILED, false);
}

// The callback is only invoked if |url_loader_| it was bound against is alive.
void NetMetricsLogUploader::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();

  int error_code = url_loader_->NetError();

  bool was_https = url_loader_->GetFinalURL().SchemeIs(url::kHttpsScheme);
  url_loader_.reset();
  on_upload_complete_.Run(response_code, error_code, was_https);
}

}  // namespace metrics
