// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A subset of the browser element identifiers are being used in Desktop UI
// benchmark. The name of the identifiers and the string names used by the
// benchmark are expected to be equal.
//
// Please keep the names in this file in sync with
// `tools/perf/page_sets/desktop_ui/browser_element_identifiers.py`

#ifndef CHROME_BROWSER_UI_BROWSER_ELEMENT_IDENTIFIERS_H_
#define CHROME_BROWSER_UI_BROWSER_ELEMENT_IDENTIFIERS_H_

#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"

// These should gradually replace values in view_ids.h.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kAppMenuButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kAppUninstallDialogOkButtonId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kAutofillCreditCardSuggestionEntryElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kAvatarButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kBookmarkStarViewElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kInstallPwaElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kLocationIconElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kMediaButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kOmniboxElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kReadLaterButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kReadLaterSidePanelWebViewElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSidePanelCloseButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSavePasswordComboboxElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSideSearchButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kTabAlertIndicatorButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kTabCounterButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kTabGroupEditorBubbleId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kTabGroupHeaderElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kTabSearchBubbleElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kTabSearchButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kTabElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kTabStripElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kTabStripRegionElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kTopContainerElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kIntentChipElementId);

DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kTabGroupedCustomEventId);

#endif  // CHROME_BROWSER_UI_BROWSER_ELEMENT_IDENTIFIERS_H_
