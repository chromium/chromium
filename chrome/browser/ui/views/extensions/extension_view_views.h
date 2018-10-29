// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_VIEW_VIEWS_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/extensions/extension_view.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"

class Browser;

namespace content {
class RenderViewHost;
}

namespace extensions {
class ExtensionHost;
}

// This handles the display portion of an ExtensionHost.
class ExtensionViewViews : public views::WebView,
                           public extensions::ExtensionView {
 public:
  // A class that represents the container that this view is in.
  // (bottom shelf, side bar, etc.)
  class Container {
   public:
    virtual ~Container() {}

    virtual void OnExtensionSizeChanged(ExtensionViewViews* view) {}
  };

  ExtensionViewViews(extensions::ExtensionHost* host, Browser* browser);
  ~ExtensionViewViews() override;

  // extensions::ExtensionView:
  Browser* GetBrowser() override;

  // views::WebView:
  void VisibilityChanged(View* starting_from, bool is_visible) override;

  void set_minimum_size(const gfx::Size& minimum_size) {
    minimum_size_ = minimum_size;
  }
  void set_container(Container* container) { container_ = container; }

 private:
  friend class extensions::ExtensionHost;

  // extensions::ExtensionView:
  gfx::NativeView GetNativeView() override;
  void ResizeDueToAutoResize(content::WebContents* web_contents,
                             const gfx::Size& new_size) override;
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  void OnLoaded() override;

  // views::WebView:
  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override;
  gfx::Size GetMinimumSize() const override;
  void PreferredSizeChanged() override;
  void OnWebContentsAttached() override;

  // Note that host_ owns view
  extensions::ExtensionHost* host_;

  // The browser window that this view is in.
  Browser* const browser_;

  // What we should set the preferred width to once the ExtensionViewViews has
  // loaded.
  gfx::Size pending_preferred_size_;
  gfx::Size minimum_size_;

  // The container this view is in (not necessarily its direct superview).
  // Note: the view does not own its container.
  Container* container_;

  // A handler to handle unhandled keyboard messages coming back from the
  // renderer process.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionViewViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_VIEW_VIEWS_H_
