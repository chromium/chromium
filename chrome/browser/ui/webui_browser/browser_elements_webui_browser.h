// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMIUM_BROWSER_UI_WEBUI_BROWSER_BROWSER_ELEMENTS_WEBUI_BROWSER_H_
#define CHROMIUM_BROWSER_UI_WEBUI_BROWSER_BROWSER_ELEMENTS_WEBUI_BROWSER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/framework_specific_implementation.h"

namespace views {
class Widget;
}  // namespace views

class BrowserWindowInterface;

// Provides WebUIBrowser-specific extensions to `BrowserElements` so it can
// provide a context and elements for the WebUIBrowser.
class BrowserElementsWebUiBrowser : public BrowserElementsViews {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  explicit BrowserElementsWebUiBrowser(BrowserWindowInterface& browser);
  ~BrowserElementsWebUiBrowser() override;

  static BrowserElementsWebUiBrowser* From(BrowserWindowInterface* browser);

  // Initializes with the browser widget. This widget is used as the context
  // to track elements in this WebUIBrowser.
  void Init(views::Widget* browser_widget);

 private:
  // BrowserElements:
  ui::ElementContext GetContext() override;
  views::Widget* GetPrimaryWindowWidget() override;
  void TearDown() override;
  bool IsInitialized() const override;

  raw_ptr<views::Widget> browser_widget_ = nullptr;
};

#endif  // CHROMIUM_BROWSER_UI_WEBUI_BROWSER_BROWSER_ELEMENTS_WEBUI_BROWSER_H_
