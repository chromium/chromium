// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_CHILD_VIEW_TARGETER_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_CHILD_VIEW_TARGETER_DELEGATE_H_

#include "ui/views/view_targeter_delegate.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class View;
}  // namespace views

// A ViewTargeterDelegate that delegates hit testing to its visible,
// event-processing children.
class ChildViewTargeterDelegate : public views::ViewTargeterDelegate {
 public:
  ChildViewTargeterDelegate() = default;
  ChildViewTargeterDelegate(const ChildViewTargeterDelegate&) = delete;
  ChildViewTargeterDelegate& operator=(const ChildViewTargeterDelegate&) =
      delete;
  ~ChildViewTargeterDelegate() override = default;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_CHILD_VIEW_TARGETER_DELEGATE_H_
