// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/net_metrics_log_uploader.h"

#include <sstream>
#include <string_view>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/encrypted_messages/encrypted_message.pb.h"
#include "components/encrypted_messages/message_encrypter.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_log_uploader.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/reporting_info.pb.h"
#include "third_party/zlib/google/compression_utils.h"
#include "url/gurl.h"

namespace {

// Constants used for encrypting logs that are sent over HTTP. The
// corresponding private key is used by the metrics server to decrypt logs.
const char kEncryptedMessageLabel[] = "metrics log";

const uint8_t kServerPublicKey[] = {
    0x51, 0xcc, 0x52, 0x67, 0x42, 0x47, 0x3b, 0x10, 0xe8, 0x63, 0x18,
    0x3c, 0x61, 0xa7, 0x96, 0x76, 0x86, 0x91, 0x40, 0x71, 0x39, 0x5f,
    0x31, 0x1a, 0x39, 0x5b, 0x76, 0xb1, 0x6b, 0x3d, 0x6a, 0x2b};

const uint32_t kServerPublicKeyVersion = 1;

constexpr char kNoUploadUrlsReasonMsg[] =
    "No server upload URLs specified. Will not attempt to retransmit.";

net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotation(
    const metrics::MetricsLogUploader::MetricServiceType& service_type,
    const metrics::LogMetadata& log_metadata) {
  // The code in this function should remain so that we won't need a default
  // case that does not have meaningful annotation.
  // Structured Metrics is an UMA consented metric service.
  if (service_type == metrics::MetricsLogUploader::UMA ||
      service_type == metrics::MetricsLogUploader::STRUCTURED_METRICS) {
    return net::DefineNetworkTrafficAnnotation("metrics_report_uma", R"(
        semantics {
          sender: "Metrics UMA Log Uploader"
          description:
            "Report of usage statistics and crash-related data about Chrome. "
            "Usage statistics contain information such as preferences, button "
            "clicks, and memory usage and do not include web page URLs or "
            "personal information. See more at "
            "https://www.google.com/chrome/browser/privacy/ under 'Usage "
            "statistics and crash reports'. Usage statistics are tied to a "
            "pseudonymous machine identifier and not to your email address."
          trigger:
            "Reports are automatically generated on startup and at intervals "
            "while Chrome is running."
          data:
            "A protocol buffer with usage statistics and crash related data."
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2024-02-15"
          user_data {
            type: BIRTH_DATE
            type: GENDER
            type: HW_OS_INFO
            type: OTHER
          }
          internal {
            contacts {
              owners: "//components/metrics/OWNERS"
            }
          }
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable this feature via "
            "\"Help improve Chrome's features and performance\" in Chrome "
            "settings under Sync and Google services > Other Google services. "
            "The feature is enabled by default."
          chrome_policy {
            MetricsReportingEnabled {
              policy_options {mode: MANDATORY}
              MetricsReportingEnabled: false
            }
          }
        })");
  }
  DCHECK_EQ(service_type, metrics::MetricsLogUploader::UKM);

  if (log_metadata.log_source_type.has_value() &&
      log_metadata.log_source_type.value() ==
          metrics::UkmLogSourceType::APPKM_ONLY) {
    return net::DefineNetworkTrafficAnnotation("metrics_report_appkm", R"(
      semantics {
        sender: "Metrics AppKM Log Uploader"
        description:
          "Report of usage statistics that are keyed by App Identifiers to "
          "Google. These reports only contain App-Keyed Metrics (AppKMs) "
          "records, which are the metrics related to the user interaction with "
          "various Apps on ChromeOS devices only. The apps platform includes, "
          "but is not limited to, progressive web apps (PWA), Chrome apps, and "
          "apps from the various VMs / GuestOS's: Android (ARC++), Linux "
          "(Crostini), Windows (Parallels), and Steam (Borealis). Usage "
          "statistics are tied to a pseudonymous machine identifier and not to "
          "your email address."
        trigger:
          "Reports are automatically generated on startup and at intervals "
          "while Chrome is running with usage statistics and App Sync settings "
          "enabled."
        data:
          "A protocol buffer with usage statistics and associated App Identifiers."
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2024-02-15"
        user_data {
          type: BIRTH_DATE
          type: GENDER
          type: HW_OS_INFO
          type: SENSITIVE_URL
          type: OTHER
        }
        internal {
          contacts {
            owners: "//components/metrics/OWNERS"
          }
        }
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users can enable or disable this feature using App Sync or usage "
          "statistics checkbox from the settings. Both are on by default, but "
          "can be turned-off by the user."
        chrome_policy {
          SyncDisabled {
            policy_options {mode: MANDATORY}
            SyncDisabled: true
          }
          MetricsReportingEnabled{
            policy_options {mode: MANDATORY}
            MetricsReportingEnabled: true
          }
          SyncTypesListDisabled {
            SyncTypesListDisabled: {
              entries: "apps"
            }
          }
        }
      })");
  } else if (log_metadata.log_source_type.has_value() &&
             log_metadata.log_source_type.value() ==
                 metrics::UkmLogSourceType::BOTH_UKM_AND_APPKM) {
    return net::DefineNetworkTrafficAnnotation("metrics_report_ukm_and_appkm",
                                               R"(
      semantics {
        sender: "Metrics UKM and AppKM Log Uploader"
        description:
          "Report of usage statistics that are keyed by URLs to Google. These "
          "reports contains both AppKM and UKM data. This includes information "
          "about the web pages you visit and your usage of them, such as page "
          "load speed. This will also include URLs and statistics related to "
          "downloaded files. These statistics may also include information "
          "about the extensions that have been installed from Chrome Web "
          "Store. Google only stores usage statistics associated with published "
          "extensions, and URLs that are known by Google’s search index. Usage "
          "statistics are tied to a pseudonymous machine identifier and not to "
          "your email address. Note: Reports containing only AppKM data will be "
          "reported under 'Metrics AppKM Log Uploader' and only UKM data will "
          "be reported under 'Metrics UKM Log Uploader' instead."
        trigger:
          "Reports are automatically generated on startup and at intervals "
          "while Chrome is running with usage statistics, 'Make searches and "
          "browsing better' and App Sync settings enabled."
        data:
          "A protocol buffer with usage statistics and associated URLs."
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2024-02-15"
        user_data {
          type: BIRTH_DATE
          type: GENDER
          type: HW_OS_INFO
          type: SENSITIVE_URL
          type: OTHER
        }
        internal {
          contacts {
            owners: "//components/metrics/OWNERS"
          }
        }
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users can disble this feature by disabling 'Make searches and "
          "browsing better' in Chrome's settings under Advanced Settings or "
          "disabling App Sync. This is only enabled if the user has 'Help "
          "improve Chrome's features and performance' enabled in the same "
          "settings menu. Information about the installed extensions is sent "
          "only if Extension Sync is enabled."
        chrome_policy {
          SyncDisabled {
            policy_options {mode: MANDATORY}
            SyncDisabled: true
          }
          MetricsReportingEnabled{
            policy_options {mode: MANDATORY}
            MetricsReportingEnabled: true
          }
          SyncTypesListDisabled {
            SyncTypesListDisabled: {
              entries: "apps"
            }
          }
          UrlKeyedAnonymizedDataCollectionEnabled {
            policy_options {mode: MANDATORY}
            UrlKeyedAnonymizedDataCollectionEnabled: false
          }
        }
      })");
  } else {
    return net::DefineNetworkTrafficAnnotation("metrics_report_ukm", R"(
      semantics {
        sender: "Metrics UKM Log Uploader"
        description:
          "Report of usage statistics that are keyed by URLs to Google. These "
          "reports contains only UKM data. This includes information about the "
          "web pages you visit and your usage of them, such as page load speed. "
          "This will also include URLs and statistics related to downloaded "
          "files. These statistics may also include information about the "
          "extensions that have been installed from Chrome Web Store. Google "
          "only stores usage statistics associated with published extensions, "
          "and URLs that are known by Google’s search index. Usage statistics "
          "are tied to a pseudonymous machine identifier and not to your email "
          "address."
        trigger:
          "Reports are automatically generated on startup and at intervals "
          "while Chrome is running with usage statistics and 'Make searches "
          "and browsing better' settings enabled."
        data:
          "A protocol buffer with usage statistics and associated URLs."
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2024-02-15"
        user_data {
          type: BIRTH_DATE
          type: GENDER
          type: HW_OS_INFO
          type: SENSITIVE_URL
          type: OTHER
        }
        internal {
          contacts {
            owners: "//components/metrics/OWNERS"
          }
        }
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
          UrlKeyedAnonymizedDataCollectionEnabled {
            policy_options {mode: MANDATORY}
            UrlKeyedAnonymizedDataCollectionEnabled: false
          }
        }
      })");
  }
}

