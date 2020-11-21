// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FONT_ACCESS_FONT_ACCESS_CHOOSER_H_
#define CHROME_BROWSER_UI_FONT_ACCESS_FONT_ACCESS_CHOOSER_H_

#include "base/callback_helpers.h"
#include "content/public/browser/font_access_chooser.h"

class FontAccessChooser : public content::FontAccessChooser {
 public:
  explicit FontAccessChooser(base::OnceClosure close_callback);
  ~FontAccessChooser() override = default;

 private:
  base::ScopedClosureRunner closure_runner_;
};

#endif  // CHROME_BROWSER_UI_FONT_ACCESS_FONT_ACCESS_CHOOSER_H_
