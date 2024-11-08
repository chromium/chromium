// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.edge_to_edge.layout;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.LayoutParams;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.components.browser_ui.edge_to_edge.BaseSystemBarColorHelper;
import org.chromium.components.browser_ui.edge_to_edge.R;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.InsetObserver.WindowInsetsConsumer;

/** Coordinator used to adjust the padding and paint color for {@link EdgeToEdgeBaseLayout}. */
public class EdgeToEdgeLayoutCoordinator extends BaseSystemBarColorHelper
        implements WindowInsetsConsumer {
    private final Activity mActivity;
    private final @Nullable InsetObserver mInsetObserver;
    private @Nullable EdgeToEdgeBaseLayout mView;

    /**
     * Construct the coordinator used to handle padding and color for the Edge to edge layout.
     *
     * @param activity The base activity.
     * @param insetObserver The inset observer of current window, if exists.
     */
    public EdgeToEdgeLayoutCoordinator(
            @NonNull Activity activity, @Nullable InsetObserver insetObserver) {
        mActivity = activity;
        mInsetObserver = insetObserver;
    }

    /**
     * @see Activity#setContentView(View)
     */
    public View wrapContentView(View contentView) {
        return wrapContentView(
                contentView,
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
    }

    /**
     * @see Activity#setContentView(View, LayoutParams)
     */
    public View wrapContentView(View contentView, LayoutParams params) {
        ensureInitialized();
        mView.addView(contentView, params);
        return mView;
    }

    // extends BaseSystemBarColorHelper
    @Override
    public void destroy() {
        if (mInsetObserver != null) {
            mInsetObserver.removeInsetsConsumer(this);
        }
        if (mView != null) {
            ViewCompat.setOnApplyWindowInsetsListener(mView, null);
            mView = null;
        }
    }

    @Override
    public void applyStatusBarColor() {
        if (mView == null) return;
        mView.setStatusBarColor(mStatusBarColor);
    }

    @Override
    public void applyNavBarColor() {
        if (mView == null) return;
        mView.setNavBarColor(mNavBarColor);
    }

    @Override
    public void applyNavigationBarDividerColor() {
        if (mView == null) return;
        mView.setNavBarDividerColor(mNavBarDividerColor);
    }

    @NonNull
    @Override
    public WindowInsetsCompat onApplyWindowInsets(
            @NonNull View view, @NonNull WindowInsetsCompat windowInsets) {
        Insets statusBarInsets = windowInsets.getInsets(WindowInsetsCompat.Type.statusBars());
        mView.setStatusBarInsets(statusBarInsets);

        Insets navBarInsets = windowInsets.getInsets(WindowInsetsCompat.Type.navigationBars());
        mView.setNavigationBarInsets(navBarInsets);

        Insets overallInsets = Insets.add(statusBarInsets, navBarInsets);
        mView.setPadding(
                overallInsets.left, overallInsets.top, overallInsets.right, overallInsets.bottom);

        // Consume the insets since the root view already adjusted their paddings.
        return new WindowInsetsCompat.Builder(windowInsets)
                .setInsets(WindowInsetsCompat.Type.statusBars(), Insets.NONE)
                .setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.NONE)
                .build();
    }

    private void ensureInitialized() {
        if (mView != null) return;

        mView =
                (EdgeToEdgeBaseLayout)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.edge_to_edge_base_layout, null, false);
        if (mInsetObserver != null) {
            mInsetObserver.addInsetsConsumer(this);
        } else {
            ViewCompat.setOnApplyWindowInsetsListener(mView, this);
        }

        applyStatusBarColor();
        applyNavBarColor();
        applyNavigationBarDividerColor();
    }
}