std::string SerializeReportingInfo(
    const metrics::ReportingInfo& reporting_info) {
  std::string bytes;
  bool success = reporting_info.SerializeToString(&bytes);
  DCHECK(success);
  return base::Base64Encode(bytes);
}

// Encrypts a |plaintext| string, using the encrypted_messages component,
// returns |encrypted| which is a serialized EncryptedMessage object. Returns
// false if there was a problem encrypting.
bool EncryptString(const std::string& plaintext, std::string* encrypted) {
  encrypted_messages::EncryptedMessage encrypted_message;
  if (!encrypted_messages::EncryptSerializedMessage(
          kServerPublicKey, kServerPublicKeyVersion, kEncryptedMessageLabel,
          plaintext, &encrypted_message)) {
    NOTREACHED_IN_MIGRATION() << "Error encrypting string.";
    return false;
  }
  if (!encrypted_message.SerializeToString(encrypted)) {
    NOTREACHED_IN_MIGRATION() << "Error serializing encrypted string.";
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
  if (!EncryptString(plaintext, &encrypted_text)) {
    return false;
  }

  *encoded = base::Base64Encode(encrypted_text);
  return true;
}

#ifndef NDEBUG
void LogUploadingHistograms(const std::string& compressed_log_data) {
  if (!VLOG_IS_ON(2)) {
    return;
  }

  std::string uncompressed;
  if (!compression::GzipUncompress(compressed_log_data, &uncompressed)) {
    DVLOG(2) << "failed to uncompress log";
    return;
  }
  metrics::ChromeUserMetricsExtension proto;
  if (!proto.ParseFromString(uncompressed)) {
    DVLOG(2) << "failed to parse uncompressed log";
    return;
  };
  DVLOG(2) << "Uploading histograms...";

  const base::StatisticsRecorder::Histograms histograms =
      base::StatisticsRecorder::GetHistograms();
  auto get_histogram_name = [&](uint64_t name_hash) -> std::string {
    for (base::HistogramBase* histogram : histograms) {
      if (histogram->name_hash() == name_hash) {
        return histogram->histogram_name();
      }
    }
    return base::StrCat({"unnamed ", base::NumberToString(name_hash)});
  };

  for (int i = 0; i < proto.histogram_event_size(); i++) {
    const metrics::HistogramEventProto& event = proto.histogram_event(i);

    std::stringstream summary;
    summary << " sum=" << event.sum();
    for (int j = 0; j < event.bucket_size(); j++) {
      const metrics::HistogramEventProto::Bucket& b = event.bucket(j);
      // Empty fields have a specific meaning, see
      // third_party/metrics_proto/histogram_event.proto.
      summary << " bucket["
              << (b.has_min() ? base::NumberToString(b.min()) : "..") << '-'
              << (b.has_max() ? base::NumberToString(b.max()) : "..") << ")="
              << (b.has_count() ? base::NumberToString(b.count()) : "(1)");
    }
    DVLOG(2) << get_histogram_name(event.name_hash()) << summary.str();
  }
}
#endif

}  // namespace

