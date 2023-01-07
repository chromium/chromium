// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_DISPLAY_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_DISPLAY_DELEGATE_H_

#include <memory>

namespace views {
class View;
}  // namespace views

// Abstract interface that exposes methods needed for a view to register
// and unregister itself.
class AssistantDisplayDelegate {
 public:
  AssistantDisplayDelegate() = default;
  virtual ~AssistantDisplayDelegate() = default;

  // Takes ownership of |views| and displays it. Returns a raw pointer
  // to the view through which the view can still be modified as long as
  // it is alive. Previously set views are destroyed.
  virtual views::View* SetView(std::unique_ptr<views::View> view) = 0;

  // Returns the currently set view.
  virtual views::View* GetView() = 0;

  // Removes the view and thereby destroys it.
  virtual void RemoveView() = 0;
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_DISPLAY_DELEGATE_H_
