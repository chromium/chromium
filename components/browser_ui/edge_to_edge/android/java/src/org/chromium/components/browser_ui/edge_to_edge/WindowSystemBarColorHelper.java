// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.edge_to_edge;

import android.app.Activity;
import android.graphics.Color;
import android.os.Build;
import android.view.Window;

import org.chromium.ui.UiUtils;
import org.chromium.ui.util.ColorUtils;

/** A wrapper class around {@link Window} to change the system bar colors. */
public final class WindowSystemBarColorHelper extends BaseSystemBarColorHelper {
    private final Window mWindow;

    /**
     * @param window Window in from {@link Activity#getWindow()}.
     */
    public WindowSystemBarColorHelper(Window window) {
        mWindow = window;

        mStatusBarColor = mWindow.getStatusBarColor();
        mNavBarColor = mWindow.getNavigationBarColor();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            mNavBarDividerColor = mWindow.getNavigationBarDividerColor();
        }
    }

    @Override
    public int getStatusBarColor() {
        return mWindow.getStatusBarColor();
    }

    /** Wrapper call to {@link Window#setStatusBarColor(int)} (int)}. */
    @Override
    protected void applyStatusBarColor() {
        mWindow.setStatusBarColor(mStatusBarColor);
        UiUtils.setStatusBarIconColor(
                mWindow.getDecorView(),
                ColorUtils.isHighLuminance(ColorUtils.calculateLuminance(mStatusBarColor)));
    }

    @Override
    public int getNavigationBarColor() {
        return mWindow.getNavigationBarColor();
    }

    /** Wrapper call to {@link Window#setNavigationBarColor(int)}. */
    @Override
    protected void applyNavBarColor() {
        mWindow.setNavigationBarColor(mNavBarColor);
        UiUtils.setNavigationBarIconColor(
                mWindow.getDecorView(),
                ColorUtils.isHighLuminance(ColorUtils.calculateLuminance(mNavBarColor)));
    }

    @Override
    public void setNavigationBarDividerColor(int dividerColor) {
        // Ignore the call on unsupported SDK.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            super.setNavigationBarDividerColor(dividerColor);
        }
    }

    /** Wrapper call to {@link Window#setNavigationBarDividerColor(int)}} */
    @Override
    protected void applyNavigationBarDividerColor() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            mWindow.setNavigationBarDividerColor(mNavBarDividerColor);
        }
    }

    @Override
    public int getNavigationBarDividerColor() {
        return (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P)
                ? mWindow.getNavigationBarDividerColor()
                : Color.TRANSPARENT;
    }

    @Override
    public void destroy() {}

    /** Wrapper call to {@link Window#setNavigationBarContrastEnforced(boolean)}. */
    public void setNavigationBarContrastEnforced(boolean enforced) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            mWindow.setNavigationBarContrastEnforced(enforced);
        }
    }
}
