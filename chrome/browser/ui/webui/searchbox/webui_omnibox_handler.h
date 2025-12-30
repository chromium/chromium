// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_WEBUI_OMNIBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_WEBUI_OMNIBOX_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

class MetricsReporter;
class OmniboxController;

namespace content {
class WebUI;
}  // namespace content

// Handles bidirectional communication between NTP realbox JS and the browser.
class WebuiOmniboxHandler : public ContextualSearchboxHandler,
                            OmniboxEditModel::Observer {
 public:
  WebuiOmniboxHandler(
      mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
      MetricsReporter* metrics_reporter,
      OmniboxController* omnibox_controller,
      content::WebUI* web_ui,
      GetSessionHandleCallback get_session_callback);

  WebuiOmniboxHandler(const WebuiOmniboxHandler&) = delete;
  WebuiOmniboxHandler& operator=(const WebuiOmniboxHandler&) = delete;

  ~WebuiOmniboxHandler() override;

  void SetEmbedder(base::WeakPtr<TopChromeWebUIController::Embedder> embedder) {
    embedder_ = embedder;
  }

  // searchbox::mojom::PageHandler:
  void ActivateKeyword(uint8_t line,
                       const GURL& url,
                       base::TimeTicks match_selection_timestamp,
                       bool is_mouse_event) override;
  void OnThumbnailRemoved() override {}
  void ShowContextMenu(const gfx::Point& point) override;
  void OpenLensSearch() override;
  void AddTabContext(int32_t tab_id,
                     bool delay_upload,
                     AddTabContextCallback) override;

  void OnShow();

  // ContextualSearchboxHandler:
  void SetPage(
      mojo::PendingRemote<searchbox::mojom::Page> pending_page) override;

  // SearchboxHandler:
  std::optional<searchbox::mojom::AutocompleteMatchPtr> CreateAutocompleteMatch(
      const AutocompleteMatch& match,
      size_t line,
      const OmniboxEditModel* edit_model,
      bookmarks::BookmarkModel* bookmark_model,
      const omnibox::GroupConfigMap& suggestion_groups_map,
      const TemplateURLService* turl_service) const override;

  // AutocompleteController::Observer:
  void OnStart(AutocompleteController* controller,
               const AutocompleteInput& input) override;
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

  // OmniboxEditModel::Observer:
  void OnSelectionChanged(OmniboxPopupSelection old_selection,
                          OmniboxPopupSelection selection) override;
  void OnMatchIconUpdated(size_t index) override {}
  void OnContentsChanged() override {}
  void OnKeywordStateChanged(bool is_keyword_selected) override;

  // `AimEligibilityService` callback.
  void OnAimEligibilityChanged();

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  void OnNavigationFinished(content::NavigationHandle* navigation_handle);

 protected:
  // ContextualSearchboxHandler:
  std::optional<lens::LensOverlayInvocationSource> GetInvocationSource()
      const override;

 private:
  // Delegate to observe WebContents.
  // Managed as a separate class to prevent member naming conflicts
  // of `web_contents_` with a member of the same name in `SearchboxHandler`.
  class WebContentsObserver : public content::WebContentsObserver {
   public:
    explicit WebContentsObserver(WebuiOmniboxHandler* handler,
                                 content::WebContents* web_contents);

    void ScopedObserve(content::WebContents* web_contents);

    void DidFinishNavigation(content::NavigationHandle* handle) override;

   private:
    raw_ptr<WebuiOmniboxHandler> handler_;
  };

  // ContextualSearchboxHandler:
  int GetContextMenuMaxTabSuggestions() override;

  WebContentsObserver web_contents_observer_;

  // Observe `OmniboxEditModel` for updates that require updating the views.
  base::ScopedObservation<OmniboxEditModel, OmniboxEditModel::Observer>
      edit_model_observation_{this};

  raw_ptr<MetricsReporter> metrics_reporter_;

  base::WeakPtr<TopChromeWebUIController::Embedder> embedder_;

  base::CallbackListSubscription aim_eligibility_subscription_;

  base::WeakPtrFactory<WebuiOmniboxHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_WEBUI_OMNIBOX_HANDLER_H_
