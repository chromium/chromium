// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SERIAL_SERIAL_CHOOSER_H_
#define CHROME_BROWSER_UI_SERIAL_SERIAL_CHOOSER_H_

#include "base/functional/callback_helpers.h"
#include "content/public/browser/serial_chooser.h"

// Owns a serial port chooser dialog and closes it when destroyed.
class SerialChooser : public content::SerialChooser {
 public:
  explicit SerialChooser(base::OnceClosure close_closure);

  SerialChooser(const SerialChooser&) = delete;
  SerialChooser& operator=(const SerialChooser&) = delete;

  ~SerialChooser() override = default;

 private:
  base::ScopedClosureRunner closure_runner_;
};

#endif  // CHROME_BROWSER_UI_SERIAL_SERIAL_CHOOSER_H_
