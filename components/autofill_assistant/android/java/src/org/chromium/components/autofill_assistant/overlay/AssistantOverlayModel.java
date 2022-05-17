// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.overlay;

import android.content.Context;
import android.graphics.Rect;
import android.graphics.RectF;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.autofill_assistant.generic_ui.AssistantDrawable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * State for the header of the Autofill Assistant.
 */
@JNINamespace("autofill_assistant")
public class AssistantOverlayModel extends PropertyModel {
    /** Wrapper model for {@code RectF}. */
    public static class AssistantOverlayRect extends RectF {
        private boolean mIsFullWidth;

        public AssistantOverlayRect() {
            super();
        }

        public AssistantOverlayRect(
                float left, float top, float right, float bottom, boolean fullWidth) {
            super(left, top, right, bottom);
            mIsFullWidth = fullWidth;
        }

        public AssistantOverlayRect(AssistantOverlayRect rect) {
            set(rect);
        }

        public AssistantOverlayRect(Rect rect) {
            super.set(rect);
        }

        public void set(AssistantOverlayRect rect) {
            super.set(rect);
            mIsFullWidth = rect.mIsFullWidth;
        }

        public boolean isFullWidth() {
            return mIsFullWidth;
        }
    }
    public static final WritableIntPropertyKey STATE = new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<WebContents> WEB_CONTENTS =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<RectF> VISUAL_VIEWPORT =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<List<AssistantOverlayRect>> TOUCHABLE_AREA =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<List<AssistantOverlayRect>> RESTRICTED_AREA =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<AssistantOverlayDelegate> DELEGATE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Integer> BACKGROUND_COLOR =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Integer> HIGHLIGHT_BORDER_COLOR =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Integer> TAP_TRACKING_COUNT =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Long> TAP_TRACKING_DURATION_MS =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<AssistantOverlayImage> OVERLAY_IMAGE =
            new WritableObjectPropertyKey<>();

    public AssistantOverlayModel() {
        super(WEB_CONTENTS, STATE, VISUAL_VIEWPORT, TOUCHABLE_AREA, RESTRICTED_AREA, DELEGATE,
                BACKGROUND_COLOR, HIGHLIGHT_BORDER_COLOR, TAP_TRACKING_COUNT,
                TAP_TRACKING_DURATION_MS, OVERLAY_IMAGE);
    }

    @CalledByNative
    private void setState(@AssistantOverlayState int state) {
        set(STATE, state);
    }

    @CalledByNative
    private void setWebContents(WebContents webContents) {
        set(WEB_CONTENTS, webContents);
    }

    private static List<AssistantOverlayRect> toRectangles(float[] coords) {
        List<AssistantOverlayRect> boxes = new ArrayList<>();
        for (int i = 0; i < coords.length; i += 5) {
            boxes.add(new AssistantOverlayRect(/* left= */ coords[i], /* top= */ coords[i + 1],
                    /* right= */ coords[i + 2], /* bottom= */ coords[i + 3],
                    /* fullWidth= */ (int) coords[i + 4] == 1));
        }
        return boxes;
    }

    @CalledByNative
    private void setVisualViewport(float left, float top, float right, float bottom) {
        set(VISUAL_VIEWPORT, new RectF(left, top, right, bottom));
    }

    @CalledByNative
    private void setTouchableArea(float[] coords) {
        set(TOUCHABLE_AREA, toRectangles(coords));
    }

    @CalledByNative
    private void setRestrictedArea(float[] coords) {
        set(RESTRICTED_AREA, toRectangles(coords));
    }

    @CalledByNative
    private void setDelegate(AssistantOverlayDelegate delegate) {
        set(DELEGATE, delegate);
    }

    @CalledByNative
    private void setBackgroundColor(@Nullable @ColorInt Integer color) {
        set(BACKGROUND_COLOR, color);
    }

    @CalledByNative
    private void setHighlightBorderColor(@Nullable @ColorInt Integer color) {
        set(HIGHLIGHT_BORDER_COLOR, color);
    }

    @CalledByNative
    private void setOverlayImage(Context context, @Nullable AssistantDrawable imageDrawable,
            int imageSizeInPixels, int imageTopMarginInPixels, int imageBottomMarginInPixels,
            String text, @Nullable @ColorInt Integer textColor, int textSizeInPixels) {
        AssistantOverlayImage assistantOverlayImage =
                new AssistantOverlayImage(imageSizeInPixels, imageTopMarginInPixels,
                        imageBottomMarginInPixels, text, textColor, textSizeInPixels);
        if (imageDrawable == null) {
            set(OVERLAY_IMAGE, assistantOverlayImage);
            return;
        }
        imageDrawable.getDrawable(context, drawable -> {
            if (drawable != null) {
                assistantOverlayImage.mDrawable = drawable;
                set(OVERLAY_IMAGE, assistantOverlayImage);
            }
        });
    }

    @CalledByNative
    private void clearOverlayImage() {
        set(OVERLAY_IMAGE, null);
    }

    @CalledByNative
    private void setTapTracking(int count, long durationMs) {
        set(TAP_TRACKING_COUNT, count);
        set(TAP_TRACKING_DURATION_MS, durationMs);
    }
}