namespace metrics {

NetMetricsLogUploader::NetMetricsLogUploader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& server_url,
    std::string_view mime_type,
    MetricsLogUploader::MetricServiceType service_type,
    const MetricsLogUploader::UploadCallback& on_upload_complete)
    : NetMetricsLogUploader(url_loader_factory,
                            server_url,
                            /*insecure_server_url=*/GURL(),
                            mime_type,
                            service_type,
                            on_upload_complete) {}

NetMetricsLogUploader::NetMetricsLogUploader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& server_url,
    const GURL& insecure_server_url,
    std::string_view mime_type,
    MetricsLogUploader::MetricServiceType service_type,
    const MetricsLogUploader::UploadCallback& on_upload_complete)
    : url_loader_factory_(std::move(url_loader_factory)),
      server_url_(server_url),
      insecure_server_url_(insecure_server_url),
      mime_type_(mime_type.data(), mime_type.size()),
      service_type_(service_type),
      on_upload_complete_(on_upload_complete) {}

NetMetricsLogUploader::~NetMetricsLogUploader() = default;

void NetMetricsLogUploader::UploadLog(const std::string& compressed_log_data,
                                      const LogMetadata& log_metadata,
                                      const std::string& log_hash,
                                      const std::string& log_signature,
                                      const ReportingInfo& reporting_info) {
  // If this attempt is a retry, there was a network error, the last attempt was
  // over HTTPS, and there is an insecure URL set, then attempt this upload over
  // HTTP.
  if (reporting_info.attempt_count() > 1 &&
      reporting_info.last_error_code() != 0 &&
      reporting_info.last_attempt_was_https() &&
      !insecure_server_url_.is_empty()) {
    UploadLogToURL(compressed_log_data, log_metadata, log_hash, log_signature,
                   reporting_info, insecure_server_url_);
    return;
  }
  UploadLogToURL(compressed_log_data, log_metadata, log_hash, log_signature,
                 reporting_info, server_url_);
}

