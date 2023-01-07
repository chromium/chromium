// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HID_HID_CHOOSER_H_
#define CHROME_BROWSER_UI_HID_HID_CHOOSER_H_

#include "base/functional/callback_helpers.h"
#include "content/public/browser/hid_chooser.h"

// Owns a HID device chooser dialog and closes it when destroyed.
class HidChooser : public content::HidChooser {
 public:
  explicit HidChooser(base::OnceClosure close_closure);

  HidChooser(const HidChooser&) = delete;
  HidChooser& operator=(const HidChooser&) = delete;

  ~HidChooser() override = default;

 private:
  base::ScopedClosureRunner closure_runner_;
};

#endif  // CHROME_BROWSER_UI_HID_HID_CHOOSER_H_
