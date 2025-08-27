// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser_side_panel_ui.h"

#include <optional>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"

WebUIBrowserSidePanelUI::WebUIBrowserSidePanelUI(Browser* browser)
    : SidePanelUIBase(browser) {}

WebUIBrowserSidePanelUI::~WebUIBrowserSidePanelUI() = default;

void WebUIBrowserSidePanelUI::Close() {}

void WebUIBrowserSidePanelUI::Toggle(SidePanelEntryKey key,
                                     SidePanelOpenTrigger open_trigger) {}

void WebUIBrowserSidePanelUI::OpenInNewTab() {}

void WebUIBrowserSidePanelUI::UpdatePinState() {}

content::WebContents* WebUIBrowserSidePanelUI::GetWebContentsForTest(
    SidePanelEntryId id) {
  return nullptr;
}

void WebUIBrowserSidePanelUI::DisableAnimationsForTesting() {}

void WebUIBrowserSidePanelUI::SetNoDelaysForTesting(
    bool no_delays_for_testing) {}

void WebUIBrowserSidePanelUI::Close(bool suppress_animations) {}

void WebUIBrowserSidePanelUI::Show(
    const UniqueKey& input,
    std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
    bool suppress_animations) {}

void WebUIBrowserSidePanelUI::PopulateSidePanel(
    bool suppress_animations,
    const UniqueKey& unique_key,
    std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
    SidePanelEntry* entry,
    std::optional<std::unique_ptr<views::View>> content_view) {}

void WebUIBrowserSidePanelUI::MaybeShowEntryOnTabStripModelChanged(
    SidePanelRegistry* old_contextual_registry,
    SidePanelRegistry* new_contextual_registry) {}

WebUIBrowserWindow* WebUIBrowserSidePanelUI::GetWebUIBrowserWindow() {
  return static_cast<WebUIBrowserWindow*>(browser()->window());
}
