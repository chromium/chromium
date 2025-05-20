// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;

/**
 * Helper class that provides the current height of the keyboard accessory (sheet or bar), providing
 * 0 until the keyboard accessory's own supplier of its height is available. Also provides the
 * current visibility of the accessory sheet.
 */
@NullMarked
public class KeyboardAccessoryStateSupplier extends ObservableSupplierImpl<Integer> {

    private final Callback<ManualFillingComponent> mManualFillingAvailableCallback =
            this::onManualFillingComponentAvailable;
    private final ObservableSupplier<ManualFillingComponent> mManualFillingComponentSupplier;
    private final Callback<Integer> mInsetChangeCallback = this::set;
    private @Nullable ManualFillingComponent mManualFillingComponent;
    private final ObservableSupplierImpl<Boolean> mIsSheetShowingSupplier =
            new ObservableSupplierImpl<>(false);
    private final View mView;

    public KeyboardAccessoryStateSupplier(
            ObservableSupplier<ManualFillingComponent> manualFillingComponentSupplier, View view) {
        super(0);
        mManualFillingComponentSupplier = manualFillingComponentSupplier;
        mView = view;
        ManualFillingComponent manualFillingComponent =
                mManualFillingComponentSupplier.addObserver(mManualFillingAvailableCallback);
        if (manualFillingComponent != null) {
            onManualFillingComponentAvailable(manualFillingComponent);
        }
    }

    @Override
    public void set(Integer object) {
        super.set(object);
        mIsSheetShowingSupplier.set(
                mManualFillingComponent != null
                        && mManualFillingComponent.isFillingViewShown(mView));
    }

    private void onManualFillingComponentAvailable(ManualFillingComponent manualFillingComponent) {
        mManualFillingComponent = manualFillingComponent;
        mManualFillingComponent.getBottomInsetSupplier().addObserver(mInsetChangeCallback);
        mManualFillingComponentSupplier.removeObserver(mManualFillingAvailableCallback);
    }

    public void destroy() {
        if (mManualFillingComponent != null) {
            mManualFillingComponent.getBottomInsetSupplier().removeObserver(mInsetChangeCallback);
        }
    }

    public ObservableSupplierImpl<Boolean> getIsSheetShowingSupplier() {
        return mIsSheetShowingSupplier;
    }
}
