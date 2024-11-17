// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.edge_to_edge.layout;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PorterDuff.Mode;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.graphics.Insets;

import org.chromium.ui.util.WindowInsetsUtils;

/**
 * A wrapper layout that adjusts padding based on the current window insets. It ensures it's child
 * views have no overlap with system insets, making them edge to edge compatible.
 *
 * <p>This layout is meant to be used when the activity is drawing under the system insets.
 */
public class EdgeToEdgeBaseLayout extends FrameLayout {
    private static final int DEFAULT_NAV_BAR_DIVIDER_SIZE = 1;
    private final Rect mViewRect = new Rect();
    private final Rect mStatusBarRect = new Rect();
    private final Rect mNavBarRect = new Rect();
    private final Rect mNavBarDividerRect = new Rect();
    private final Paint mStatusBarPaint = new Paint();
    private final Paint mNavBarPaint = new Paint();
    private final Paint mNavBarDividerPaint = new Paint();

    private Insets mStatusBarInsets = Insets.NONE;
    private Insets mNavigationBarInsets = Insets.NONE;

    public EdgeToEdgeBaseLayout(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);

        // Nav bar can draw on top of the status bar in landscape using 3-button mode.
        mNavBarPaint.setXfermode(new PorterDuffXfermode(Mode.SRC_OVER));
    }

    @Override
    public void onDraw(@NonNull Canvas canvas) {
        // Draw colors over its padding.
        colorRectOnDraw(canvas, mStatusBarRect, mStatusBarPaint);
        colorRectOnDraw(canvas, mNavBarRect, mNavBarPaint);
        if (!mNavBarDividerRect.isEmpty() && mNavBarDividerPaint.getAlpha() > 0) {
            colorRectOnDraw(canvas, mNavBarDividerRect, mNavBarDividerPaint);
        }

        super.onDraw(canvas);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        updateCachedRects();
    }

    void updateCachedRects() {
        mViewRect.set(getLeft(), getTop(), getRight(), getBottom());
        mStatusBarRect.set(WindowInsetsUtils.toRectInWindow(mViewRect, mStatusBarInsets));
        mNavBarRect.set(WindowInsetsUtils.toRectInWindow(mViewRect, mNavigationBarInsets));
        mNavBarDividerRect.set(getNavBarDividerRectFromInset(mNavigationBarInsets, mNavBarRect));

        // In landscape mode, status bar can intersect with nav bar.
        if (Rect.intersects(mStatusBarRect, mNavBarRect)) {
            mStatusBarRect.left += mNavigationBarInsets.left;
            mStatusBarRect.right -= mNavigationBarInsets.right;
        }
    }

    void setStatusBarInsets(@NonNull Insets insets) {
        mStatusBarInsets = insets;
    }

    void setNavigationBarInsets(@NonNull Insets insets) {
        mNavigationBarInsets = insets;
    }

    void setStatusBarColor(@ColorInt int color) {
        mStatusBarPaint.setColor(color);
    }

    void setNavBarColor(@ColorInt int color) {
        mNavBarPaint.setColor(color);
    }

    void setNavBarDividerColor(@ColorInt int color) {
        mNavBarDividerPaint.setColor(color);
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

    Rect getNavigationBarRectForTesting() {
        return mNavBarRect;
    }

    Rect getNavigationBarDividerRectForTesting() {
        return mNavBarDividerRect;
    }
}
