// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_VIEW_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_VIEW_UTIL_H_

#include <memory>

#include "base/strings/string16.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace autofill {

// Returns a new label with auto-color readability disabled to ensure consistent
// colors in the title when a dark native theme is applied
// (https://crbug.com/881514).
std::unique_ptr<views::Label> CreateLabelWithColorReadabilityDisabled(
    const base::string16& text,
    int text_context,
    int text_style);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_VIEW_UTIL_H_
