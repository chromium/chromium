// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import android.content.res.Resources;

import androidx.annotation.DrawableRes;
import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;

import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Wrapper for VectorDrawableCompat to add trace event to
 * VectorDrawableCompat.create().
 */
@NullMarked
public class TraceEventVectorDrawableCompat {
    /** Wrapper of VectorDrawableCompat.create() with trace event. */
    public static @Nullable VectorDrawableCompat create(
            Resources res, @DrawableRes int resId, Resources.@Nullable Theme theme) {
        try (TraceEvent te = TraceEvent.scoped("VectorDrawableCompat.create")) {
            return VectorDrawableCompat.create(res, resId, theme);
        }
    }
}
