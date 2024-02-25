// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Simple enum for the types of toolbar bitmap captures. Treat this list as append only and keep it
 * in sync with ToolbarCaptureType in enums.xml.
 **/
@IntDef({
    ToolbarCaptureType.UNKNOWN,
    ToolbarCaptureType.TOP,
    ToolbarCaptureType.BOTTOM,
    ToolbarCaptureType.NUM_ENTRIES
})
@Retention(RetentionPolicy.SOURCE)
public @interface ToolbarCaptureType {
    int UNKNOWN = 0;
    int TOP = 1;
    int BOTTOM = 2;
    int NUM_ENTRIES = 3;
}
