// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_REQUEST_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_REQUEST_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/types/id_type.h"
#include "base/types/optional_ref.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "url/gurl.h"

namespace policy {
class BrowserPolicyConnector;
}  // namespace policy

namespace enterprise_connectors {

// A class to encapsulate the a request for upload. This class will provide
// all the functionality needed to generate a ContentAnalysisRequest, and
// subclasses will provide different sources of data to upload (e.g. file,
// page or string).
class BinaryUploadRequest {
 public:
  using BrowserPolicyConnectorGetter =
      base::RepeatingCallback<policy::BrowserPolicyConnector*()>;
  // Callbacks used to pass along the results of scanning. The response protos
  // will only be populated if the result is SUCCESS. Will run on UI thread.
  using ContentAnalysisCallback =
      base::OnceCallback<void(ScanRequestUploadResult,
                              ContentAnalysisResponse)>;

  // RequestStartCallback: Optional callback, called on the UI thread before
  // authentication attempts or upload. Useful for tracking individual
  // uploads.
  using RequestStartCallback =
      base::OnceCallback<void(const BinaryUploadRequest&)>;

  // Type alias for safe IDs
  using Id = base::IdTypeU32<class RequestClass>;

  BinaryUploadRequest(ContentAnalysisCallback,
                      CloudOrLocalAnalysisSettings settings,
                      BrowserPolicyConnectorGetter policy_connector_getter);
  // Optional constructor which accepts RequestStartCallback. Will be called
  // before request attempts upload.
  BinaryUploadRequest(ContentAnalysisCallback,
                      CloudOrLocalAnalysisSettings settings,
                      RequestStartCallback,
                      BrowserPolicyConnectorGetter policy_connector_getter);
  virtual ~BinaryUploadRequest();
  BinaryUploadRequest(const BinaryUploadRequest&) = delete;
  BinaryUploadRequest& operator=(const BinaryUploadRequest&) = delete;
  BinaryUploadRequest(BinaryUploadRequest&&) = delete;
  BinaryUploadRequest& operator=(BinaryUploadRequest&&) = delete;

  // Structure of data returned in the callback to GetRequestData().
  struct Data {
    Data();
    Data(const Data&);
    Data(Data&&);
    Data& operator=(const Data&);
    Data& operator=(Data&&);
    ~Data();

    // The data content. Only populated for string requests.
    std::string contents;

    // The path to the file to be scanned. Only populated for file requests.
    base::FilePath path;

    // The SHA256 of the data.
    std::string hash;

    // The size of the data. This can differ from `contents.size()` when the
    // file is too large for deep scanning. This field will contain the true
    // size.
    uint64_t size = 0;

    // The mime type of the data. Only populated for file requests.
    std::string mime_type;

    // The page's content. Only populated for page requests.
    base::ReadOnlySharedMemoryRegion page;

    // Whether the file has been obfuscated. Only populated for file requests.
    bool is_obfuscated = false;
  };

  // Asynchronously returns the data required to make a MultipartUploadRequest.
  // `result` is set to SUCCESS if getting the request data succeeded or
  // some value describing the error.
  using DataCallback = base::OnceCallback<void(ScanRequestUploadResult, Data)>;
  virtual void GetRequestData(DataCallback callback) = 0;

  // Returns the URL to send the request to.
  GURL GetUrlWithParams() const;

  // Returns the metadata to upload, as a ContentAnalysisRequest.
  const ContentAnalysisRequest& content_analysis_request() const {
    return content_analysis_request_;
  }

  const CloudOrLocalAnalysisSettings& cloud_or_local_settings() const {
    return cloud_or_local_settings_;
  }

  void set_id(Id id);
  Id id() const;

  void set_per_profile_request(bool per_profile_request);
  bool per_profile_request() const;

