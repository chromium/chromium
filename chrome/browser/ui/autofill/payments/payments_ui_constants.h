// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_UI_CONSTANTS_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_UI_CONSTANTS_H_

#include "base/time/time.h"
#include "ui/gfx/geometry/insets.h"

namespace autofill {

constexpr int kMigrationDialogMainContainerChildSpacing = 24;
constexpr gfx::Insets kMigrationDialogInsets = gfx::Insets(0, 24, 48, 24);

// The time span a card bubble should be visible even if the document
// navigates away meanwhile. This is to ensure that the user can see
// the bubble.
constexpr base::TimeDelta kCardBubbleSurviveNavigationTime =
    base::TimeDelta::FromSeconds(5);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_UI_CONSTANTS_H_
