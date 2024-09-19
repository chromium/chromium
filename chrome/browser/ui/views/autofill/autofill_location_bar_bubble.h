// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_LOCATION_BAR_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_LOCATION_BAR_BUBBLE_H_

#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace autofill {

// Base class for Autofill bubbles anchored on the location bar.
// The main purpose of this base class is to make downcasting `AutofillBaseBase`
// to `BubbleDialogDelegate` safe.
class AutofillLocationBarBubble : public AutofillBubbleBase,
                                  public LocationBarBubbleDelegateView {
  METADATA_HEADER(AutofillLocationBarBubble, LocationBarBubbleDelegateView)
  using LocationBarBubbleDelegateView::LocationBarBubbleDelegateView;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_LOCATION_BAR_BUBBLE_H_
