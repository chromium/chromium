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
#include "content/public/browser/web_contents.h"

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
  bool drag_originated_from_renderer = GetDragOriginatedFromRendererAndReset();

  // If the drag originated from a renderer (web page), only allow safe schemes
  // (HTTP, HTTPS, file). Block and redirect other schemes (e.g., chrome://) to
  // about:blank#blocked to match native UI drag-and-drop navigation security.
  if (drag_originated_from_renderer) {
    if (!url.SchemeIsHTTPOrHTTPS() && !url.SchemeIs(url::kFileScheme)) {
      browser_.get().OpenGURL(GURL("about:blank#blocked"),
                              WindowOpenDisposition::CURRENT_TAB);
      return;
    }
  } else {
    // If initiated locally (e.g., OS file manager drop), allow most schemes
    // but block javascript: URLs to prevent self-XSS.
    if (url.SchemeIs(url::kJavaScriptScheme)) {
      return;
    }
  }

  browser_.get().OpenGURL(url, WindowOpenDisposition::CURRENT_TAB);
}

void BrowserControlsAdapterImpl::NavigateText(const std::string& text) {
  std::u16string text_u16 = base::UTF8ToUTF16(text);
  std::u16string sanitized_text = AutocompleteInput::SanitizeString(text_u16);

  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(browser_.get().GetProfile())
      ->Classify(sanitized_text, false, false,
                 metrics::OmniboxEventProto::INVALID_SPEC, &match, nullptr);

  if (!match.destination_url.is_valid()) {
    return;
  }

  bool drag_originated_from_renderer = GetDragOriginatedFromRendererAndReset();

  // For text drops, enforce stricter filtering for renderer-originated drags.
  // Only allow HTTP/HTTPS to prevent web pages from forcing navigation to local
  // system files (file://) or other unsafe URLs by tricking the user into
  // dragging plain text.
  if (drag_originated_from_renderer) {
    if (!match.destination_url.SchemeIsHTTPOrHTTPS()) {
      return;
    }
  } else {
    // For local text drops (e.g., notepad), allow all URL schemes except
    // javascript.
    if (match.destination_url.SchemeIs(url::kJavaScriptScheme)) {
      return;
    }
  }

  browser_.get().OpenGURL(match.destination_url,
                          WindowOpenDisposition::CURRENT_TAB);
}

webui_toolbar::TabSplitStatus
BrowserControlsAdapterImpl::ComputeSplitTabStatus() {
  return webui_toolbar::ComputeTabSplitStatus(&browser_.get());
}

bool BrowserControlsAdapterImpl::GetDragOriginatedFromRendererAndReset() {
  if (!web_contents()) {
    return false;
  }
  auto* drag_state =
      webui_toolbar::WebUIToolbarDragState::FromWebContents(web_contents());

  if (!drag_state) {
    return false;
  }

  bool val = drag_state->drag_originated_from_renderer();
  // Clear the drag state immediately after reading to ensure that the
  // "renderer-tainted" drag state does not persist and pollute subsequent
  // generic non-drag navigations (e.g. if the WebUI triggers
  // Navigate/NavigateText outside a drag transaction).
  drag_state->set_drag_originated_from_renderer(false);
  return val;
}

}  // namespace browser_controls_api
