// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A means of tracking which mechanism is being used to focus the omnibox. */
@IntDef({
    OmniboxFocusReason.OMNIBOX_TAP,
    OmniboxFocusReason.OMNIBOX_LONG_PRESS,
    OmniboxFocusReason.FAKE_BOX_TAP,
    OmniboxFocusReason.FAKE_BOX_LONG_PRESS,
    OmniboxFocusReason.ACCELERATOR_TAP,
    OmniboxFocusReason.TAB_SWITCHER_OMNIBOX_TAP,
    OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_TAP,
    OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_LONG_PRESS,
    OmniboxFocusReason.DEFAULT_WITH_HARDWARE_KEYBOARD,
    OmniboxFocusReason.SEARCH_QUERY,
    OmniboxFocusReason.LAUNCH_NEW_INCOGNITO_TAB,
    OmniboxFocusReason.MENU_OR_KEYBOARD_ACTION,
    OmniboxFocusReason.UNFOCUS,
    OmniboxFocusReason.QUERY_TILES_NTP_TAP,
    OmniboxFocusReason.FOLD_TRANSITION_RESTORATION,
    OmniboxFocusReason.DRAG_DROP_TO_OMNIBOX,
    OmniboxFocusReason.TAP_AFTER_FOCUS_FROM_KEYBOARD
})
@Retention(RetentionPolicy.SOURCE)
public @interface OmniboxFocusReason {
    int OMNIBOX_TAP = 0;
    int OMNIBOX_LONG_PRESS = 1;
    int FAKE_BOX_TAP = 2;
    int FAKE_BOX_LONG_PRESS = 3;
    int ACCELERATOR_TAP = 4;
    // TAB_SWITCHER_OMNIBOX_TAP has not been used anymore, keep it for record for now.
    int TAB_SWITCHER_OMNIBOX_TAP = 5;
    int TASKS_SURFACE_FAKE_BOX_TAP = 6;
    int TASKS_SURFACE_FAKE_BOX_LONG_PRESS = 7;
    int DEFAULT_WITH_HARDWARE_KEYBOARD = 8;
    int SEARCH_QUERY = 9;
    int LAUNCH_NEW_INCOGNITO_TAB = 10;
    int MENU_OR_KEYBOARD_ACTION = 11;
    int UNFOCUS = 12;
    int QUERY_TILES_NTP_TAP = 13;
    int FOLD_TRANSITION_RESTORATION = 14;
    int DRAG_DROP_TO_OMNIBOX = 15;
    // Emitted on tap after focus from #8.
    int TAP_AFTER_FOCUS_FROM_KEYBOARD = 16;
    int NUM_ENTRIES = 17;
}
