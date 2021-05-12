// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_REALBOX_REALBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_REALBOX_REALBOX_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"
#include "chrome/browser/ui/webui/realbox/realbox.mojom.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Image;
}  // namespace gfx

// Handles bidirectional communication between NTP realbox JS and the browser.
class RealboxHandler : public realbox::mojom::PageHandler,
                       public AutocompleteController::Observer {
 public:
  RealboxHandler(
      mojo::PendingReceiver<realbox::mojom::PageHandler> pending_page_handler,
      Profile* profile,
      content::WebContents* web_contents);
  ~RealboxHandler() override;

  // realbox::mojom::PageHandler:
  void SetPage(mojo::PendingRemote<realbox::mojom::Page> pending_page) override;
  void QueryAutocomplete(const std::u16string& input,
                         bool prevent_inline_autocomplete) override;
  void StopAutocomplete(bool clear_result) override;
  void OpenAutocompleteMatch(uint8_t line,
                             const GURL& url,
                             bool are_matches_showing,
                             base::TimeDelta time_elapsed_since_last_focus,
                             uint8_t mouse_button,
                             bool alt_key,
                             bool ctrl_key,
                             bool meta_key,
                             bool shift_key) override;
  void DeleteAutocompleteMatch(uint8_t line) override;
  void ToggleSuggestionGroupIdVisibility(int32_t suggestion_group_id) override;
  void LogCharTypedToRepaintLatency(base::TimeDelta latency) override;

  // AutocompleteController::Observer:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

  void OnRealboxBitmapFetched(int match_index,
                              const GURL& image_url,
                              const SkBitmap& bitmap);
  void OnRealboxFaviconFetched(int match_index,
                               const GURL& page_url,
                               const gfx::Image& favicon);

 private:
  Profile* profile_;
  content::WebContents* web_contents_;
  std::unique_ptr<AutocompleteController> autocomplete_controller_;
  BitmapFetcherService* bitmap_fetcher_service_;
  std::vector<BitmapFetcherService::RequestId> bitmap_request_ids_;
  FaviconCache favicon_cache_;
  base::TimeTicks time_user_first_modified_realbox_;

  mojo::Remote<realbox::mojom::Page> page_;
  mojo::Receiver<realbox::mojom::PageHandler> page_handler_;

  base::WeakPtrFactory<RealboxHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RealboxHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_REALBOX_REALBOX_HANDLER_H_
