// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.load_progress;

import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** List of load progress bar properties. */
class LoadProgressProperties {
    @IntDef({
            CompletionState.UNFINISHED,
            CompletionState.FINISHED_DO_ANIMATE,
            CompletionState.FINISHED_DONT_ANIMATE,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface CompletionState {
        int UNFINISHED = 0;
        int FINISHED_DO_ANIMATE = 1;
        int FINISHED_DONT_ANIMATE = 2;
    }
    public static final PropertyModel.WritableIntPropertyKey COMPLETION_STATE =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableFloatPropertyKey PROGRESS =
            new PropertyModel.WritableFloatPropertyKey();
    static final PropertyKey[] ALL_KEYS = {COMPLETION_STATE, PROGRESS};
}