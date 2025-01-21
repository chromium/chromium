// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.edge_to_edge;

import android.graphics.Color;
import android.view.View;
import android.view.Window;

import androidx.annotation.ColorInt;

import org.chromium.ui.UiUtils;
import org.chromium.ui.util.ColorUtils;

/** Base implementation of a {@link SystemBarColorHelper} that tracks colors internally. */
public abstract class BaseSystemBarColorHelper implements SystemBarColorHelper {
    protected @ColorInt int mStatusBarColor = Color.TRANSPARENT;
    protected @ColorInt int mNavBarColor = Color.TRANSPARENT;
    protected @ColorInt int mNavBarDividerColor = Color.TRANSPARENT;

    @Override
    public void setStatusBarColor(@ColorInt int color) {
        if (color == getStatusBarColor()) return;
        mStatusBarColor = color;
        applyStatusBarColor();
    }

    @Override
    public void setNavigationBarColor(@ColorInt int color) {
        if (color == getNavigationBarColor()) return;
        mNavBarColor = color;
        applyNavBarColor();
    }

    /** Wrapper call to {@link Window#setNavigationBarDividerColor(int)}} */
    @Override
    public void setNavigationBarDividerColor(@ColorInt int dividerColor) {
        if (dividerColor == getNavigationBarDividerColor()) return;
        mNavBarDividerColor = dividerColor;
        applyNavigationBarDividerColor();
    }

    /** Return the current status bar color tracked by this instance. */
    public @ColorInt int getStatusBarColor() {
        return mStatusBarColor;
    }

    /** Return the current nav bar color tracked by this instance. */
    public @ColorInt int getNavigationBarColor() {
        return mNavBarColor;
    }

    /**
     * Sets the status bar icons to dark or light based on the luminance of mStatusBarColor to
     * ensure enough contrast.
     */
    public void updateStatusBarIconColor(View rootView) {
        // TODO(crbug.com/390261112): Handle coloring Status bar and Nav bar icon colors in
        //  BaseSystemColorHelper.
        UiUtils.setStatusBarIconColor(
                rootView,
                ColorUtils.isHighLuminance(ColorUtils.calculateLuminance(mStatusBarColor)));
    }

    /**
     * Sets the navigation bar icons to dark or light based on the luminance of mNavBarColor to
     * ensure enough contrast.
     */
    public void updateNavigationBarIconColor(View rootView) {
        // TODO(crbug.com/390261112): Handle coloring Status bar and Nav bar icon colors in
        //  BaseSystemColorHelper.
        UiUtils.setNavigationBarIconColor(
                rootView, ColorUtils.isHighLuminance(ColorUtils.calculateLuminance(mNavBarColor)));
    }

    /** Return the current nav bar divider color tracked by this instance. */
    public @ColorInt int getNavigationBarDividerColor() {
        return mNavBarDividerColor;
    }

    protected abstract void applyStatusBarColor();

    protected abstract void applyNavBarColor();

    protected abstract void applyNavigationBarDividerColor();
}
