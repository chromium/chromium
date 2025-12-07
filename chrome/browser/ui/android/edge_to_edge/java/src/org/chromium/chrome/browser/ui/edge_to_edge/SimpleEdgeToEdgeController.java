// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static androidx.core.view.WindowInsetsCompat.Type.navigationBars;

import static org.chromium.ui.insets.InsetObserver.WindowInsetsConsumer.InsetConsumerSource.EDGE_TO_EDGE_CONTROLLER_IMPL;

import android.content.Context;
import android.view.View;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.ui.insets.InsetObserver;

/**
 * A simple implementation of the {@link EdgeToEdgeController} that consumes the bottom navigation
 * bar insets and makes them available to pad views containing content.
 */
@NullMarked
public class SimpleEdgeToEdgeController implements EdgeToEdgeController {
    private final InsetObserver mInsetObserver;
    private final InsetObserver.WindowInsetsConsumer mWindowInsetsConsumer;
    private final ObserverList<ChangeObserver> mObservers = new ObserverList<>();
    private final ObserverList<EdgeToEdgePadAdjuster> mAdjusters = new ObserverList<>();

    /** Multiplier to convert from pixels to DPs. */
    private final float mPxToDp;

    private @Px int mBottomInset;

    /**
     * Creates a simple edge-to-edge controller for consuming bottom navigation bar insets and
     * padding views accordingly.
     *
     * @param context The context of the host view.
     * @param insetObserver The {@link InsetObserver} for observing the system's window insets.
     */
    public SimpleEdgeToEdgeController(Context context, InsetObserver insetObserver) {
        mPxToDp = 1.f / context.getResources().getDisplayMetrics().density;
        mInsetObserver = insetObserver;
        mWindowInsetsConsumer = this::onApplyWindowInsets;
        mInsetObserver.addInsetsConsumer(mWindowInsetsConsumer, EDGE_TO_EDGE_CONTROLLER_IMPL);
        mInsetObserver.retriggerOnApplyWindowInsets();
    }

    @VisibleForTesting
    WindowInsetsCompat onApplyWindowInsets(View view, WindowInsetsCompat windowInsetsCompat) {
        Insets navigationBarInsets = windowInsetsCompat.getInsets(navigationBars());
        if (mBottomInset != navigationBarInsets.bottom) {
            mBottomInset = navigationBarInsets.bottom;
            updateObservers();
            updatePadAdjusters();
        }
        return new WindowInsetsCompat.Builder(windowInsetsCompat)
                .setInsets(
                        navigationBars(),
                        Insets.of(
                                navigationBarInsets.left,
                                navigationBarInsets.top,
                                navigationBarInsets.right,
                                0))
                .build();
    }

    @Override
    public void destroy() {
        if (mInsetObserver != null) {
            mInsetObserver.removeInsetsConsumer(mWindowInsetsConsumer);
        }
    }

    @Override
    public int getBottomInset() {
        return (int) Math.ceil(mBottomInset * mPxToDp);
    }

    @Override
    public int getBottomInsetPx() {
        return mBottomInset;
    }

    @Override
    public int getSystemBottomInsetPx() {
        return mBottomInset;
    }

    @Override
    public boolean isDrawingToEdge() {
        return mBottomInset > 0;
    }

    @Override
    public boolean isPageOptedIntoEdgeToEdge() {
        return isDrawingToEdge();
    }

    @Override
    public void registerObserver(ChangeObserver changeObserver) {
        mObservers.addObserver(changeObserver);
        changeObserver.onToEdgeChange(mBottomInset, isDrawingToEdge(), isPageOptedIntoEdgeToEdge());
    }

    @Override
    public void unregisterObserver(ChangeObserver changeObserver) {
        mObservers.removeObserver(changeObserver);
    }

    @Override
    public void registerAdjuster(EdgeToEdgePadAdjuster adjuster) {
        mAdjusters.addObserver(adjuster);
        adjuster.overrideBottomInset(mBottomInset);
    }

    @Override
    public void unregisterAdjuster(EdgeToEdgePadAdjuster adjuster) {
        mAdjusters.removeObserver(adjuster);
    }

    private void updateObservers() {
        for (ChangeObserver observer : mObservers) {
            observer.onToEdgeChange(mBottomInset, isDrawingToEdge(), isPageOptedIntoEdgeToEdge());
        }
    }

    private void updatePadAdjusters() {
        for (EdgeToEdgePadAdjuster adjuster : mAdjusters) {
            adjuster.overrideBottomInset(mBottomInset);
        }
    }
}
