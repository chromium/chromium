// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INTERACTION_BROWSER_ELEMENTS_VIEWS_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_INTERACTION_BROWSER_ELEMENTS_VIEWS_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/views/view.h"

// Implementation of BrowserElementsViews for non-Webium (i.e. normal) browsers.
// This class should never be referenced outside of BrowserWindowFeatures.
class BrowserElementsViewsImpl : public BrowserElementsViews {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  explicit BrowserElementsViewsImpl(BrowserWindowInterface& browser);
  ~BrowserElementsViewsImpl() override;

  // Initializes with a view that provides context (typically a BrowserView, but
  // no need to inject a dependency here).
  void Init(views::View* context_view);

 private:
  // BrowserElementsViews:
  ui::ElementContext GetContext() override;
  views::Widget* GetPrimaryWindowWidget() override;
  void TearDown() override;
  bool IsInitialized() const override;

  raw_ptr<views::View> context_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_INTERACTION_BROWSER_ELEMENTS_VIEWS_IMPL_H_
