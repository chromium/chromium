// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTERACTION_BROWSER_ELEMENTS_H_
#define CHROME_BROWSER_UI_INTERACTION_BROWSER_ELEMENTS_H_

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

// Provides context and retrieves elements from the current browser window.
//
// The full browser implementation (BrowserElementsViews) may also be retrieved
// via its `From()` method as well, providing access to views.
class BrowserElements : public ui::FrameworkSpecificImplementation {
 public:
  DECLARE_USER_DATA(BrowserElements);

  explicit BrowserElements(BrowserWindowInterface& browser);
  ~BrowserElements() override;

  static BrowserElements* From(BrowserWindowInterface* browser);

  // Retrieves the element context.
  virtual ui::ElementContext GetContext() = 0;

  // Equivalent to ElementTracker calls, but without having to specify the
  // context:

  using ElementList = ui::ElementTracker::ElementList;
  ui::TrackedElement* GetElement(ui::ElementIdentifier id);
  ElementList GetAllElements(ui::ElementIdentifier id);

  // Sends `event_type` on the first matching element with `id`; returns false
  // if no such element is found.
  bool NotifyEvent(ui::ElementIdentifier id,
                   ui::CustomElementEventType event_type);

 private:
  ui::ScopedUnownedUserData<BrowserElements> scoped_data_holder_;
};

#endif  // CHROME_BROWSER_UI_INTERACTION_BROWSER_ELEMENTS_H_
