// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHOOSER_BUBBLE_TESTAPI_H_
#define CHROME_BROWSER_UI_CHOOSER_BUBBLE_TESTAPI_H_

#include <memory>

namespace test {

// A ChooserBubbleUiWaiter allows waiting for a device chooser bubble to be
// closed, and keeps track of whether such a bubble has been shown or closed
// yet.
class ChooserBubbleUiWaiter {
 public:
  virtual ~ChooserBubbleUiWaiter() = default;

  // Creates an instance of this class appropriate for the current platform.
  static std::unique_ptr<ChooserBubbleUiWaiter> Create();

  bool has_shown() { return has_shown_; }
  bool has_closed() { return has_closed_; }

  virtual void WaitForChange() = 0;

 protected:
  bool has_shown_ = false;
  bool has_closed_ = false;
};

}  // namespace test

#endif  // CHROME_BROWSER_UI_CHOOSER_BUBBLE_TESTAPI_H_
