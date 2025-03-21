// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.edge_to_edge.layout;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff.Mode;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.core.graphics.Insets;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.util.WindowInsetsUtils;

/**
 * A wrapper layout that adjusts padding based on the current window insets. It ensures it's child
 * views have no overlap with system insets, making them edge to edge compatible.
 *
 * <p>The layout paints the status / navigation bar color according to settings; the layout paints
 * the display cutout black. When display cutout overlaps with the status bar, the display cutout
 * paint will be ignored.
 *
 * <p>This layout is meant to be used when the activity is drawing under the system insets.
 */
@NullMarked
public class EdgeToEdgeBaseLayout extends FrameLayout {
    private static final int DEFAULT_NAV_BAR_DIVIDER_SIZE = 1;
    private static final int DISPLAY_CUTOUT_PAINT_COLOR = Color.BLACK;
    private static final int DEBUG_PAINT_COLOR = Color.argb(100, 200, 0, 200);

    private final Rect mViewRect = new Rect();
    private final Rect mStatusBarRect = new Rect();
    private final Rect mNavBarRect = new Rect();
    private final Rect mNavBarDividerRect = new Rect();
    private final Rect mCutoutRectTop = new Rect();
    private final Rect mCutoutRectLeft = new Rect();
    private final Rect mCutoutRectRight = new Rect();
    private final Rect mCaptionBarRect = new Rect();

    private final Rect mStatusBarRectDebug = new Rect(); // Draws at 50% width of the actual rect.
    private final Rect mNavBarRectDebug = new Rect(); // Draws at 50% width of the actual rect.

    private final Paint mDebugPaint = new Paint();
    private final Paint mStatusBarPaint = new Paint();
    private final Paint mNavBarPaint = new Paint();
    private final Paint mNavBarDividerPaint = new Paint();
    private final Paint mDisplayCutoutPaint = new Paint();

    private Insets mStatusBarInsets = Insets.NONE;
    private Insets mNavigationBarInsets = Insets.NONE;
    private Insets mCutoutInsetsTop = Insets.NONE;
    private Insets mCutoutInsetsLeft = Insets.NONE;
    private Insets mCutoutInsetsRight = Insets.NONE;
    private Insets mCaptionBarInsets = Insets.NONE;

    private boolean mIsDebugging;

    public EdgeToEdgeBaseLayout(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);

        // Nav bar can draw on top of the status bar in landscape using 3-button mode.
        mNavBarPaint.setXfermode(new PorterDuffXfermode(Mode.SRC_OVER));

