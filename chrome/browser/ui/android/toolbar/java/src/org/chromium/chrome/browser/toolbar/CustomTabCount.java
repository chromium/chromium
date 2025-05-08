// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;

/**
 * CustomTabCount is a wrapper class to supply the current tab count. This class allows us to
 * overwrite the tab count for purposes like animations.
 */
@NullMarked
public class CustomTabCount extends ObservableSupplierImpl<Integer> implements Destroyable {
    private final ObservableSupplier<Integer> mTabModelSelectorTabCountSupplier;
    private final Callback<Integer> mTabCountObserver = this::onUpdateTabCount;
    private boolean mIsCustom;

    /**
     * Creates an instance of {@link CustomTabCount}.
     *
     * @param tabModelSelectorTabCountSupplier Supplier for the current tab count in the current tab
     *     model. It updates as the current tab model changes so it will contain the current tab
     *     count.
     */
    public CustomTabCount(ObservableSupplier<Integer> tabModelSelectorTabCountSupplier) {
        // TODO(crbug.com/40282469): Use {@link TokenHolder}.
        super(tabModelSelectorTabCountSupplier.get());
        mIsCustom = false;
        mTabModelSelectorTabCountSupplier = tabModelSelectorTabCountSupplier;
        mTabModelSelectorTabCountSupplier.addObserver(mTabCountObserver);
    }

    private void onUpdateTabCount(int tabCount) {
        if (!mIsCustom) {
            super.set(mTabModelSelectorTabCountSupplier.get());
        }
    }

    /** Releases the custom tab count and goes back to the real tab count value. */
    public void release() {
        if (mIsCustom) {
            mIsCustom = false;
            super.set(mTabModelSelectorTabCountSupplier.get());
        }
    }

    @Override
    public void set(Integer tabCount) {
        mIsCustom = true;
        super.set(tabCount);
    }

    @Override
    public void destroy() {
        mTabModelSelectorTabCountSupplier.removeObserver(mTabCountObserver);
    }

    public boolean getIsCustomForTesting() {
        return mIsCustom;
    }
}
