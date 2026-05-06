// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.overlay_panel;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** State of the Overlay Panel. */
@IntDef({
    PanelState.UNDEFINED,
    PanelState.CLOSED,
    PanelState.PEEKED,
    PanelState.EXPANDED,
    PanelState.MAXIMIZED
})
@Retention(RetentionPolicy.SOURCE)
@Target(ElementType.TYPE_USE)
@NullMarked
public @interface PanelState {
    int UNDEFINED = 0;
    int CLOSED = 1;
    int PEEKED = 2;
    int EXPANDED = 3;
    int MAXIMIZED = 4;
    int NUM_ENTRIES = 5;
}
