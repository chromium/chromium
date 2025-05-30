// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_LENS_SEARCHBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_LENS_SEARCHBOX_HANDLER_H_

#include "chrome/browser/ui/webui/searchbox/searchbox_handler.h"
#include "components/omnibox/browser/omnibox.mojom.h"

class MetricsReporter;
class LensSearchboxClient;
class Profile;

// Browser-side handler for bidirectional communication with the WebUI
// lens overlay and side panel searchboxes.
class LensSearchboxHandler : public SearchboxHandler {
 public:
  LensSearchboxHandler(
      mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
      Profile* profile,
      content::WebContents* web_contents,
      MetricsReporter* metrics_reporter,
      LensSearchboxClient* lens_searchbox_client);

  ~LensSearchboxHandler() override;

  // searchbox::mojom::PageHandler:
  void SetPage(
      mojo::PendingRemote<searchbox::mojom::Page> pending_page) override;
  void OnFocusChanged(bool focused) override;
  void QueryAutocomplete(const std::u16string& input,
                         bool prevent_inline_autocomplete) override;
  void DeleteAutocompleteMatch(uint8_t line, const GURL& url) override {}
  void ExecuteAction(uint8_t line,
                     uint8_t action_index,
                     const GURL& url,
                     base::TimeTicks match_selection_timestamp,
                     uint8_t mouse_button,
                     bool alt_key,
                     bool ctrl_key,
                     bool meta_key,
                     bool shift_key) override {}
  void PopupElementSizeChanged(const gfx::Size& size) override {}
  void OnThumbnailRemoved() override;

  // Invoked by LensSearchboxController.
  void SetInputText(const std::string& input_text);
  // Invoked by LensSearchboxController.
  void SetThumbnail(const std::string& thumbnail_url, bool is_deletable);

  // AutocompleteController::Observer:
  void OnAutocompleteStopTimerTriggered(
      const AutocompleteInput& input) override;
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

 private:
  // Owns this.
  raw_ptr<LensSearchboxClient> lens_searchbox_client_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_LENS_SEARCHBOX_HANDLER_H_
