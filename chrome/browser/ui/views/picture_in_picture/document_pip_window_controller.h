// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_WINDOW_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "chrome/browser/extensions/window_controller.h"
#include "components/sessions/core/session_id.h"

class DocumentPipBaseWindow;
class DocumentPipHost;

namespace extensions {

// Presents a standalone PiP window without a Browser or tab strip.
class DocumentPipWindowController : public WindowController {
 public:
  DocumentPipWindowController(
      DocumentPipHost& host,
      std::unique_ptr<DocumentPipBaseWindow> base_window,
      SessionID session_id);
  DocumentPipWindowController(const DocumentPipWindowController&) = delete;
  DocumentPipWindowController& operator=(const DocumentPipWindowController&) =
      delete;
  ~DocumentPipWindowController() override;

  // WindowController:
  int GetWindowId() const override;
  std::string GetWindowTypeText() const override;
  void SetFullscreenMode(bool is_fullscreen,
                         const GURL& extension_url) const override;
  content::WebContents* GetActiveTab() const override;
  int GetTabCount() const override;
  content::WebContents* GetWebContentsAt(int i) const override;
  bool IsVisibleToTabsAPIForExtension(
      const Extension* extension,
      bool include_dev_tools_windows) const override;
  base::DictValue CreateWindowValueForExtension(
      const Extension* extension,
      PopulateTabBehavior populate_tab_behavior,
      mojom::ContextType context) const override;
  base::ListValue CreateTabList(const Extension* extension,
                                mojom::ContextType context) const override;
  bool OpenOptionsPage(const Extension* extension,
                       const GURL& url,
                       bool open_in_tab) override;

 private:
  const raw_ref<DocumentPipHost> host_;
  const std::unique_ptr<DocumentPipBaseWindow> base_window_;
  const SessionID session_id_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_WINDOW_CONTROLLER_H_
