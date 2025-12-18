// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_CONTEXT_CONTROLLER_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_CONTEXT_CONTROLLER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/observer_list_types.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/lens/lens_bitmap_processing.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "third_party/lens_server_proto/lens_overlay_client_context.pb.h"
#include "third_party/lens_server_proto/lens_overlay_cluster_info.pb.h"
#include "third_party/lens_server_proto/lens_overlay_selection_type.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "third_party/lens_server_proto/lens_overlay_visual_search_interaction_data.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"

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
  // Note: When the ContextualTasks feature is enabled, some of these parameters
  // are overridden by the ComposeboxQueryController.
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
    // Whether to offer ZPS for the first document attachment, when multiple
    // attachments are available (true), or the only attachment if exactly one
    // attachment is available (false).
    bool prioritize_suggestions_for_the_first_attached_document = false;
    // Whether or not to support the context_id migration on the server, for
    // the multi-context input flow.
    bool enable_context_id_migration = false;
    // Whether or not to attach the page title and url directly to the suggest
    // request params.
    bool attach_page_title_and_url_to_suggest_requests = false;
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

    // The entry point for the AIM search.
    omnibox::ChromeAimEntryPoint aim_entry_point =
        omnibox::UNKNOWN_AIM_ENTRY_POINT;

    // The tokens of the contextual inputs to attach to the search url.
    std::vector<base::UnguessableToken> file_tokens;

    // Additional params to attach to the search url.
    std::map<std::string, std::string> additional_params;

    // The selection type corresponding to the interaction.
    std::optional<lens::LensOverlaySelectionType> lens_overlay_selection_type;

    // The invocation source of the interaction.
    std::optional<lens::LensOverlayInvocationSource> invocation_source;

    // The image crop corresponding to the interaction. This should only be set
    // if the selection type is set for an interaction.
    // TODO(crbug.com/462509452): Consider passing a OnceCallback that returns
    // the image crop, so that it can be create asynchronously.
    std::optional<lens::ImageCrop> image_crop;

    // The client logs corresponding to the interaction. This should only be set
    // if the selection type is set for an interaction.
    std::optional<lens::LensOverlayClientLogs> client_logs;
  };

  // Struct containing information needed to create a ClientToAimMessage.
  struct CreateClientToAimRequestInfo {
   public:
    CreateClientToAimRequestInfo();
    ~CreateClientToAimRequestInfo();

    // The text of the query.
    std::string query_text;

    // The client-side time the query was started.
    base::Time query_start_time;

    // The tokens of the newly uploaded contextual inputs to attach to the AIM
    // turn.
    std::vector<base::UnguessableToken> file_tokens;

    // The input source of the query text.
    lens::QueryPayload::QueryTextSource query_text_source =
        lens::QueryPayload::QUERY_TEXT_SOURCE_UNSPECIFIED;

    // Whether deep search is selected.
    bool deep_search_selected = false;

    // Whether create images is selected.
    bool create_images_selected = false;
  };

  virtual ~ContextualSearchContextController() = default;

  // Called when a UI is associated with the context controller.
  virtual void InitializeIfNeeded() = 0;

  // Called when a query has been submitted. `query_start_time` is the time
  // that the user clicked the submit button.
  virtual void CreateSearchUrl(
      std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info,
      base::OnceCallback<void(GURL)> callback) = 0;

  // Called when a follow-up Aquery has been submitted. `query_start_time` is
  // the time that the user clicked the submit button.
  virtual lens::ClientToAimMessage CreateClientToAimRequest(
      std::unique_ptr<CreateClientToAimRequestInfo>
          create_client_to_aim_request_info) = 0;

  // Observer management.
  virtual void AddObserver(FileUploadStatusObserver* obs) = 0;
  virtual void RemoveObserver(FileUploadStatusObserver* obs) = 0;

  // Triggers upload of the file with data and stores the file info in the
  // internal map. Call after setting the file info fields.
  virtual void StartFileUploadFlow(
      const base::UnguessableToken& file_token,
      std::unique_ptr<lens::ContextualInputData> contextual_input_data,
      std::optional<lens::ImageEncodingOptions> image_options) = 0;

  virtual bool DeleteFile(const base::UnguessableToken& file_token) = 0;
  virtual void ClearFiles() = 0;

  // Creates the suggest inputs proto for the given attached context tokens.
  virtual std::unique_ptr<lens::proto::LensOverlaySuggestInputs>
  CreateSuggestInputs(
      const std::vector<base::UnguessableToken>& attached_context_tokens) = 0;

  // Return the file from `active_files_` map or nullptr if not found.
  virtual const FileInfo* GetFileInfo(
      const base::UnguessableToken& file_token) = 0;

  // Return the file infos for all files in the request.
  virtual std::vector<const FileInfo*> GetFileInfoList() = 0;
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_CONTEXT_CONTROLLER_H_