  // Methods for modifying the ContentAnalysisRequest.
  void set_analysis_connector(AnalysisConnector connector);
  void set_url(const GURL& url);
  void set_source(const std::string& source);
  void set_destination(const std::string& destination);
  void set_csd(safe_browsing::ClientDownloadRequest csd);
  void add_tag(const std::string& tag);
  void set_email(const std::string& email);
  void set_device_token(const std::string& token);
  void set_filename(const std::string& filename);
  void set_digest(const std::string& digest);
  void clear_dlp_scan_request();
  void set_client_metadata(ClientMetadata metadata);
  void set_content_type(const std::string& type);
  void set_tab_title(const std::string& tab_title);
  void set_user_action_id(const std::string& user_action_id);
  void set_user_action_requests_count(uint64_t user_action_requests_count);
  void set_tab_url(const GURL& tab_url);
  void set_printer_name(const std::string& printer_name);
  void set_printer_type(
      ContentMetaData::PrintMetadata::PrinterType printer_type);
  void set_clipboard_source_type(
      ContentMetaData::CopiedTextSource::CopiedTextSourceType source_type);
  void set_clipboard_source_url(const std::string& url);
  void set_password(const std::string& password);
  void set_reason(ContentAnalysisRequest::Reason reason);
  void set_require_metadata_verdict(bool require_metadata_verdict);
  void set_is_content_encrypted(bool is_content_encrypted);
  void set_is_content_too_large(bool is_content_too_large);
  void set_should_skip_malware_scan(bool should_skip);
  void set_blocking(bool blocking);
  void add_local_ips(const std::string& ip_address);
  void set_referrer_chain(const google::protobuf::RepeatedPtrField<
                          safe_browsing::ReferrerChainEntry> referrer_chain);
  void set_content_area_account_email(const std::string& email);
  void set_source_content_area_account_email(const std::string& email);
  void set_frame_url_chain(
      const google::protobuf::RepeatedPtrField<std::string> frame_url_chain);
  void set_content_hash_in_final_call(bool content_hash_in_final_call);
  void set_file_size(uint64_t file_size);

  std::string SetRandomRequestToken();

  // Methods for accessing the ContentAnalysisRequest.
  AnalysisConnector analysis_connector();
  const std::string& device_token() const;
  const std::string& request_token() const;
  const std::string& filename() const;
  const std::string& digest() const;
  const std::string& content_type() const;
  const std::string& user_action_id() const;
  const std::string& tab_title() const;
  const std::string& printer_name() const;
  uint64_t user_action_requests_count() const;
  GURL tab_url() const;
  base::optional_ref<const std::string> password() const;
  ContentAnalysisRequest::Reason reason() const;
  bool blocking() const;
  bool is_content_encrypted() const;
  bool is_content_too_large() const;
  bool should_skip_malware_scan() const;
  bool content_hash_in_final_call() const;
  uint64_t file_size() const;

  // Called when beginning to try upload.
  void StartRequest();

  // Finish the request, with the given `result` and `response` from the
  // server.
  void FinishRequest(ScanRequestUploadResult result,
                     ContentAnalysisResponse response);

  // Calls SerializeToString on the appropriate proto request.
  void SerializeToString(std::string* destination) const;

  // Method used to identify authentication requests.
  virtual bool IsAuthRequest() const;

  const std::string& access_token() const;
  void set_access_token(const std::string& access_token);

  void set_image_paste(bool image_paste);
  bool image_paste() const;

  // Non-null for a file request when the hash is computed after
  // GetRequestData. When Run() with a callback parameter, stores the cb to run
  // it with the hash once it is computed. If the hash already computed, run the
  // cb immediately. When the boolean param is true, the cb will be placed at
  // the end of the list that of cbs to run.
  base::RepeatingCallback<void(bool, OnGotHashCallback)>
      register_on_got_hash_callback_;

 private:
  std::optional<GURL> GetUrlOverride() const;

  Id id_;
  ContentAnalysisRequest content_analysis_request_;
  ContentAnalysisCallback content_analysis_callback_;
  RequestStartCallback request_start_callback_;

  // Settings used to determine how the request is used in the cloud or
  // locally.
  CloudOrLocalAnalysisSettings cloud_or_local_settings_;

  BrowserPolicyConnectorGetter policy_connector_getter_;

  // Indicates if the request was triggered by a profile-level policy or not.
  bool per_profile_request_ = false;

  // Access token to be attached in the request headers.
  std::string access_token_;

  bool image_paste_ = false;

  bool is_content_too_large_ = false;
  bool should_skip_malware_scan_ = false;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_REQUEST_H_
