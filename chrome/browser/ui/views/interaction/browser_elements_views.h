// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INTERACTION_BROWSER_ELEMENTS_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_INTERACTION_BROWSER_ELEMENTS_VIEWS_H_

#include <concepts>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"

// Provides Views-specific extensions to `BrowserElements` so it can
// provide a context, elements, and Views.
class BrowserElementsViews : public BrowserElements {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  explicit BrowserElementsViews(BrowserWindowInterface& browser);
  ~BrowserElementsViews() override;

  static BrowserElementsViews* From(BrowserWindowInterface* browser);

  // Initializes with a view that provides context (typically a BrowserView, but
  // no need to inject a dependency here).
  void Init(views::View* context_view);

  // Call this when the hosting context is going away.
  void TearDown();

  // BrowserElements:
  ui::ElementContext GetContext() override;

  // These provide convenience access to ElementTrackerViews without having to
  // specify context:

  using ViewList = views::ElementTrackerViews::ViewList;
  views::View* GetView(ui::ElementIdentifier id);
  ViewList GetAllViews(ui::ElementIdentifier id);

  template <typename T>
    requires std::derived_from<T, views::View>
  T* GetViewAs(ui::ElementIdentifier id);

 private:
  raw_ptr<views::View> context_view_ = nullptr;
};

// Template implementations:

template <typename T>
  requires std::derived_from<T, views::View>
T* BrowserElementsViews::GetViewAs(ui::ElementIdentifier id) {
  return views::ElementTrackerViews::GetInstance()->GetFirstMatchingViewAs<T>(
      id, GetContext());
}

#endif  // CHROME_BROWSER_UI_VIEWS_INTERACTION_BROWSER_ELEMENTS_VIEWS_H_
