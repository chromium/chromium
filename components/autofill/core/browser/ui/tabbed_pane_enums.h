// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_TABBED_PANE_ENUMS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_TABBED_PANE_ENUMS_H_

namespace autofill {

// Represents the type of a singular tab inside of a tabbed pane in an Autofill
// dropdown.
enum class TabbedPaneTabType {
  // The pay now tab. This contains payment methods for paying at this moment,
  // such as credit cards.
  kPayNow = 0,
  // The pay later tab. This contains payment methods to pay over time, such as
  // individual BNPL issuers.
  kPayLater = 1,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_TABBED_PANE_ENUMS_H_
