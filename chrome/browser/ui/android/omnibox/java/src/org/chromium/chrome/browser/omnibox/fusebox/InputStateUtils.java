// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.content.Context;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.contextual_search.InputState;
import org.chromium.components.omnibox.InputSourceProtoIntDef.InputSource;
import org.chromium.components.omnibox.InputTypeConfigProto.InputTypeConfig;

/** Utility methods for resolving UI labels and resources from {@link InputState}. */
@NullMarked
public final class InputStateUtils {
    private InputStateUtils() {}

    /**
     * Returns the non-empty menu label from {@code inputState} for {@code inputSource}, or
     * optionally records a fallback metric and returns the localized string for {@code
     * defaultTextRes} if absent.
     */
    static String resolveInputMenuLabel(
            Context context,
            @Nullable InputState inputState,
            @InputSource int inputSource,
            @StringRes int defaultTextRes,
            boolean shouldRecordFallback) {
        if (inputState != null) {
            InputTypeConfig config = inputState.getInputTypeConfig(inputSource);
            if (config != null) {
                String label = config.getMenuLabel();
                if (!label.isEmpty()) {
                    return label;
                }
            }
        }
        // TODO(crbug.com/579103629): Remove client fallback strings once InputState covers all
        // visible attachment sources.
        if (shouldRecordFallback) {
            FuseboxMetrics.notifyAttachmentLabelFallback(inputSource);
        }
        return context.getString(defaultTextRes);
    }
}
