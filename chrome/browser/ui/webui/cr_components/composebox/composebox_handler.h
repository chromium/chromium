// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_COMPOSEBOX_COMPOSEBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_COMPOSEBOX_COMPOSEBOX_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"
#include "url/gurl.h"

class Profile;
class OmniboxController;

class TopChromeWebUIController;

class ComposeboxHandler : public composebox::mojom::PageHandler,
                          public ContextualSearchboxHandler {
 public:
  explicit ComposeboxHandler(
      mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler,
      Profile* profile,
      content::WebContents* web_contents);
  ~ComposeboxHandler() override;

  void SetEmbedder(base::WeakPtr<TopChromeWebUIController::Embedder> embedder) {
    embedder_ = embedder;
  }

  // composebox::mojom::PageHandler:
  void FocusChanged(bool focused) override;
  void SetDeepSearchMode(bool enabled) override;
  void SetCreateImageMode(bool enabled, bool image_present) override;

  void HandleLensButtonClick() override;

  // searchbox::mojom::PageHandler:
  void ExecuteAction(uint8_t line,
                     uint8_t action_index,
                     const GURL& url,
                     base::TimeTicks match_selection_timestamp,
                     uint8_t mouse_button,
                     bool alt_key,
                     bool ctrl_key,
                     bool meta_key,
                     bool shift_key) override;
  void OnThumbnailRemoved() override;
  void SubmitQuery(const std::string& query_text,
                   uint8_t mouse_button,
                   bool alt_key,
                   bool ctrl_key,
                   bool meta_key,
                   bool shift_key) override;
  void ClearFiles() override;
  void ShowContextMenu(const gfx::Point& point) override;

  // This is called from either the ComposeboxOmniboxClient when a match is
  // present in navigation or for the PageHandler's `SubmitQuery()` when there
  // was no match present. The latter only happens when submit is clicked with
  // only a file and no input.
  // If there is a match present in navigation, `additional_params` from the
  // match's `detination_url` will be appended during url creation.
  void SubmitQuery(const std::string& query_text,
                   WindowOpenDisposition disposition,
                   omnibox::ChromeAimEntryPoint aim_entrypoint,
                   std::map<std::string, std::string> additional_params);

  omnibox::ChromeAimToolsAndModels GetAimToolMode() override;

  // Called to update the suggested tab context chip in the compose box.
  virtual void UpdateSuggestedTabContext(searchbox::mojom::TabInfoPtr tab_info);

 protected:
  ComposeboxHandler(
      mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler,
      Profile* profile,
      content::WebContents* web_contents,
      std::unique_ptr<OmniboxController> omnibox_controller);

 private:
  // The tool mode for the composebox, if any. These tool modes are disjoint
  // and it's only possible for one mode to be set at one time.
  omnibox::ChromeAimToolsAndModels aim_tool_mode_ =
      omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED;
  raw_ptr<content::WebContents> web_contents_;
  base::WeakPtr<TopChromeWebUIController::Embedder> embedder_;

  // These are located at the end of the list of member variables to ensure the
  // WebUI page is disconnected before other members are destroyed.
  mojo::Remote<composebox::mojom::Page> page_;
  mojo::Receiver<composebox::mojom::PageHandler> handler_;
};
#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_COMPOSEBOX_COMPOSEBOX_HANDLER_H_
