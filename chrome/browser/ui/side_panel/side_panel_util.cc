// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/side_panel_util.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "ui/base/class_property.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/views/side_panel/side_panel_helper.h"
#include "ui/actions/actions.h"
#endif

// static
SidePanelContentProxy* SidePanelUtil::GetSidePanelContentProxy(
    ui::PropertyHandler* content_view) {
  if (!content_view->GetProperty(kSidePanelContentProxyKey)) {
    content_view->SetProperty(
        kSidePanelContentProxyKey,
        std::make_unique<SidePanelContentProxy>(true).release());
  }
  return content_view->GetProperty(kSidePanelContentProxyKey);
}

// static
std::u16string_view SidePanelUtil::GetTitleText(
    SidePanelEntry* entry,
    BrowserWindowInterface* browser) {
#if BUILDFLAG(IS_ANDROID)
  auto* title = entry->GetProperty(kSidePanelTitleKey);
  if (title) {
    return *title;
  }
#else
  if (entry->GetProperty(kShouldShowTitleInSidePanelHeaderKey)) {
    return SidePanelHelper::GetActionItem(browser, entry->key())->GetText();
  }
#endif
  return std::u16string_view();
}
