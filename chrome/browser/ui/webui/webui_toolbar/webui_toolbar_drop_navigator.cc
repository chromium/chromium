// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_drop_navigator.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_drag_state.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/common/referrer.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace webui_toolbar {

WebUIToolbarDropNavigator::WebUIToolbarDropNavigator(
    BrowserWindowInterface& browser,
    content::WebContents* toolbar_contents)
    : content::WebContentsObserver(toolbar_contents), browser_(browser) {}

WebUIToolbarDropNavigator::~WebUIToolbarDropNavigator() = default;

void WebUIToolbarDropNavigator::NavigateToDroppedUrl(const GURL& url) {
  const bool drag_originated_from_renderer =
      GetDragOriginatedFromRendererAndReset();

  // Disallow javascript: URLs to prevent self-XSS.
  if (url.SchemeIs(url::kJavaScriptScheme)) {
    return;
  }

  // If the drag originated from a renderer (web page), only allow safe schemes
  // (HTTP, HTTPS, file) to match native UI drag-and-drop navigation security.
  if (drag_originated_from_renderer && !url.SchemeIsHTTPOrHTTPS() &&
      !url.SchemeIs(url::kFileScheme)) {
    return;
  }

  OpenDroppedUrl(url, drag_originated_from_renderer);
}

void WebUIToolbarDropNavigator::NavigateToDroppedText(const std::string& text) {
  const bool drag_originated_from_renderer =
      GetDragOriginatedFromRendererAndReset();

  std::u16string text_u16 = base::UTF8ToUTF16(text);
  std::u16string sanitized_text = AutocompleteInput::SanitizeString(text_u16);

  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(browser_->GetProfile())
      ->Classify(sanitized_text, false, false,
                 metrics::OmniboxEventProto::INVALID_SPEC, &match, nullptr);

  if (!match.destination_url.is_valid()) {
    return;
  }

  // Disallow javascript: URLs to prevent self-XSS.
  if (match.destination_url.SchemeIs(url::kJavaScriptScheme)) {
    return;
  }

  // For text drops, enforce stricter filtering for renderer-originated drags.
  // Only allow HTTP/HTTPS to prevent web pages from forcing navigation to local
  // system files (file://) or other unsafe URLs by tricking the user into
  // dragging plain text.
  if (drag_originated_from_renderer &&
      !match.destination_url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  OpenDroppedUrl(match.destination_url, drag_originated_from_renderer);
}

bool WebUIToolbarDropNavigator::GetDragOriginatedFromRendererAndReset() {
  return WebUIToolbarDragState::TakeDragOriginatedFromRenderer(web_contents());
}

void WebUIToolbarDropNavigator::OpenDroppedUrl(
    const GURL& url,
    bool drag_originated_from_renderer) {
  content::OpenURLParams params(url, content::Referrer(),
                                ui::mojom::WindowOpenDisposition::kCurrentTab,
                                ui::PAGE_TRANSITION_LINK,
                                /*is_renderer_initiated=*/false);
  // A drag that started in a web page carries data controlled by that page, so
  // the navigation must not be treated as browser-initiated. Otherwise it
  // would be sent with `Sec-Fetch-Site: none` and SameSite=Strict cookies.
  // `content::DropData` does not carry the page origin, so an opaque origin is
  // used instead.
  if (drag_originated_from_renderer) {
    params.initiator_origin = url::Origin();
  }
  browser_->OpenURL(params, /*navigation_handle_callback=*/{});
}

}  // namespace webui_toolbar
