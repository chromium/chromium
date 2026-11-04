// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Containers that can host an adaptive UI entry. */
@NullMarked
@IntDef(
        flag = true,
        value = {
            AdaptiveUiContainer.BOTTOM_SHEET,
            AdaptiveUiContainer.MODAL_DIALOG,
            AdaptiveUiContainer.SIDE_PANEL,
        })
@Retention(RetentionPolicy.SOURCE)
public @interface AdaptiveUiContainer {
    int BOTTOM_SHEET = 1 << 0;
    int MODAL_DIALOG = 1 << 1;
    int SIDE_PANEL = 1 << 2;
}
