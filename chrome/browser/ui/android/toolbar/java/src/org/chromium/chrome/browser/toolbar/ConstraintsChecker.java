// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.os.Handler;
import android.os.Looper;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/**
 * Watches a constraints supplier for the next time the browser controls are unlocked,
 * and then tells the {@link ViewResourceAdapter} to generate a resource.
 */
public class ConstraintsChecker implements Callback<Integer> {
    @NonNull private final ViewResourceAdapter mViewResourceAdapter;
    @NonNull private final ObservableSupplier<Integer> mConstraintsSupplier;
    @NonNull private final Handler mHandler;

    /**
     * @param viewResourceAdapter The target to notify when a capture is needed.
     * @param constraintsSupplier The underlying supplier for the state of constraints.
     * @param looper Message loop to post deferred tasks to.
     */
    public ConstraintsChecker(
            @NonNull ViewResourceAdapter viewResourceAdapter,
            @NonNull ObservableSupplier<Integer> constraintsSupplier,
            @NonNull Looper looper) {
        mViewResourceAdapter = viewResourceAdapter;
        mConstraintsSupplier = constraintsSupplier;
        mHandler = new Handler(looper);
    }

    public void destroy() {
        mHandler.removeCallbacksAndMessages(null);
    }

    /**
     * Returns whether the controls are currently locked. When the supplier returns null and we are
     * uncertain if the controls are locked or not, true is also returned here to be safe.
     */
    public boolean areControlsLocked() {
        Integer constraints = mConstraintsSupplier.get();
        return constraints == null
                || constraints.intValue() == org.chromium.cc.input.BrowserControlsState.SHOWN;
    }

    /**
     * Sets up an observer that will request a resource from the {@link ViewResourceAdapter} when
     * the controls become unlocked. Depending on what is going on with the browser, this could take
     * a long time to result in a call.
     */
    public void scheduleRequestResourceOnUnlock() {
        mConstraintsSupplier.addObserver(this);
    }

    @Override
    public void onResult(Integer result) {
        if (!areControlsLocked()) {
            mConstraintsSupplier.removeObserver(this);
            mHandler.post(mViewResourceAdapter::onResourceRequested);
        }
    }
}