void NetMetricsLogUploader::UploadLogToURL(
    const std::string& compressed_log_data,
    const LogMetadata& log_metadata,
    const std::string& log_hash,
    const std::string& log_signature,
    const ReportingInfo& reporting_info,
    const GURL& url) {
  DCHECK(!log_hash.empty());

#ifndef NDEBUG
  // For debug builds, you can use -vmodule=net_metrics_log_uploader=2
  // to enable logging of uploaded histograms. You probably also want to use
  // --force-enable-metrics-reporting, or metrics reporting may not be enabled.
  LogUploadingHistograms(compressed_log_data);
#endif

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

  net::NetworkTrafficAnnotationTag traffic_annotation =
      GetNetworkTrafficAnnotation(service_type_, log_metadata);
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

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

  // It's safe to use |base::Unretained(this)| here, because |this| owns
  // the |url_loader_|, and the callback will be cancelled if the |url_loader_|
  // is destroyed.
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&NetMetricsLogUploader::OnURLLoadComplete,
                     base::Unretained(this)));
}

void NetMetricsLogUploader::HTTPFallbackAborted() {
  // The callback is called with: a response code of 0 to indicate no upload was
  // attempted, a generic net error, and false to indicate it wasn't a secure
  // connection. If no server URLs were specified, discard the log and do not
  // attempt retransmission.
  bool force_discard =
      server_url_.is_empty() && insecure_server_url_.is_empty();
  std::string_view force_discard_reason =
      force_discard ? kNoUploadUrlsReasonMsg : "";
  on_upload_complete_.Run(/*response_code=*/0, net::ERR_FAILED,
                          /*was_https=*/false, force_discard,
                          force_discard_reason);
}

// The callback is only invoked if |url_loader_| it was bound against is alive.
void NetMetricsLogUploader::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  int error_code = url_loader_->NetError();

  bool was_https = url_loader_->GetFinalURL().SchemeIs(url::kHttpsScheme);
  url_loader_.reset();

  // If no server URLs were specified, discard the log and do not attempt
  // retransmission.
  bool force_discard =
      server_url_.is_empty() && insecure_server_url_.is_empty();
  std::string_view force_discard_reason =
      force_discard ? kNoUploadUrlsReasonMsg : "";
  on_upload_complete_.Run(response_code, error_code, was_https, force_discard,
                          force_discard_reason);
}

}  // namespace metrics
