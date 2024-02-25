// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import android.content.res.Resources;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;

import org.chromium.base.TraceEvent;

/**
 * Wrapper for VectorDrawableCompat to add trace event to
 * VectorDrawableCompat.create().
 */
public class TraceEventVectorDrawableCompat {
    /** Wrapper of VectorDrawableCompat.create() with trace event. */
    public static VectorDrawableCompat create(
            @NonNull Resources res, @DrawableRes int resId, @Nullable Resources.Theme theme) {
        try (TraceEvent te = TraceEvent.scoped("VectorDrawableCompat.create")) {
            return VectorDrawableCompat.create(res, resId, theme);
        }
    }
}
