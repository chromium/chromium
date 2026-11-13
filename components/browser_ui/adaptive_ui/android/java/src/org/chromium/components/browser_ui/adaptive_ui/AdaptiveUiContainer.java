// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

// LINT.IfChange(AdaptiveUiContainer)
/**
 * Containers that can host an adaptive UI entry.
 *
 * <p>These values are persisted to logs. Entries should not be renumbered and numeric values should
 * never be reused.
 */
@NullMarked
@IntDef({AdaptiveUiContainer.BOTTOM_SHEET, AdaptiveUiContainer.MODAL_DIALOG})
@Retention(RetentionPolicy.SOURCE)
@Target(ElementType.TYPE_USE)
public @interface AdaptiveUiContainer {
    int BOTTOM_SHEET = 0;
    int MODAL_DIALOG = 1;

    int COUNT = 2;
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AdaptiveUiContainer)
