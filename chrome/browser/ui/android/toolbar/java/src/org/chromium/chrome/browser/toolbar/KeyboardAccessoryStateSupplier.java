// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;

/**
 * Helper class that provides the current height of the keyboard accessory (sheet or bar), providing
 * 0 until the keyboard accessory's own supplier of its height is available. Also provides the
 * current visibility of the accessory sheet.
 */
@NullMarked
public class KeyboardAccessoryStateSupplier {

    private final Callback<ManualFillingComponent> mManualFillingAvailableCallback =
            this::onManualFillingComponentAvailable;
    private final MonotonicObservableSupplier<ManualFillingComponent>
            mManualFillingComponentSupplier;
    private final Callback<Integer> mInsetChangeCallback = this::onInsetChange;
    private @Nullable ManualFillingComponent mManualFillingComponent;
    private final SettableNonNullObservableSupplier<Boolean> mIsSheetShowingSupplier =
            ObservableSuppliers.createNonNull(false);
    private final View mView;
    private final SettableNonNullObservableSupplier<Integer> mInsetSupplier =
            ObservableSuppliers.createNonNull(0);

    public KeyboardAccessoryStateSupplier(
            MonotonicObservableSupplier<ManualFillingComponent> manualFillingComponentSupplier,
            View view) {
        mManualFillingComponentSupplier = manualFillingComponentSupplier;
        mView = view;
        ManualFillingComponent manualFillingComponent =
                mManualFillingComponentSupplier.addSyncObserverAndPostIfNonNull(
                        mManualFillingAvailableCallback);
        if (manualFillingComponent != null) {
            onManualFillingComponentAvailable(manualFillingComponent);
        }
    }

    private void onInsetChange(Integer value) {
        mInsetSupplier.set(value);
        mIsSheetShowingSupplier.set(
                mManualFillingComponent != null
                        && mManualFillingComponent.isFillingViewShown(mView));
    }

    private void onManualFillingComponentAvailable(ManualFillingComponent manualFillingComponent) {
        mManualFillingComponent = manualFillingComponent;
        mManualFillingComponent
                .getBottomInsetSupplier()
                .addSyncObserverAndPostIfNonNull(mInsetChangeCallback);
        mManualFillingComponentSupplier.removeObserver(mManualFillingAvailableCallback);
    }

    public void destroy() {
        if (mManualFillingComponent != null) {
            mManualFillingComponent.getBottomInsetSupplier().removeObserver(mInsetChangeCallback);
        }
        mInsetSupplier.destroy();
    }

    public NonNullObservableSupplier<Boolean> getIsSheetShowingSupplier() {
        return mIsSheetShowingSupplier;
    }

    public NonNullObservableSupplier<Integer> getInsetSupplier() {
        return mInsetSupplier;
    }
}
