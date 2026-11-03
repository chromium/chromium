// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.contextual_search.InputState;
import org.chromium.components.omnibox.InputSourceProtoIntDef.InputSource;
import org.chromium.components.omnibox.InputTypeConfigProto.InputTypeConfig;
import org.chromium.components.omnibox.ToolConfigProto.ToolConfig;
import org.chromium.components.omnibox.ToolModeProtoIntDef.ToolMode;

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

    /**
     * Returns the client drawable for {@code inputSource} in {@code inputState}, or optionally
     * records a fallback metric and returns {@code defaultIconRes} if absent or unmapped.
     */
    public static @DrawableRes int resolveInputIconRes(
            @Nullable InputState inputState,
            @InputSource int inputSource,
            @DrawableRes int defaultIconRes,
            boolean shouldRecordFallback) {
        if (inputState != null) {
            InputTypeConfig config = inputState.getInputTypeConfig(inputSource);
            @DrawableRes
            int iconRes =
                    FuseboxIconUtils.resolveIconRes(
                            config != null && config.hasIcon() ? config.getIcon() : null,
                            Resources.ID_NULL);
            if (iconRes != Resources.ID_NULL) {
                return iconRes;
            }
        }
        // TODO(crbug.com/579115127): Remove client fallback drawables once InputState covers all
        // visible attachment sources with mapped icon ids.
        if (shouldRecordFallback) {
            FuseboxMetrics.notifyAttachmentIconFallback(inputSource);
        }
        return defaultIconRes;
    }

    /**
     * Returns the client drawable for {@code toolMode} in {@code inputState}, or {@code
     * defaultIconRes} if absent or unmapped.
     */
    public static @DrawableRes int resolveToolIconRes(
            @Nullable InputState inputState,
            @ToolMode int toolMode,
            @DrawableRes int defaultIconRes) {
        if (inputState == null) {
            return defaultIconRes;
        }
        ToolConfig config = inputState.getToolConfig(toolMode);
        return FuseboxIconUtils.resolveIconRes(
                config != null && config.hasIcon() ? config.getIcon() : null, defaultIconRes);
    }
}
