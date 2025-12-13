// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;

import java.util.function.Supplier;

/**
 * Fulfilled when the Tab has a native page of the expected type.
 *
 * @param <NativePageT> the expected subclass of {@link NativePage}.
 */
public class NativePageCondition<NativePageT extends NativePage>
        extends ConditionWithResult<NativePageT> {
    private final Supplier<Tab> mLoadedTabSupplier;
    private final Class<NativePageT> mNativePageClass;

    public NativePageCondition(
            Class<NativePageT> nativePageClass, Supplier<Tab> loadedTabSupplier) {
        super(/* isRunOnUiThread= */ true);
        mNativePageClass = nativePageClass;
        mLoadedTabSupplier = dependOnSupplier(loadedTabSupplier, "LoadedTab");
    }

    @Override
    public String buildDescription() {
        return "NativePage of type " + mNativePageClass.getSimpleName();
    }

    @Override
    protected ConditionStatusWithResult<NativePageT> resolveWithSuppliers() {
        Tab tab = mLoadedTabSupplier.get();

        NativePage nativePage = tab.getNativePage();
        if (nativePage == null) {
            return notFulfilled("tab.getNativePage() is null").withoutResult();
        }

        if (!mNativePageClass.isAssignableFrom(nativePage.getClass())) {
            return notFulfilled(
                            "native page has [type %s, title \"%s\"], waiting to be %s",
                            nativePage.getClass().getName(),
                            nativePage.getTitle(),
                            mNativePageClass.getName())
                    .withoutResult();
        }

        return fulfilled().withResult(mNativePageClass.cast(nativePage));
    }
}
