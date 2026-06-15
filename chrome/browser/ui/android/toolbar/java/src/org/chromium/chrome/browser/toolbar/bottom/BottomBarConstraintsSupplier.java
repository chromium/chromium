// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils;

/**
 * Computes the {@link BrowserControlsState} constraints for the bottom bar by observing the base
 * constraints, the current tab, and the tab's content state.
 *
 * <p>It tracks transitions to and from the New Tab Page (NTP) by listening to {@link
 * TabObserver#onContentChanged(Tab)}.
 *
 * <p>When the current tab is on an NTP, the constraints emitted by this supplier are overridden and
 * forced to {@link BrowserControlsState#BOTH}. This is exclusively to ensure that
 * ScrollingBottomViewResourceFrameLayout allows screenshot updates, preventing stale screenshots.
 * It does not affect the physical scroll behavior of the bottom bar, which is driven by the actual
 * tab constraints.
 */
@NullMarked
public class BottomBarConstraintsSupplier
        implements NullableObservableSupplier<@BrowserControlsState Integer>, Destroyable {
    private final SettableNullableObservableSupplier<@BrowserControlsState Integer> mSupplier =
            ObservableSuppliers.createNullable();

    private final Callback<@Nullable Integer> mConstraintsObserver = this::onConstraintsChanged;
    private final Callback<@Nullable Tab> mTabObserver = this::onTabChanged;
    private final TabObserver mPageObserver =
            new EmptyTabObserver() {
                // Triggered when a NativePage is swapped in or out (e.g., the New Tab Page).
                // This is the primary way we detect transitions to/from the NTP.
                @Override
                public void onContentChanged(Tab tab) {
                    recalculate();
                }
            };

    private final NullableObservableSupplier<@BrowserControlsState Integer>
            mBrowserControlsStateSupplier;
    private final NullableObservableSupplier<Tab> mCurrentTabSupplier;
    private final Context mContext;
    private @Nullable Tab mCurrentTab;

    /**
     * Constructs a new {@link BottomBarConstraintsSupplier}.
     *
     * @param browserControlsStateSupplier The base supplier for browser controls constraints. This
     *     state dictates whether the browser controls should be SHOWN, HIDDEN, or BOTH (can
     *     scroll). This is the source of truth when constraints are not being overridden.
     * @param currentTabSupplier The supplier for the currently active {@link Tab}. Used to track
     *     when the user switches tabs or when the current tab's content changes.
     * @param context The {@link Context} used to determine bottom bar configuration.
     */
    public BottomBarConstraintsSupplier(
            NullableObservableSupplier<@BrowserControlsState Integer> browserControlsStateSupplier,
            NullableObservableSupplier<Tab> currentTabSupplier,
            Context context) {
        mBrowserControlsStateSupplier = browserControlsStateSupplier;
        mCurrentTabSupplier = currentTabSupplier;
        mContext = context;

        mBrowserControlsStateSupplier.addSyncObserver(mConstraintsObserver);
        mCurrentTabSupplier.addSyncObserver(mTabObserver);
    }

    private void onConstraintsChanged(@Nullable Integer constraints) {
        recalculate();
    }

    private void onTabChanged(@Nullable Tab tab) {
        if (mCurrentTab == tab) return;
        if (mCurrentTab != null) {
            mCurrentTab.removeObserver(mPageObserver);
        }
        mCurrentTab = tab;
        if (tab != null) {
            tab.addObserver(mPageObserver);
        }
        recalculate();
    }

    private void recalculate() {
        @BrowserControlsState Integer constraints = mBrowserControlsStateSupplier.get();
        if (constraints == null) {
            mSupplier.set(null);
            return;
        }

        @BrowserControlsState int newConstraints = constraints;
        if (BottomBarConfigUtils.shouldForceBothConstraintsForBottomControls(
                mCurrentTab, mContext)) {
            newConstraints = BrowserControlsState.BOTH;
        }
        mSupplier.set(newConstraints);
    }

    @Override
    public void destroy() {
        mBrowserControlsStateSupplier.removeObserver(mConstraintsObserver);
        mCurrentTabSupplier.removeObserver(mTabObserver);
        if (mCurrentTab != null) {
            mCurrentTab.removeObserver(mPageObserver);
            mCurrentTab = null;
        }
        mSupplier.destroy();
    }

    @Override
    public @Nullable @BrowserControlsState Integer addObserver(
            Callback<@Nullable Integer> obs, int behavior) {
        return mSupplier.addObserver(obs, behavior);
    }

    @Override
    public void removeObserver(Callback<@Nullable Integer> obs) {
        mSupplier.removeObserver(obs);
    }

    @Override
    public int getObserverCount() {
        return mSupplier.getObserverCount();
    }

    @Override
    public @Nullable @BrowserControlsState Integer get() {
        return mSupplier.get();
    }
}
