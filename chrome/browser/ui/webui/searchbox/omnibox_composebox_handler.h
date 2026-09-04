// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_OMNIBOX_COMPOSEBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_OMNIBOX_COMPOSEBOX_HANDLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/prefs/pref_change_registrar.h"

class OmniboxController;
class Profile;

namespace content {
class WebContents;
}  // namespace content

// ComposeboxHandler for the Omnibox Popup.
class OmniboxComposeboxHandler : public ComposeboxHandler {
  friend class OmniboxComposeboxHandlerTest;

 public:
  OmniboxComposeboxHandler(
      mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler,
      mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
      Profile* profile,
      content::WebContents* web_contents,
      GetSessionHandleCallback get_session_callback,
      ClearSessionHandleCallback clear_session_callback);

  ~OmniboxComposeboxHandler() override;

  // composebox::mojom::PageHandler:
  void HandleFileUpload(bool is_image) override;

  // searchbox::mojom::PageHandler:
  void OpenLensSearch() override;

 protected:
  // ComposeboxHandler:
  void ProcessContextAndOpenUrl(
      GURL url,
      const WindowOpenDisposition disposition) override;

 private:
  void OnContentSharingPolicyChanged();
  void OnPopupStateChanged(OmniboxPopupState old_state,
                           OmniboxPopupState new_state);
  void UpdateLensSearchEligibility(const AutocompleteInput& input,
                                   AutocompleteProviderClient* client);

  base::CallbackListSubscription popup_state_subscription_;
  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<OmniboxComposeboxHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_OMNIBOX_COMPOSEBOX_HANDLER_H_
