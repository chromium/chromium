// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_SEARCHBOX_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_SEARCHBOX_CONTROLLER_H_

#include "chrome/browser/ui/webui/searchbox/lens_searchbox_client.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/sessions/core/session_id.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "url/gurl.h"

class LensSearchController;

namespace lens {

// Controller for the Lens searchbox. This class is responsible for handling
// communications between the Lens WebUI searchbox and other Lens components.
// This class is responsible for both the overlay and side panel searchboxes.
class LensSearchboxController : public LensSearchboxClient {
 public:
  explicit LensSearchboxController(
      LensSearchController* lens_search_controller);
  ~LensSearchboxController() override;

 private:
  // Overridden from LensSearchboxClient:
  const GURL& GetPageURL() const override;
  SessionID GetTabId() const override;
  metrics::OmniboxEventProto::PageClassification GetPageClassification()
      const override;
  std::string& GetThumbnail() override;
  const lens::proto::LensOverlaySuggestInputs& GetLensSuggestInputs()
      const override;
  void OnTextModified() override;
  void OnThumbnailRemoved() override;
  void OnSuggestionAccepted(const GURL& destination_url,
                            AutocompleteMatchType::Type match_type,
                            bool is_zero_prefix_suggestion) override;
  void OnFocusChanged(bool focused) override;
  void OnPageBound() override;
  void ShowGhostLoaderErrorState() override;
  void OnZeroSuggestShown() override;

  // Owns this.
  const raw_ptr<LensSearchController> lens_search_controller_;

  // TODO(crbug.com/413138792): Implement temporary placeholder.
  std::string selected_region_thumbnail_uri_;
};
}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_SEARCHBOX_CONTROLLER_H_
