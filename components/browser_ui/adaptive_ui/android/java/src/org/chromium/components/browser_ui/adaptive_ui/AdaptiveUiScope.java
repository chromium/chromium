// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Scopes defining the lifecycle of an adaptive UI entry. */
@NullMarked
@IntDef({
    AdaptiveUiScope.TRANSIENT,
    AdaptiveUiScope.WINDOW,
})
@Retention(RetentionPolicy.SOURCE)
public @interface AdaptiveUiScope {
    int TRANSIENT = 0;
    int WINDOW = 1;
}
