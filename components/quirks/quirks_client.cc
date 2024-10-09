// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/quirks/quirks_client.h"

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/quirks/quirks_manager.h"
#include "components/version_info/version_info.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace quirks {

namespace {

const char kQuirksUrlFormat[] =
    "https://chromeosquirksserver-pa.googleapis.com/v2/display/%s/clients"
    "/chromeos/M%d?";

const int kMaxServerFailures = 10;

const net::BackoffEntry::Policy kDefaultBackoffPolicy = {
    1,                // Initial errors before applying backoff
    10000,            // 10 seconds.
    2,                // Factor by which the waiting time will be multiplied.
    0,                // Random fuzzing percentage.
    1000 * 3600 * 6,  // Max wait between requests = 6 hours.
    -1,               // Don't discard entry.
    true,             // Use initial delay after first error.
};

bool WriteIccFile(const base::FilePath file_path, const std::string& data) {
  if (!base::WriteFile(file_path, data)) {
    PLOG(ERROR) << "Write failed: " << file_path.value();
    return false;
  }

  VLOG(1) << data.size() << "bytes written to: " << file_path.value();
  return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// QuirksClient

QuirksClient::QuirksClient(int64_t product_id,
                           const std::string& display_name,
                           RequestFinishedCallback on_request_finished,
                           QuirksManager* manager)
    : product_id_(product_id),
      display_name_(display_name),
      on_request_finished_(std::move(on_request_finished)),
      manager_(manager),
      icc_path_(manager->delegate()->GetDisplayProfileDirectory().Append(
          IdToFileName(product_id))),
      backoff_entry_(&kDefaultBackoffPolicy) {}

QuirksClient::~QuirksClient() = default;

void QuirksClient::StartDownload() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // URL of icc file on Quirks Server.
  int major_version = version_info::GetMajorVersionNumberAsInt();
  std::string url = base::StringPrintf(
      kQuirksUrlFormat, IdToHexString(product_id_).c_str(), major_version);

  if (!display_name_.empty()) {
    url += "display_name=" + base::EscapeQueryParamValue(display_name_, true) +
           "&";
  }

  VLOG(2) << "Preparing to download\n  " << url << "\nto file "
          << icc_path_.value();

  url += "key=" + manager_->delegate()->GetApiKey();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(url);
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("quirks_display_fetcher", R"(
          semantics {
            sender: "Quirks"
            description: "Download custom display calibration file."
            trigger:
                "Chrome OS attempts to download monitor calibration files on"
                "first device login, and then once every 30 days."
            data: "ICC files to calibrate and improve the quality of a display."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            chrome_policy {
              DeviceQuirksDownloadEnabled {
                  DeviceQuirksDownloadEnabled: false
              }
            }
          }
        )");

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      manager_->url_loader_factory(),
      base::BindOnce(&QuirksClient::OnDownloadComplete,
                     base::Unretained(this)));
}

void QuirksClient::OnDownloadComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Take ownership of the loader in this scope.
  std::unique_ptr<network::SimpleURLLoader> url_loader = std::move(url_loader_);

  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers)
    response_code = url_loader->ResponseInfo()->headers->response_code();

  VLOG(2) << "QuirksClient::OnURLFetchComplete():"
          << "  net_error=" << url_loader->NetError()
          << ", response_code=" << response_code;

  if (response_code == net::HTTP_NOT_FOUND) {
    VLOG(1) << IdToFileName(product_id_) << " not found on Quirks server.";
    Shutdown(false);
    return;
  }

  if (url_loader->NetError() != net::OK) {
    if (backoff_entry_.failure_count() >= kMaxServerFailures) {
      // After 10 retires (5+ hours), give up, and try again in a month.
      VLOG(1) << "Too many retries; Quirks Client shutting down.";
      Shutdown(false);
      return;
    }
    Retry();
    return;
  }

  DCHECK(response_body);  // Guaranteed to be valid if NetError() is net::OK.
  VLOG(2) << "Quirks server response:\n" << *response_body;

  // Parse response data and write to file on file thread.
  std::string data;
  if (!ParseResult(*response_body, &data)) {
    Shutdown(false);
    return;
  }

  manager_->task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&WriteIccFile, icc_path_, data),
      base::BindOnce(&QuirksClient::Shutdown, weak_ptr_factory_.GetWeakPtr()));
}

void QuirksClient::Shutdown(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::move(on_request_finished_)
      .Run(success ? icc_path_ : base::FilePath(), true);
  manager_->ClientFinished(this);
}

void QuirksClient::Retry() {
  DCHECK(thread_checker_.CalledOnValidThread());
  backoff_entry_.InformOfRequest(false);
  const base::TimeDelta delay = backoff_entry_.GetTimeUntilRelease();

  VLOG(1) << "Schedule next Quirks download attempt in " << delay.InSecondsF()
          << " seconds (retry = " << backoff_entry_.failure_count() << ").";
  request_scheduled_.Start(FROM_HERE, delay, this,
                           &QuirksClient::StartDownload);
}

bool QuirksClient::ParseResult(const std::string& result, std::string* data) {
  std::optional<base::Value> maybe_json = base::JSONReader::Read(result);
  if (!maybe_json || !maybe_json->is_dict()) {
    VLOG(1) << "Failed to parse JSON icc data";
    return false;
  }

  std::string* data64 = maybe_json->GetDict().FindString("icc");
  if (!data64) {
    VLOG(1) << "Missing icc data";
    return false;
  }

  if (!base::Base64Decode(*data64, data)) {
    VLOG(1) << "Failed to decode Base64 icc data";
    return false;
  }

  return true;
}

}  // namespace quirks
