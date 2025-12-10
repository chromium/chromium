// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_QUERY_FLOW_ROUTER_H_
#define CHROME_BROWSER_UI_LENS_LENS_QUERY_FLOW_ROUTER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "content/public/browser/web_contents.h"

namespace lens {

using CreateSearchUrlRequestInfo = contextual_search::
    ContextualSearchContextController::CreateSearchUrlRequestInfo;
using SearchUrlType =
    contextual_search::ContextualSearchContextController::SearchUrlType;

// A router for queries that Lens should perform.
class LensQueryFlowRouter {
 public:
  explicit LensQueryFlowRouter(LensSearchController* lens_search_controller);
  virtual ~LensQueryFlowRouter();

  // Whether the query router is in an off state.
  bool IsOff() const;

  // Starts a query flow by sending a request to Lens using the viewport
  // screenshot. This is called by the the overlay when it is first initialized.
  // This is not called when the overlay is reshown. If there is a pending
  // contextual or region search that should be requested immediately, it will
  // be called after this function.
  void StartQueryFlow(
      const SkBitmap& screenshot,
      GURL page_url,
      std::optional<std::string> page_title,
      std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes,
      base::span<const PageContent> underlying_page_contents,
      lens::MimeType primary_content_type,
      std::optional<uint32_t> pdf_current_page,
      float ui_scale_factor,
      base::TimeTicks invocation_time);

  // Restarts the query flow if its in a state where no cluster info is
  // available or permissions were not granted.
  void MaybeRestartQueryFlow();

  // Sends a region search interaction. Expected to be called multiple times. If
  // region_bytes are included, those will be sent to Lens instead of cropping
  // the region out of the screenshot. This should be used to provide a higher
  // definition image than image cropping would provide.
  void SendRegionSearch(
      base::Time query_start_time,
      lens::mojom::CenterRotatedBoxPtr region,
      lens::LensOverlaySelectionType lens_selection_type,
      std::map<std::string, std::string> additional_search_query_params,
      std::optional<SkBitmap> region_bytes);

  // Sends a text-only interaction. Expected to be called multiple times.
  void SendTextOnlyQuery(
      base::Time query_start_time,
      const std::string& query_text,
      lens::LensOverlaySelectionType lens_selection_type,
      std::map<std::string, std::string> additional_search_query_params);

  // Sends a text query interaction contextualized to the current page. Expected
  // to be called multiple times.
  void SendContextualTextQuery(
      base::Time query_start_time,
      const std::string& query_text,
      lens::LensOverlaySelectionType lens_selection_type,
      std::map<std::string, std::string> additional_search_query_params);

  // Sends a multimodal interaction. Expected to be called multiple times.
  void SendMultimodalRequest(
      base::Time query_start_time,
      lens::mojom::CenterRotatedBoxPtr region,
      const std::string& query_text,
      lens::LensOverlaySelectionType lens_selection_type,
      std::map<std::string, std::string> additional_search_query_params,
      std::optional<SkBitmap> region_bytes);

 protected:
  // Creates a contextual search session handle. Virtual for testing.
  virtual std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
  CreateContextualSearchSessionHandle();

  // Returns the viewport screenshot. Virtual for testing.
  virtual const SkBitmap& GetViewportScreenshot() const;

 private:
  LensOverlayQueryController* lens_overlay_query_controller() const {
    return lens_search_controller_->lens_overlay_query_controller();
  }

  LensSearchContextualizationController*
  lens_search_contextualization_controller() const {
    return lens_search_controller_->lens_search_contextualization_controller();
  }

  tabs::TabInterface* tab_interface() const {
    return lens_search_controller_->GetTabInterface();
  }

  content::WebContents* web_contents() const {
    return tab_interface()->GetContents();
  }

  BrowserWindowInterface* browser_window_interface() const {
    return tab_interface()->GetBrowserWindowInterface();
  }

  Profile* profile() const {
    return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  }

  // Loads the provided query text in the contextual tasks panel.
  void LoadQueryInContextualTasks(const std::string& query_text);

  // Sends the provided request info to the contextual tasks panel to create a
  // search URL which is then loaded into the contextual tasks panel.
  void SendInteractionToContextualTasks(
      std::unique_ptr<CreateSearchUrlRequestInfo> request_info);

  // Opens the contextual tasks panel to a provided URL.
  void OpenContextualTasksPanel(const GURL& url);

  // Uploads the viewport and page context using the provided session handle.
  void UploadContextualInputData(
      contextual_search::ContextualSearchSessionHandle* session_handle,
      std::unique_ptr<lens::ContextualInputData> contextual_input_data);

  // Called when the tab context has been added to the session handle, allowing
  // the upload flow to start.
  void OnFinishedAddingTabContext(
      contextual_search::ContextualSearchSessionHandle* session_handle,
      std::unique_ptr<lens::ContextualInputData> contextual_input_data,
      const base::UnguessableToken& token);

  // Creates the contextual input data from data collected from overlay
  // initialization to be used for the contextual search session.
  std::unique_ptr<lens::ContextualInputData> CreateContextualInputData(
      const SkBitmap& screenshot,
      GURL page_url,
      std::optional<std::string> page_title,
      std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes,
      base::span<const PageContent> underlying_page_contents,
      lens::MimeType primary_content_type,
      std::optional<uint32_t> pdf_current_page,
      float ui_scale_factor,
      base::TimeTicks invocation_time);

  // Creates the search url request info from an interaction.
  std::unique_ptr<CreateSearchUrlRequestInfo>
  CreateSearchUrlRequestInfoFromInteraction(
      lens::mojom::CenterRotatedBoxPtr region,
      std::optional<SkBitmap> region_bytes,
      std::optional<std::string> query_text,
      lens::LensOverlaySelectionType lens_selection_type,
      std::map<std::string, std::string> additional_search_query_params,
      base::Time query_start_time);

  // Returns the contextual search session handle for the query router. If the
  // handle does not exist, it will create one.
  contextual_search::ContextualSearchSessionHandle*
  GetOrCreateContextualSearchSessionHandle();

  // Returns the contextual search session handle for the query router if it
  // exists.
  contextual_search::ContextualSearchSessionHandle*
  GetContextualSearchSessionHandle() const;

  // The contextual search session handle that is used to make requests to the
  // contextual search service. This is only stored by this query router in
  // cases where the overlay has been opened but a results panel is not present.
  // This handle is pending because it will eventually be moved to the
  // contextual tasks UI for ownership.
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      pending_session_handle_;

  raw_ptr<LensSearchController> lens_search_controller_;

  base::WeakPtrFactory<LensQueryFlowRouter> weak_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_QUERY_FLOW_ROUTER_H_
