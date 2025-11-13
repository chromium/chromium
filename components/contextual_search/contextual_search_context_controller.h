// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_CONTEXT_CONTROLLER_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_CONTEXT_CONTROLLER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/observer_list_types.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/lens/lens_bitmap_processing.h"

namespace base {
class Time;
class UnguessableToken;
}  // namespace base

class GURL;

namespace lens {
enum class MimeType;
struct ContextualInputData;
namespace proto {
class LensOverlaySuggestInputs;
}  // namespace proto
}  // namespace lens

namespace contextual_search {

// The contextual search context controller is responsible for managing the
// context of a contextual search query.
class ContextualSearchContextController {
 public:
  // Struct containing configuration params for the context controller.
  struct ConfigParams {
   public:
    // Whether to send the `lns_surface` parameter in search URLs.
    bool send_lns_surface = false;
    // If `send_lns_surface` is true, whether to suppress the `lns_surface`
    // parameter if there is no image upload. Does nothing if `send_lns_surface`
    // is false.
    bool suppress_lns_surface_param_if_no_image = true;
    // Whether to enable the multi-context input flow.
    bool enable_multi_context_input_flow = false;
    // Whether to enable viewport images.
    bool enable_viewport_images = false;
    // Whether or not to send viewport images with separate request ids from
    // their associated page context, for the multi-context input flow.
    // Does nothing if `enable_multi_context_input_flow` is false or if
    // `enable_viewport_images` is false.
    bool use_separate_request_ids_for_multi_context_viewport_images = true;
  };

  // Observer interface for the Page Handler to get updates on file upload
  class FileUploadStatusObserver : public base::CheckedObserver {
   public:
    virtual void OnFileUploadStatusChanged(
        const base::UnguessableToken& file_token,
        lens::MimeType mime_type,
        FileUploadStatus file_upload_status,
        const std::optional<FileUploadErrorType>& error_type) = 0;

   protected:
    ~FileUploadStatusObserver() override = default;
  };

  // The possible search url types.
  enum class SearchUrlType {
    // The standard "All" tab search experience
    kStandard = 0,
    // The AIM search type.
    kAim = 1,
  };

  // Struct containing information needed to construct a search url.
  struct CreateSearchUrlRequestInfo {
   public:
    CreateSearchUrlRequestInfo();
    ~CreateSearchUrlRequestInfo();

    // The text of the query.
    std::string query_text;

    // The client-side time the query was started.
    base::Time query_start_time;

    // The type of search url to create.
    SearchUrlType search_url_type = SearchUrlType::kAim;

    // Additional params to attach to the search url.
    std::map<std::string, std::string> additional_params;
  };

  virtual ~ContextualSearchContextController() = default;

  // Called when a UI is associated with the context controller.
  virtual void InitializeIfNeeded() = 0;

  // Called when a query has been submitted. `query_start_time` is the time
  // that the user clicked the submit button.
  virtual GURL CreateSearchUrl(
      std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info) = 0;

  // Observer management.
  virtual void AddObserver(FileUploadStatusObserver* obs) = 0;
  virtual void RemoveObserver(FileUploadStatusObserver* obs) = 0;

  // Triggers upload of the file with data and stores the file info in the
  // internal map. Call after setting the file info fields.
  virtual void StartFileUploadFlow(
      const base::UnguessableToken& file_token,
      std::unique_ptr<lens::ContextualInputData> contextual_input_data,
      std::optional<lens::ImageEncodingOptions> image_options) = 0;

  // Removes file from file cache.
  virtual bool DeleteFile(const base::UnguessableToken& file_token) = 0;

  // Clear entire file cache.
  virtual void ClearFiles() = 0;

  // Resets the suggest inputs, setting it to the suggest inputs for the
  // last file if there is only one attached file remaining.
  virtual void ResetSuggestInputs() = 0;

  virtual int num_files_in_request() = 0;

  // Return the file from `active_files_` map or nullptr if not found.
  virtual const FileInfo* GetFileInfo(
      const base::UnguessableToken& file_token) = 0;

  virtual const lens::proto::LensOverlaySuggestInputs& suggest_inputs()
      const = 0;
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_CONTEXT_CONTROLLER_H_
