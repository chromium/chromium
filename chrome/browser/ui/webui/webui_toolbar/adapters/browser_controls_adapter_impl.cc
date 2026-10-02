// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/adapters/browser_controls_adapter_impl.h"

#include "base/check_deref.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/split_tabs_utils.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/toolbar_button_utils.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_drag_state.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace browser_controls_api {

BrowserControlsAdapterImpl::BrowserControlsAdapterImpl(
    BrowserWindowInterface* browser_interface,
    CommandUpdater* command_updater,
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      browser_(CHECK_DEREF(browser_interface)),
      command_updater_(CHECK_DEREF(command_updater)) {}

BrowserControlsAdapterImpl::~BrowserControlsAdapterImpl() {}

void BrowserControlsAdapterImpl::Reload(bool bypass_cache,
                                        WindowOpenDisposition disposition) {
  command_updater_->ExecuteCommandWithDisposition(
      bypass_cache ? IDC_RELOAD_BYPASSING_CACHE : IDC_RELOAD, disposition);
}

void BrowserControlsAdapterImpl::Stop() {
  command_updater_->ExecuteCommandWithDisposition(
      IDC_STOP, WindowOpenDisposition::CURRENT_TAB);
}

void BrowserControlsAdapterImpl::Back(WindowOpenDisposition disposition) {
  command_updater_->ExecuteCommandWithDisposition(IDC_BACK, disposition);
}

void BrowserControlsAdapterImpl::Forward(WindowOpenDisposition disposition) {
  command_updater_->ExecuteCommandWithDisposition(IDC_FORWARD, disposition);
}

void BrowserControlsAdapterImpl::BackButtonHovered() {
  auto* const active_tab = browser_.get().GetActiveTabInterface();
  if (!active_tab) {
    return;
  }
  auto* const contents = active_tab->GetContents();
  if (!contents) {
    return;
  }
  contents->BackNavigationLikely(chrome_preloading_predictor::kBackButtonHover,
                                 WindowOpenDisposition::CURRENT_TAB);
}

void BrowserControlsAdapterImpl::CreateNewSplitTab() {
  chrome::NewSplitTab(&browser_.get(), split_tabs::SplitTabLayout::kSideBySide,
                      split_tabs::SplitTabCreatedSource::kToolbarButton);
}

void BrowserControlsAdapterImpl::NavigateHome(
    WindowOpenDisposition disposition) {
  command_updater_->ExecuteCommandWithDisposition(IDC_HOME, disposition);
}

void BrowserControlsAdapterImpl::Navigate(const GURL& url) {
  const bool drag_has_javascript_url = GetDragHasJavaScriptUrlAndReset();
  const bool drag_originated_from_renderer =
      GetDragOriginatedFromRendererAndReset();

  // Disallow javascript: URLs to prevent self-XSS. The unfiltered drag data is
  // also consulted, since `FilterDropData` may have rewritten the URL.
  if (url.SchemeIs(url::kJavaScriptScheme) || drag_has_javascript_url) {
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

void BrowserControlsAdapterImpl::NavigateText(const std::string& text) {
  // Text drops are checked by the scheme of the classified URL below, so the
  // unfiltered drag URL state is only cleared here so that it does not leak
  // into a later call.
  GetDragHasJavaScriptUrlAndReset();
  const bool drag_originated_from_renderer =
      GetDragOriginatedFromRendererAndReset();

  std::u16string text_u16 = base::UTF8ToUTF16(text);
  std::u16string sanitized_text = AutocompleteInput::SanitizeString(text_u16);

  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(browser_.get().GetProfile())
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

webui_toolbar::TabSplitStatus
BrowserControlsAdapterImpl::ComputeSplitTabStatus() {
  return webui_toolbar::ComputeTabSplitStatus(&browser_.get());
}

bool BrowserControlsAdapterImpl::GetDragOriginatedFromRendererAndReset() {
  return webui_toolbar::WebUIToolbarDragState::TakeDragOriginatedFromRenderer(
      web_contents());
}

bool BrowserControlsAdapterImpl::GetDragHasJavaScriptUrlAndReset() {
  return webui_toolbar::WebUIToolbarDragState::TakeDragHasJavaScriptUrl(
      web_contents());
}

void BrowserControlsAdapterImpl::OpenDroppedUrl(
    const GURL& url,
    bool drag_originated_from_renderer) {
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
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
  browser_.get().OpenURL(params, /*navigation_handle_callback=*/{});
}

}  // namespace browser_controls_api
