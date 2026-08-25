// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

class MetricsReporter;
class OmniboxEverywhereService;

namespace content {
class WebUI;
}

// Custom handler for OmniboxEverywhere searchbox.
// It owns its own OmniboxController and OmniboxEverywhereClient.
class OmniboxEverywhereHandler : public ContextualSearchboxHandler {
 public:
  OmniboxEverywhereHandler(
      mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
      mojo::PendingRemote<searchbox::mojom::Page> pending_page,
      MetricsReporter* metrics_reporter,
      content::WebUI* web_ui,
      OmniboxEverywhereService* service,
      GetSessionHandleCallback get_session_callback,
      ScreenshareDelegate* screenshare_delegate = nullptr);

  OmniboxEverywhereHandler(const OmniboxEverywhereHandler&) = delete;
  OmniboxEverywhereHandler& operator=(const OmniboxEverywhereHandler&) = delete;

  ~OmniboxEverywhereHandler() override;

  // searchbox::mojom::PageHandler:
  void ActivateKeyword(uint8_t line,
                       const GURL& url,
                       base::TimeTicks match_selection_timestamp,
                       bool is_mouse_event) override;
  void OnThumbnailRemoved() override {}
  void SubmitQuery(const std::string& query_text,
                   uint8_t mouse_button,
                   bool alt_key,
                   bool ctrl_key,
                   bool meta_key,
                   bool shift_key,
                   bool is_voice_search) override;

  // SearchboxHandler:
  std::optional<searchbox::mojom::AutocompleteMatchPtr> CreateAutocompleteMatch(
      const AutocompleteMatch& match,
      size_t line,
      bookmarks::BookmarkModel* bookmark_model,
      const omnibox::GroupConfigMap& suggestion_groups_map,
      const TemplateURLService* turl_service) const override;

  // Overridden to intercept the Drive upload request, dynamically associate the
  // standalone WebContents with the latest active BrowserWindowInterface, and
  // update the OmniboxEverywhereService state.
  void OnDriveUploadClicked(OnDriveUploadClickedCallback callback) override;
  void OpenProfilePicker() override;

  // ContextualSearchboxHandler:
  void OpenUrl(GURL url,
               const WindowOpenDisposition disposition,
               base::OnceCallback<void(content::NavigationHandle&)>
                   navigation_handle_callback) override;

  // Overridden to notify the OmniboxEverywhereService when the Drive picker is
  // dismissed, allowing the standalone widget to regain activation and focus.
  void CleanupDrivePicker() override;

 private:
  void OnAimEligibilityChanged();

  raw_ptr<OmniboxEverywhereService> service_;
  PrefChangeRegistrar pref_change_registrar_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_HANDLER_H_
