// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INTERACTION_BROWSER_ELEMENTS_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_INTERACTION_BROWSER_ELEMENTS_VIEWS_H_

#include <concepts>
#include <map>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/base/interaction/typed_identifier.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"

DECLARE_TYPED_IDENTIFIER_VALUE(views::WebView,
                               kActiveContentsWebViewRetrievalId);

// Provides Views-specific extensions to `BrowserElements` so it can
// provide a context, elements, and Views.
class BrowserElementsViews : public BrowserElements {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  explicit BrowserElementsViews(BrowserWindowInterface& browser);
  ~BrowserElementsViews() override;

  // Returns a `BrowserElementsViews` if there is one associated with `browser`.
  // This should be valid for any browser which uses Views, including standard
  // Desktop Chrome and Webium builds.
  static BrowserElementsViews* From(BrowserWindowInterface* browser);

  // These provide convenience access to ElementTrackerViews without having to
  // specify context:

  using ViewList = views::ElementTrackerViews::ViewList;
  views::View* GetView(ui::ElementIdentifier id);
  ViewList GetAllViews(ui::ElementIdentifier id);

  template <typename T>
    requires std::derived_from<T, views::View>
  T* GetViewAs(ui::ElementIdentifier id);

  // Returns the widget of the primary window. Default implementation uses
  // context and `ElementTrackerViews`.
  virtual views::Widget* GetPrimaryWindowWidget();

  // Call this when the hosting context is going away.
  virtual void TearDown();

  // Functions to add additional retrieval methods.
  // Usage:
  // ```
  //  // In browser_window_features.cc
  //  browser_elements_views->Init(...);
  //  browser_elements_views->AddRetrievalCallback(
  //      kMyViewRetrievalId,
  //      base::BindRepeating(...));
  //
  //  // In code that needs this view:
  //  MyView* my_view = BrowserElementsViews::From(browser)
  //      ->RetrieveView(kMyViewRetrievalId);
  // ```

  template <typename T>
    requires std::derived_from<T, views::View>
  using RetrievalCallback = base::RepeatingCallback<T*()>;

  // Registers a retrieval callback. Must be called between Init() and
  // TearDown().
  template <typename T>
    requires std::derived_from<T, views::View>
  void AddRetrievalCallback(ui::TypedIdentifier<T> retrieval_id,
                            RetrievalCallback<T> callback);

  // Retrieves a view using a callback registered with `retrieval_id`. Will
  // return null if no callback is registered, this object is in an invalid
  // state, or the view is not present or is the wrong type.
  template <typename T>
    requires std::derived_from<T, views::View>
  T* RetrieveView(ui::TypedIdentifier<T> retrieval_id);

 private:
  virtual bool IsInitialized() const = 0;

  std::map<ui::ElementIdentifier, base::RepeatingCallback<views::View*()>>
      retrieval_callbacks_;
};

// Template implementations:

template <typename T>
  requires std::derived_from<T, views::View>
T* BrowserElementsViews::GetViewAs(ui::ElementIdentifier id) {
  return views::ElementTrackerViews::GetInstance()->GetFirstMatchingViewAs<T>(
      id, GetContext());
}

template <typename T>
  requires std::derived_from<T, views::View>
void BrowserElementsViews::AddRetrievalCallback(
    ui::TypedIdentifier<T> retrieval_id,
    RetrievalCallback<T> callback) {
  CHECK(IsInitialized());
  const auto result = retrieval_callbacks_.emplace(
      retrieval_id.identifier(),
      base::BindRepeating([](const RetrievalCallback<T>& callback)
                              -> views::View* { return callback.Run(); },
                          std::move(callback)));
  CHECK(result.second) << "Retrieval callback already registered for "
                       << retrieval_id;
}

template <typename T>
  requires std::derived_from<T, views::View>
T* BrowserElementsViews::RetrieveView(ui::TypedIdentifier<T> id) {
  if (const auto it = retrieval_callbacks_.find(id.identifier());
      it != retrieval_callbacks_.end()) {
    return views::AsViewClass<T>(it->second.Run());
  }
  return nullptr;
}

#endif  // CHROME_BROWSER_UI_VIEWS_INTERACTION_BROWSER_ELEMENTS_VIEWS_H_
