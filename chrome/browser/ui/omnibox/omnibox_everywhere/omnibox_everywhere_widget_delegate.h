// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_WIDGET_DELEGATE_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_WIDGET_DELEGATE_H_

#include "ui/views/widget/widget_delegate.h"

namespace omnibox_everywhere {

// Defines the custom window behavior and layout for the desktop Omnibox
// Everywhere widget.
class OmniboxEverywhereWidgetDelegate : public views::WidgetDelegate {
 public:
  OmniboxEverywhereWidgetDelegate();
  OmniboxEverywhereWidgetDelegate(const OmniboxEverywhereWidgetDelegate&) =
      delete;
  OmniboxEverywhereWidgetDelegate& operator=(
      const OmniboxEverywhereWidgetDelegate&) = delete;
  ~OmniboxEverywhereWidgetDelegate() override;
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_WIDGET_DELEGATE_H_
