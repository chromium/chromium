// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/lens/lens_composebox_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/lens/lens_composebox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_omnibox_client.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_handler.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"
#include "url/gurl.h"

namespace lens {

LensComposeboxHandler::LensComposeboxHandler(
    lens::LensComposeboxController* parent_controller,
    Profile* profile,
    content::WebContents* web_contents,
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler)
    : SearchboxHandler(
          std::move(pending_searchbox_handler),
          profile,
          web_contents,
          std::make_unique<OmniboxController>(
              /*view=*/nullptr,
              std::make_unique<composebox::ComposeboxOmniboxClient>(
                  profile,
                  web_contents,
                  this,
                  /*query_controller=*/nullptr))),
      lens_composebox_controller_(parent_controller),
      page_{std::move(pending_page)},
      handler_(this, std::move(pending_handler)) {
  autocomplete_controller_observation_.Observe(autocomplete_controller());
}

LensComposeboxHandler::~LensComposeboxHandler() = default;

void LensComposeboxHandler::SubmitQuery(
    const std::string& query_text,
    WindowOpenDisposition disposition,
    std::map<std::string, std::string> additional_params) {
  lens_composebox_controller_->IssueComposeboxQuery(query_text);
}

void LensComposeboxHandler::SubmitQuery(const std::string& query_text,
                                        uint8_t mouse_button,
                                        bool alt_key,
                                        bool ctrl_key,
                                        bool meta_key,
                                        bool shift_key) {
  SubmitQuery(query_text,
              ui::DispositionFromClick(
                  /*middle_button=*/mouse_button == 1, alt_key, ctrl_key,
                  meta_key, shift_key),
              /*additional_params=*/{});
}

void LensComposeboxHandler::FocusChanged(bool focused) {
  lens_composebox_controller_->OnFocusChanged(focused);
}

void LensComposeboxHandler::SetDeepSearchMode(bool enabled) {
  // Ignore, intentionally unimplemented for Lens. Deep search not implemented
  // in Lens.
}

void LensComposeboxHandler::SetCreateImageMode(bool enabled) {
  // Ignore, intentionally unimplemented for Lens. Create image not implemented
  // in Lens.
}

void LensComposeboxHandler::HandleLensButtonClick() {
  lens_composebox_controller_->ShowLensSelectionOverlay();
}

void LensComposeboxHandler::DeleteAutocompleteMatch(uint8_t line,
                                                    const GURL& url) {
  NOTREACHED();
}

void LensComposeboxHandler::ExecuteAction(
    uint8_t line,
    uint8_t action_index,
    const GURL& url,
    base::TimeTicks match_selection_timestamp,
    uint8_t mouse_button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key) {
  NOTREACHED();
}

void LensComposeboxHandler::OnThumbnailRemoved() {
  NOTREACHED();
}

}  // namespace lens
