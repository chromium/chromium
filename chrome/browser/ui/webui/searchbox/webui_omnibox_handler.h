// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_WEBUI_OMNIBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_WEBUI_OMNIBOX_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_handler.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

class MetricsReporter;
class OmniboxController;
class OmniboxPopupUI;

namespace content {
class WebUI;
}  // namespace content

// Handles bidirectional communication between NTP realbox JS and the browser.
class WebuiOmniboxHandler : public SearchboxHandler,
                            OmniboxEditModel::Observer {
 public:
  WebuiOmniboxHandler(
      mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
      MetricsReporter* metrics_reporter,
      OmniboxController* omnibox_controller,
      OmniboxPopupUI* omnibox_popup_ui,
      content::WebUI* web_ui);

  WebuiOmniboxHandler(const WebuiOmniboxHandler&) = delete;
  WebuiOmniboxHandler& operator=(const WebuiOmniboxHandler&) = delete;

  ~WebuiOmniboxHandler() override;

  // searchbox::mojom::PageHandler:
  void ActivateKeyword(uint8_t line,
                       const GURL& url,
                       base::TimeTicks match_selection_timestamp,
                       bool is_mouse_event) override;
  void OnThumbnailRemoved() override {}
  void ShowContextMenu(const gfx::Point& point) override;

  // SearchboxHandler:
  std::optional<searchbox::mojom::AutocompleteMatchPtr> CreateAutocompleteMatch(
      const AutocompleteMatch& match,
      size_t line,
      const OmniboxEditModel* edit_model,
      bookmarks::BookmarkModel* bookmark_model,
      const omnibox::GroupConfigMap& suggestion_groups_map,
      const TemplateURLService* turl_service) const override;

  // AutocompleteController::Observer:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

  // OmniboxEditModel::Observer:
  void OnSelectionChanged(OmniboxPopupSelection old_selection,
                          OmniboxPopupSelection selection) override;
  void OnMatchIconUpdated(size_t index) override {}

 private:
  // Observe `OmniboxEditModel` for updates that require updating the views.
  base::ScopedObservation<OmniboxEditModel, OmniboxEditModel::Observer>
      edit_model_observation_{this};

  raw_ptr<MetricsReporter> metrics_reporter_;

  raw_ref<OmniboxPopupUI> omnibox_popup_ui_;

  base::WeakPtrFactory<WebuiOmniboxHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_WEBUI_OMNIBOX_HANDLER_H_
