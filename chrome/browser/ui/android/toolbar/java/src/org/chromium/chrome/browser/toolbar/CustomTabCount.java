// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.util.TokenHolder;

/**
 * CustomTabCount is a wrapper class to supply the current tab count. This class allows us to
 * overwrite the tab count for purposes like animations.
 */
@NullMarked
public class CustomTabCount implements Destroyable {
    private final MonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final Callback<TabModelSelector> mTabModelSelectorObserver =
            this::onTabModelSelectorAvailable;
    private final Callback<Integer> mTabCountObserver = this::onUpdateTabCount;
    private final TokenHolder mTokenHolder;
    private final SettableNonNullObservableSupplier<Integer> mSupplier =
            ObservableSuppliers.createNonNull(0);
    private @Nullable NonNullObservableSupplier<Integer> mTabModelSelectorTabCountSupplier;

    /**
     * Creates an instance of {@link CustomTabCount}.
     *
     * @param tabModelSelectorSupplier Supplier for the {@link TabModelSelector}.
     */
    public CustomTabCount(MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mTabModelSelectorSupplier.addSyncObserverAndCallIfNonNull(mTabModelSelectorObserver);
        mTokenHolder = new TokenHolder(this::onTokenChanged);
    }

    private void onTabModelSelectorAvailable(TabModelSelector tabModelSelector) {
        mTabModelSelectorSupplier.removeObserver(mTabModelSelectorObserver);
        mTabModelSelectorTabCountSupplier = tabModelSelector.getCurrentModelTabCountSupplier();
        mTabModelSelectorTabCountSupplier.addSyncObserverAndPostIfNonNull(mTabCountObserver);
    }

    private void onUpdateTabCount(int tabCount) {
        if (!mTokenHolder.hasTokens()) {
            assumeNonNull(mTabModelSelectorTabCountSupplier);
            mSupplier.set(mTabModelSelectorTabCountSupplier.get());
        }
    }

    private void onTokenChanged() {
        if (!mTokenHolder.hasTokens()) {
            assumeNonNull(mTabModelSelectorTabCountSupplier);
            mSupplier.set(mTabModelSelectorTabCountSupplier.get());
        }
    }

    /**
     * Sets a custom tab count.
     *
     * @param tabCount The tab count to set.
     */
    public int setCount(int tabCount) {
        mSupplier.set(tabCount);
        return mTokenHolder.acquireToken();
    }

    /**
     * Releases the custom tab count and goes back to the real tab count value if all tokens are
     * released.
     *
     * @param token The token to release.
     */
    public void releaseCount(int token) {
        mTokenHolder.releaseToken(token);
    }

    @Override
    public void destroy() {
        if (mTabModelSelectorTabCountSupplier != null) {
            mTabModelSelectorTabCountSupplier.removeObserver(mTabCountObserver);
        }
        mSupplier.destroy();
    }

    public boolean hasTokensForTesting() {
        return mTokenHolder.hasTokens();
    }

    public int get() {
        return mSupplier.get();
    }

    public NonNullObservableSupplier<Integer> getObservable() {
        return mSupplier;
    }
}
