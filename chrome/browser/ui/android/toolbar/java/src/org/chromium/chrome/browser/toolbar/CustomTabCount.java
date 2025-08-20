// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.util.TokenHolder;

/**
 * CustomTabCount is a wrapper class to supply the current tab count. This class allows us to
 * overwrite the tab count for purposes like animations.
 */
@NullMarked
public class CustomTabCount extends ObservableSupplierImpl<Integer> implements Destroyable {
    private final ObservableSupplier<Integer> mTabModelSelectorTabCountSupplier;
    private final Callback<Integer> mTabCountObserver = this::onUpdateTabCount;
    private final TokenHolder mTokenHolder;

    /**
     * Creates an instance of {@link CustomTabCount}.
     *
     * @param tabModelSelectorTabCountSupplier Supplier for the current tab count in the current tab
     *     model. It updates as the current tab model changes so it will contain the current tab
     *     count.
     */
    public CustomTabCount(ObservableSupplier<Integer> tabModelSelectorTabCountSupplier) {
        super(tabModelSelectorTabCountSupplier.get());
        mTabModelSelectorTabCountSupplier = tabModelSelectorTabCountSupplier;
        mTabModelSelectorTabCountSupplier.addObserver(mTabCountObserver);
        mTokenHolder = new TokenHolder(this::onTokenChanged);
    }

    private void onUpdateTabCount(int tabCount) {
        if (!mTokenHolder.hasTokens()) {
            super.set(mTabModelSelectorTabCountSupplier.get());
        }
    }

    private void onTokenChanged() {
        if (!mTokenHolder.hasTokens()) {
            super.set(mTabModelSelectorTabCountSupplier.get());
        }
    }

    /**
     * Sets a custom tab count.
     *
     * @param tabCount The tab count to set.
     */
    public int setCount(int tabCount) {
        super.set(tabCount);
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
    public void set(Integer tabCount) {
        assert false : "Use setCount instead.";
    }

    @Override
    public void destroy() {
        mTabModelSelectorTabCountSupplier.removeObserver(mTabCountObserver);
    }

    public boolean hasTokensForTesting() {
        return mTokenHolder.hasTokens();
    }
}