        mDisplayCutoutPaint.setColor(DISPLAY_CUTOUT_PAINT_COLOR);
    }

    void setIsDebugging(boolean isDebugging) {
        mIsDebugging = isDebugging;
        mDebugPaint.setColor(mIsDebugging ? DEBUG_PAINT_COLOR : Color.TRANSPARENT);
    }

    @Override
    public void onDraw(Canvas canvas) {
        // Draw colors over its padding.
        colorRectOnDraw(canvas, mStatusBarRect, mStatusBarPaint);
        // Reuse the status bar color for the caption bar color.
        colorRectOnDraw(canvas, mCaptionBarRect, mStatusBarPaint);
        colorRectOnDraw(canvas, mNavBarRect, mNavBarPaint);
        colorRectOnDraw(canvas, mCutoutRectTop, mDisplayCutoutPaint);
        colorRectOnDraw(canvas, mCutoutRectLeft, mDisplayCutoutPaint);
        colorRectOnDraw(canvas, mCutoutRectRight, mDisplayCutoutPaint);
        if (!mNavBarDividerRect.isEmpty() && mNavBarDividerPaint.getAlpha() > 0) {
            colorRectOnDraw(canvas, mNavBarDividerRect, mNavBarDividerPaint);
        }

        if (mIsDebugging) {
            colorRectOnDraw(canvas, mStatusBarRectDebug, mDebugPaint);
            colorRectOnDraw(canvas, mNavBarRectDebug, mDebugPaint);
        }

        super.onDraw(canvas);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        updateCachedRects();
    }

    void updateCachedRects() {
        mViewRect.set(getLeft(), getTop(), getRight(), getBottom());
        mStatusBarRect.set(WindowInsetsUtils.toRectInWindow(mViewRect, mStatusBarInsets));
        mNavBarRect.set(WindowInsetsUtils.toRectInWindow(mViewRect, mNavigationBarInsets));
        mCaptionBarRect.set(WindowInsetsUtils.toRectInWindow(mViewRect, mCaptionBarInsets));

        // In landscape mode, status bar can intersect with nav bar.
        if (Rect.intersects(mStatusBarRect, mNavBarRect)) {
            mStatusBarRect.left += mNavigationBarInsets.left;
            mStatusBarRect.right -= mNavigationBarInsets.right;
        }
        // In landscape mode, caption bar can intersect with nav bar. The caption bar often
        // intersects with the status bar, but that's alright since it will share the same color.
        if (Rect.intersects(mCaptionBarRect, mNavBarRect)) {
            mCaptionBarRect.left += mNavigationBarInsets.left;
            mCaptionBarRect.right -= mNavigationBarInsets.right;
        }
        // TODO(crbug.com/400517589): Cleanup display cutout Rects.
        mCutoutRectTop.set(WindowInsetsUtils.toRectInWindow(mViewRect, mCutoutInsetsTop));
        // When display cutout rect is coming from different direction as the status bar and
        // navigation bar, intersect those system bars.
        mCutoutRectLeft.set(WindowInsetsUtils.toRectInWindow(mViewRect, mCutoutInsetsLeft));
        mCutoutRectRight.set(WindowInsetsUtils.toRectInWindow(mViewRect, mCutoutInsetsRight));
        if (!mCutoutRectLeft.isEmpty() || !mCutoutRectRight.isEmpty()) {
            if (Rect.intersects(mStatusBarRect, mCutoutRectLeft)) {
                mStatusBarRect.left += mCutoutInsetsLeft.left;
            }
            if (Rect.intersects(mStatusBarRect, mCutoutRectRight)) {
                mStatusBarRect.right -= mCutoutInsetsRight.right;
            }
            if (Rect.intersects(mCaptionBarRect, mCutoutRectLeft)) {
                mCaptionBarRect.left += mCutoutInsetsLeft.left;
            }
            if (Rect.intersects(mCaptionBarRect, mCutoutRectRight)) {
                mCaptionBarRect.right -= mCutoutInsetsRight.right;
            }
            if (Rect.intersects(mNavBarRect, mCutoutRectLeft)) {
                mNavBarRect.left += mCutoutInsetsLeft.left;
            }
            if (Rect.intersects(mNavBarRect, mCutoutRectRight)) {
                mNavBarRect.right -= mCutoutInsetsRight.right;
            }
        }

        // Set the nav bar divider the last after the nav bar adjustments.
        mNavBarDividerRect.set(getNavBarDividerRectFromInset(mNavigationBarInsets, mNavBarRect));

        if (mIsDebugging) {
            mStatusBarRectDebug.set(mStatusBarRect);
            mStatusBarRectDebug.inset(mStatusBarRect.width() / 4, 0);
            mNavBarRectDebug.set(mNavBarRect);
            mNavBarRectDebug.inset(mNavBarRect.width() / 4, 0);
        }
    }

    void setStatusBarInsets(Insets insets) {
        mStatusBarInsets = insets;
    }

    void setNavigationBarInsets(Insets insets) {
        mNavigationBarInsets = insets;
    }

    void setDisplayCutoutTop(Insets insets) {
        assert insets.top > 0 || Insets.NONE.equals(insets);
        mCutoutInsetsTop = insets;
    }

    void setCaptionBarInsets(Insets insets) {
        mCaptionBarInsets = insets;
    }

    void setDisplayCutoutInsetLeft(Insets insets) {
        assert insets.left > 0 || Insets.NONE.equals(insets);
        mCutoutInsetsLeft = insets;
    }

    void setDisplayCutoutInsetRight(Insets insets) {
        assert insets.right > 0 || Insets.NONE.equals(insets);
        mCutoutInsetsRight = insets;
    }

    void setStatusBarColor(@ColorInt int color) {
        mStatusBarPaint.setColor(color);
        invalidate();
    }

    void setNavBarColor(@ColorInt int color) {
        mNavBarPaint.setColor(color);
        invalidate();
    }

    void setNavBarDividerColor(@ColorInt int color) {
        mNavBarDividerPaint.setColor(color);
        invalidate();
    }

    private static void colorRectOnDraw(Canvas canvas, Rect rect, Paint paint) {
        if (rect.isEmpty()) return;
        canvas.save();
        canvas.clipRect(rect);
        canvas.drawPaint(paint);
        canvas.restore();
    }

    private static Rect getNavBarDividerRectFromInset(Insets navBarInsets, Rect navBarRect) {
        Rect navBarDivider = new Rect(navBarRect);
        if (navBarInsets.bottom > 0) { // Divider on the top edge of nav bar.
            navBarDivider.bottom = navBarDivider.top + DEFAULT_NAV_BAR_DIVIDER_SIZE;
        } else if (navBarInsets.left > 0) { // // Divider on the right edge of nav bar.
            navBarDivider.left = navBarDivider.right - DEFAULT_NAV_BAR_DIVIDER_SIZE;
        } else if (navBarInsets.right > 0) { // Divider on the left edge of nav bar.
            navBarDivider.right = navBarDivider.left + DEFAULT_NAV_BAR_DIVIDER_SIZE;
        } else {
            assert navBarInsets.top <= 0 : "Nav bar insets should not be at the top.";
        }

        return navBarDivider;
    }

    Rect getStatusBarRectForTesting() {
        return mStatusBarRect;
    }

    Rect getCaptionBarRectForTesting() {
        return mCaptionBarRect;
    }

    Rect getNavigationBarRectForTesting() {
        return mNavBarRect;
    }

    Rect getNavigationBarDividerRectForTesting() {
        return mNavBarDividerRect;
    }

    Rect getCutoutRectTopForTesting() {
        return mCutoutRectTop;
    }

    Rect getCutoutRectLeftForTesting() {
        return mCutoutRectLeft;
    }

    Rect getCutoutRectRightForTesting() {
        return mCutoutRectRight;
    }
}
