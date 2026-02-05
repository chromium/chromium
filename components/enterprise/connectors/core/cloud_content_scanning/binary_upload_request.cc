// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/enterprise/common/strings.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"
#include "net/base/url_util.h"

namespace enterprise_connectors {

BinaryUploadRequest::Data::Data() = default;

BinaryUploadRequest::Data::Data(const Data& other) {
  operator=(other);
}

BinaryUploadRequest::Data::Data(Data&&) = default;

BinaryUploadRequest::Data& BinaryUploadRequest::Data::operator=(
    const BinaryUploadRequest::Data& other) {
  contents = other.contents;
  path = other.path;
  hash = other.hash;
  size = other.size;
  mime_type = other.mime_type;
  page = other.page.Duplicate();
  is_obfuscated = other.is_obfuscated;
  return *this;
}

BinaryUploadRequest::Data& BinaryUploadRequest::Data::operator=(
    BinaryUploadRequest::Data&& other) = default;
BinaryUploadRequest::Data::~Data() = default;

BinaryUploadRequest::BinaryUploadRequest(
    ContentAnalysisCallback callback,
    enterprise_connectors::CloudOrLocalAnalysisSettings settings,
    BrowserPolicyConnectorGetter policy_connector_getter)
    : content_analysis_callback_(std::move(callback)),
      cloud_or_local_settings_(std::move(settings)),
      policy_connector_getter_(std::move(policy_connector_getter)) {}

BinaryUploadRequest::BinaryUploadRequest(
    ContentAnalysisCallback content_analysis_callback,
    enterprise_connectors::CloudOrLocalAnalysisSettings settings,
    BinaryUploadRequest::RequestStartCallback start_callback,
    BrowserPolicyConnectorGetter policy_connector_getter)
    : content_analysis_callback_(std::move(content_analysis_callback)),
      request_start_callback_(std::move(start_callback)),
      cloud_or_local_settings_(std::move(settings)),
      policy_connector_getter_(std::move(policy_connector_getter)) {}

BinaryUploadRequest::~BinaryUploadRequest() = default;

void BinaryUploadRequest::set_id(Id id) {
  id_ = id;
}

BinaryUploadRequest::Id BinaryUploadRequest::id() const {
  return id_;
}

void BinaryUploadRequest::set_per_profile_request(bool per_profile_request) {
  per_profile_request_ = per_profile_request;
}

bool BinaryUploadRequest::per_profile_request() const {
  return per_profile_request_;
}

void BinaryUploadRequest::set_device_token(const std::string& token) {
  content_analysis_request_.set_device_token(token);
}

void BinaryUploadRequest::set_filename(const std::string& filename) {
  content_analysis_request_.mutable_request_data()->set_filename(filename);
}

void BinaryUploadRequest::set_digest(const std::string& digest) {
  content_analysis_request_.mutable_request_data()->set_digest(digest);
}

void BinaryUploadRequest::clear_dlp_scan_request() {
  auto* tags = content_analysis_request_.mutable_tags();
  auto it = std::ranges::find(*tags, "dlp");
  if (it != tags->end()) {
    tags->erase(it);
  }
}

void BinaryUploadRequest::set_analysis_connector(
    enterprise_connectors::AnalysisConnector connector) {
  content_analysis_request_.set_analysis_connector(connector);
}

void BinaryUploadRequest::set_url(const GURL& url) {
  content_analysis_request_.mutable_request_data()->set_url(url.spec());
}

void BinaryUploadRequest::set_source(const std::string& source) {
  content_analysis_request_.mutable_request_data()->set_source(source);
}

void BinaryUploadRequest::set_destination(const std::string& destination) {
  content_analysis_request_.mutable_request_data()->set_destination(
      destination);
}

void BinaryUploadRequest::set_csd(safe_browsing::ClientDownloadRequest csd) {
  *content_analysis_request_.mutable_request_data()->mutable_csd() =
      std::move(csd);
}

void BinaryUploadRequest::add_tag(const std::string& tag) {
  content_analysis_request_.add_tags(tag);
}

void BinaryUploadRequest::set_email(const std::string& email) {
  content_analysis_request_.mutable_request_data()->set_email(email);
}

void BinaryUploadRequest::set_client_metadata(
    enterprise_connectors::ClientMetadata metadata) {
  *content_analysis_request_.mutable_client_metadata() = std::move(metadata);
}

void BinaryUploadRequest::set_content_type(const std::string& type) {
  content_analysis_request_.mutable_request_data()->set_content_type(type);
}

void BinaryUploadRequest::set_tab_title(const std::string& tab_title) {
  content_analysis_request_.mutable_request_data()->set_tab_title(tab_title);
}

void BinaryUploadRequest::set_user_action_id(
    const std::string& user_action_id) {
  content_analysis_request_.set_user_action_id(user_action_id);
}

void BinaryUploadRequest::set_user_action_requests_count(
    uint64_t user_action_requests_count) {
  content_analysis_request_.set_user_action_requests_count(
      user_action_requests_count);
}

void BinaryUploadRequest::set_tab_url(const GURL& tab_url) {
  content_analysis_request_.mutable_request_data()->set_tab_url(tab_url.spec());
}

void BinaryUploadRequest::set_printer_name(const std::string& printer_name) {
  content_analysis_request_.mutable_request_data()
      ->mutable_print_metadata()
      ->set_printer_name(printer_name);
}

void BinaryUploadRequest::set_printer_type(
    enterprise_connectors::ContentMetaData::PrintMetadata::PrinterType
        printer_type) {
  content_analysis_request_.mutable_request_data()
      ->mutable_print_metadata()
      ->set_printer_type(printer_type);
}

void BinaryUploadRequest::set_clipboard_source_type(
    enterprise_connectors::ContentMetaData::CopiedTextSource::
        CopiedTextSourceType source_type) {
  content_analysis_request_.mutable_request_data()
      ->mutable_copied_text_source()
      ->set_context(source_type);
}

void BinaryUploadRequest::set_clipboard_source_url(const std::string& url) {
  content_analysis_request_.mutable_request_data()
      ->mutable_copied_text_source()
      ->set_url(url);
}

void BinaryUploadRequest::set_password(const std::string& password) {
  content_analysis_request_.mutable_request_data()->set_decryption_key(
      password);
}

void BinaryUploadRequest::set_reason(
    enterprise_connectors::ContentAnalysisRequest::Reason reason) {
  content_analysis_request_.set_reason(reason);
}

void BinaryUploadRequest::set_require_metadata_verdict(
    bool require_metadata_verdict) {
  content_analysis_request_.set_require_metadata_verdict(
      require_metadata_verdict);
}

void BinaryUploadRequest::set_is_content_encrypted(bool is_content_encrypted) {
  content_analysis_request_.set_is_content_encrypted(is_content_encrypted);
}

void BinaryUploadRequest::set_is_content_too_large(bool is_content_too_large) {
  is_content_too_large_ = is_content_too_large;
}

void BinaryUploadRequest::set_should_skip_malware_scan(bool should_skip) {
  should_skip_malware_scan_ = should_skip;
}

void BinaryUploadRequest::set_blocking(bool blocking) {
  content_analysis_request_.set_blocking(blocking);
}

void BinaryUploadRequest::add_local_ips(const std::string& ip_address) {
  content_analysis_request_.add_local_ips(ip_address);
}

void BinaryUploadRequest::set_referrer_chain(
    const google::protobuf::RepeatedPtrField<safe_browsing::ReferrerChainEntry>
        referrer_chain) {
  *content_analysis_request_.mutable_request_data()->mutable_referrer_chain() =
      std::move(referrer_chain);
}

void BinaryUploadRequest::set_content_area_account_email(
    const std::string& email) {
  content_analysis_request_.mutable_request_data()
      ->set_content_area_account_email(email);
}

void BinaryUploadRequest::set_source_content_area_account_email(
    const std::string& email) {
  content_analysis_request_.mutable_request_data()
      ->set_source_content_area_account_email(email);
}

void BinaryUploadRequest::set_frame_url_chain(
    const google::protobuf::RepeatedPtrField<std::string> frame_url_chain) {
  *content_analysis_request_.mutable_request_data()->mutable_frame_url_chain() =
      std::move(frame_url_chain);
}

void BinaryUploadRequest::set_content_hash_in_final_call(
    bool content_hash_in_final_call) {
  content_analysis_request_.set_content_hash_in_final_call(
      content_hash_in_final_call);
}

void BinaryUploadRequest::set_file_size(uint64_t file_size) {
  content_analysis_request_.mutable_request_data()->set_file_size(file_size);
}

std::string BinaryUploadRequest::SetRandomRequestToken() {
  DCHECK(request_token().empty());
  content_analysis_request_.set_request_token(
      base::HexEncode(base::RandBytesAsVector(128)));
  return content_analysis_request_.request_token();
}

enterprise_connectors::AnalysisConnector
BinaryUploadRequest::analysis_connector() {
  return content_analysis_request_.analysis_connector();
}

const std::string& BinaryUploadRequest::device_token() const {
  return content_analysis_request_.device_token();
}

const std::string& BinaryUploadRequest::request_token() const {
  return content_analysis_request_.request_token();
}

const std::string& BinaryUploadRequest::filename() const {
  return content_analysis_request_.request_data().filename();
}

const std::string& BinaryUploadRequest::digest() const {
  return content_analysis_request_.request_data().digest();
}

const std::string& BinaryUploadRequest::content_type() const {
  return content_analysis_request_.request_data().content_type();
}

const std::string& BinaryUploadRequest::user_action_id() const {
  return content_analysis_request_.user_action_id();
}

const std::string& BinaryUploadRequest::tab_title() const {
  return content_analysis_request_.request_data().tab_title();
}

const std::string& BinaryUploadRequest::printer_name() const {
  return content_analysis_request_.request_data()
      .print_metadata()
      .printer_name();
}

uint64_t BinaryUploadRequest::user_action_requests_count() const {
  return content_analysis_request_.user_action_requests_count();
}

GURL BinaryUploadRequest::tab_url() const {
  if (!content_analysis_request_.has_request_data()) {
    return GURL();
  }
  return GURL(content_analysis_request_.request_data().tab_url());
}

base::optional_ref<const std::string> BinaryUploadRequest::password() const {
  return content_analysis_request_.request_data().has_decryption_key()
             ? base::optional_ref(
                   content_analysis_request_.request_data().decryption_key())
             : std::nullopt;
}

enterprise_connectors::ContentAnalysisRequest::Reason
BinaryUploadRequest::reason() const {
  return content_analysis_request_.reason();
}

bool BinaryUploadRequest::blocking() const {
  return content_analysis_request_.blocking();
}

bool BinaryUploadRequest::image_paste() const {
  return image_paste_;
}

void BinaryUploadRequest::set_image_paste(bool image_paste) {
  image_paste_ = image_paste;
}

bool BinaryUploadRequest::is_content_too_large() const {
  return is_content_too_large_;
}

bool BinaryUploadRequest::should_skip_malware_scan() const {
  return should_skip_malware_scan_;
}

bool BinaryUploadRequest::is_content_encrypted() const {
  return content_analysis_request_.is_content_encrypted();
}

bool BinaryUploadRequest::content_hash_in_final_call() const {
  return content_analysis_request_.content_hash_in_final_call();
}

uint64_t BinaryUploadRequest::file_size() const {
  return content_analysis_request_.request_data().file_size();
}

void BinaryUploadRequest::StartRequest() {
  if (!request_start_callback_.is_null()) {
    std::move(request_start_callback_).Run(*this);
  }
}

void BinaryUploadRequest::FinishRequest(
    enterprise_connectors::ScanRequestUploadResult result,
    enterprise_connectors::ContentAnalysisResponse response) {
  if (content_analysis_callback_) {
    std::move(content_analysis_callback_).Run(result, response);
  }
}

void BinaryUploadRequest::SerializeToString(std::string* destination) const {
  content_analysis_request_.SerializeToString(destination);
}

GURL BinaryUploadRequest::GetUrlWithParams() const {
  DCHECK(std::holds_alternative<enterprise_connectors::CloudAnalysisSettings>(
      cloud_or_local_settings_));

  GURL url = GetUrlOverride().value_or(cloud_or_local_settings_.analysis_url());
  url = net::AppendQueryParameter(url, enterprise::kUrlParamDeviceToken,
                                  device_token());

  std::string connector;
  switch (content_analysis_request_.analysis_connector()) {
    case enterprise_connectors::FILE_ATTACHED:
      connector = "OnFileAttached";
      break;
    case enterprise_connectors::FILE_DOWNLOADED:
      connector = "OnFileDownloaded";
      break;
    case enterprise_connectors::BULK_DATA_ENTRY:
      connector = "OnBulkDataEntry";
      break;
    case enterprise_connectors::PRINT:
      connector = "OnPrint";
      break;
    case enterprise_connectors::FILE_TRANSFER:
      connector = "OnFileTransfer";
      break;
    case enterprise_connectors::ANALYSIS_CONNECTOR_UNSPECIFIED:
      break;
  }
  if (!connector.empty()) {
    url = net::AppendQueryParameter(url, enterprise::kUrlParamConnector,
                                    connector);
  }

  for (const std::string& tag : content_analysis_request_.tags()) {
    url = net::AppendQueryParameter(url, enterprise::kUrlParamTag, tag);
  }

  return url;
}

bool BinaryUploadRequest::IsAuthRequest() const {
  return false;
}

const std::string& BinaryUploadRequest::access_token() const {
  return access_token_;
}

void BinaryUploadRequest::set_access_token(const std::string& access_token) {
  access_token_ = access_token;
}

std::optional<GURL> BinaryUploadRequest::GetUrlOverride() const {
  // Ignore this flag on Stable and Beta to avoid abuse.
  if (policy_connector_getter_.is_null()) {
    return std::nullopt;
  }
  policy::BrowserPolicyConnector* policy_connector =
      policy_connector_getter_.Run();
  if (!policy_connector || !policy_connector->IsCommandLineSwitchSupported()) {
    return std::nullopt;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          safe_browsing::switches::kCloudBinaryUploadServiceUrlFlag)) {
    GURL url = GURL(command_line->GetSwitchValueASCII(
        safe_browsing::switches::kCloudBinaryUploadServiceUrlFlag));
    if (url.is_valid()) {
      return url;
    } else {
      LOG(ERROR) << "--binary-upload-service-url is set to an invalid URL";
    }
  }

  return std::nullopt;
}

}  // namespace enterprise_connectors
