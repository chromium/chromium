// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_LENS_LENS_COMPOSEBOX_HANDLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_COMPOSEBOX_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"
#include "url/gurl.h"

class MetricsReporter;
class Profile;

namespace lens {

class LensComposeboxController;

class LensComposeboxHandler : public composebox::mojom::PageHandler,
                              public SearchboxHandler {
 public:
  explicit LensComposeboxHandler(
      lens::LensComposeboxController* parent_controller,
      Profile* profile,
      content::WebContents* web_contents,
      mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler);
  ~LensComposeboxHandler() override;

  // composebox::mojom::PageHandler:
  void SubmitQuery(const std::string& query_text,
                   uint8_t mouse_button,
                   bool alt_key,
                   bool ctrl_key,
                   bool meta_key,
                   bool shift_key) override;
  void FocusChanged(bool focused) override;
  void SetDeepSearchMode(bool enabled) override;
  void SetCreateImageMode(bool enabled, bool image_present) override;
  void HandleLensButtonClick() override;

  // searchbox::mojom::PageHandler:
  void DeleteAutocompleteMatch(uint8_t line, const GURL& url) override;
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
  void DeleteContext(const base::UnguessableToken& file_token) override;
  void ClearFiles() override;

 private:
  // Owns this.
  const raw_ptr<lens::LensComposeboxController> lens_composebox_controller_;

  // These are located at the end of the list of member variables to ensure the
  // WebUI page is disconnected before other members are destroyed.
  mojo::Remote<composebox::mojom::Page> page_;
  mojo::Receiver<composebox::mojom::PageHandler> handler_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_COMPOSEBOX_HANDLER_H_
