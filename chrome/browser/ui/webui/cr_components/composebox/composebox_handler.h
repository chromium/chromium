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

class ComposeboxHandler : public composebox::mojom::PageHandler,
                          public ContextualSearchboxHandler {
 public:
  using ClearSessionHandleCallback = base::RepeatingClosure;

  explicit ComposeboxHandler(
      mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler,
      mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
      Profile* profile,
      content::WebContents* web_contents,
      GetSessionHandleCallback get_session_callback,
      ClearSessionHandleCallback clear_session_callback);
  ~ComposeboxHandler() override;

  // composebox::mojom::PageHandler:
  void FocusChanged(bool focused) override;
  void StartPlatformVoiceRecognition() override;
  void HandleLensButtonClick() override;
  void HandleFileUpload(bool is_image) override;
  void NavigateUrl(const GURL& url) override;
  void CloseLensOverlayFromWebUI(
      composebox::mojom::LensOverlayDismissalSource dismissal_source) override;
  void SetSmartTabSharingActive(bool active) override;
  void GetSmartTabSharingActive(
      GetSmartTabSharingActiveCallback callback) override;
  void OnContextMenuOpened() override;
  void NotifyComposeboxQuerySubmittedWithContext() override;

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
                   bool shift_key,
                   bool is_voice_search) override;
  void ClearFiles(bool should_block_auto_suggested_tabs) override;

  // This is called from either the ComposeboxOmniboxClient when a match is
  // present in navigation or for the PageHandler's `SubmitQuery()` when there
  // was no match present. The latter only happens when submit is clicked with
  // only a file and no input.
  // If there is a match present in navigation, `additional_params` from the
  // match's `detination_url` will be appended during url creation.
  void SubmitQuery(const std::string& query_text,
                   WindowOpenDisposition disposition,
                   omnibox::ChromeAimEntryPoint aim_entrypoint,
                   std::map<std::string, std::string> additional_params,
                   bool is_voice_search);

  virtual void ClearSessionHandle();

 protected:
  void OpenUrl(GURL url, const WindowOpenDisposition disposition) override;

  FRIEND_TEST_ALL_PREFIXES(ComposeboxHandlerTest,
                           OpenUrl_ResetsContextControllerObserver);
  FRIEND_TEST_ALL_PREFIXES(ComposeboxHandlerTest, SetSmartTabSharingEnabled);
  FRIEND_TEST_ALL_PREFIXES(ComposeboxHandlerTest,
                           SetSmartTabSharingEnabled_FeatureDisabled);

 protected:
  ComposeboxHandler(
      mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler,
      mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
      Profile* profile,
      content::WebContents* web_contents,
      std::unique_ptr<OmniboxController> omnibox_controller,
      GetSessionHandleCallback get_session_callback,
      ClearSessionHandleCallback clear_session_callback);

 private:
  ClearSessionHandleCallback clear_session_callback_;

  // These are located at the end of the list of member variables to ensure the
  // WebUI page is disconnected before other members are destroyed.
  mojo::Remote<composebox::mojom::Page> page_;
  mojo::Receiver<composebox::mojom::PageHandler> handler_;
};
#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_COMPOSEBOX_COMPOSEBOX_HANDLER_H_
