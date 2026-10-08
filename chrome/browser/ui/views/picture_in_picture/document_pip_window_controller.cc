// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_window_controller.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_base_window.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"
#include "extensions/common/extension.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {

DocumentPipWindowController::DocumentPipWindowController(
    DocumentPipHost& host,
    std::unique_ptr<DocumentPipBaseWindow> base_window,
    SessionID session_id)
    : WindowController(base_window.get(), host.GetProfile()),
      host_(host),
      base_window_(std::move(base_window)),
      session_id_(session_id) {
  CHECK(base_window_);
  CHECK(session_id_.is_valid());
  CHECK(host_->GetChildWebContents());
  WindowControllerList::GetInstance()->AddExtensionWindow(this);
}

DocumentPipWindowController::~DocumentPipWindowController() {
  WindowControllerList::GetInstance()->RemoveExtensionWindow(this);
}

int DocumentPipWindowController::GetWindowId() const {
  return session_id_.id();
}

std::string DocumentPipWindowController::GetWindowTypeText() const {
  return api::tabs::ToString(api::tabs::WindowType::kPopup);
}

void DocumentPipWindowController::SetFullscreenMode(
    bool is_fullscreen,
    const GURL& extension_url) const {
  // Document PiP does not support extension-triggered fullscreen.
}

content::WebContents* DocumentPipWindowController::GetActiveTab() const {
  return host_->GetChildWebContents();
}

int DocumentPipWindowController::GetTabCount() const {
  return GetActiveTab() ? 1 : 0;
}

content::WebContents* DocumentPipWindowController::GetWebContentsAt(
    int i) const {
  return i == 0 ? GetActiveTab() : nullptr;
}

bool DocumentPipWindowController::IsVisibleToTabsAPIForExtension(
    const Extension* extension,
    bool include_dev_tools_windows) const {
  return !extension || !extension->is_platform_app();
}

base::DictValue DocumentPipWindowController::CreateWindowValueForExtension(
    const Extension* extension,
    PopulateTabBehavior populate_tab_behavior,
    mojom::ContextType context) const {
  const gfx::Rect bounds = base_window_->IsMinimized()
                               ? base_window_->GetRestoredBounds()
                               : base_window_->GetBounds();
  base::DictValue dict;
  dict.Set("id", GetWindowId());
  dict.Set("focused", base_window_->IsActive());
  dict.Set("top", bounds.y());
  dict.Set("left", bounds.x());
  dict.Set("width", bounds.width());
  dict.Set("height", bounds.height());
  dict.Set("type", GetWindowTypeText());
  dict.Set("state", base_window_->IsMinimized() ? "minimized" : "normal");
  dict.Set("incognito", profile()->IsOffTheRecord());
  dict.Set("alwaysOnTop",
           base_window_->GetZOrderLevel() == ui::ZOrderLevel::kFloatingWindow);
  if (populate_tab_behavior == kPopulateTabs) {
    dict.Set(ExtensionTabUtil::kTabsKey, CreateTabList(extension, context));
  }
  return dict;
}

base::ListValue DocumentPipWindowController::CreateTabList(
    const Extension* extension,
    mojom::ContextType context) const {
  base::ListValue tabs;
  if (content::WebContents* child = GetActiveTab()) {
    auto tab = ExtensionTabUtil::CreateTabObject(
        child, ExtensionTabUtil::GetScrubTabBehavior(extension, context, child),
        extension, nullptr, 0);
    tab.active = true;
    tab.selected = true;
    tab.highlighted = true;
    tabs.Append(tab.ToValue());
  }
  return tabs;
}

bool DocumentPipWindowController::OpenOptionsPage(const Extension* extension,
                                                  const GURL& url,
                                                  bool open_in_tab) {
  BrowserWindowInterface* browser =
      ProfileBrowserCollection::GetForProfile(profile())->FindTabbedBrowser();
  return browser &&
         BrowserExtensionWindowController::From(browser)->OpenOptionsPage(
             extension, url, open_in_tab);
}

}  // namespace extensions
