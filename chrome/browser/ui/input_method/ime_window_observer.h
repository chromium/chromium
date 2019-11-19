// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INPUT_METHOD_IME_WINDOW_OBSERVER_H_
#define CHROME_BROWSER_UI_INPUT_METHOD_IME_WINDOW_OBSERVER_H_

namespace ui {

class ImeWindowObserver {
 public:
  ImeWindowObserver() {}
  virtual void OnWindowDestroyed(ImeWindow* ime_window) = 0;

 protected:
  virtual ~ImeWindowObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ImeWindowObserver);
};

}  // namespace ui

#endif  // CHROME_BROWSER_UI_INPUT_METHOD_IME_WINDOW_OBSERVER_H_
