// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.notifications;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;

import org.mockito.ArgumentCaptor;

import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.InstrumentationThreadCondition;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/** Condition fulfilled when the Google Lens intent is launched. */
public class LensIntentFulfilledCondition extends InstrumentationThreadCondition {
    private final LensController mLensController;

    public LensIntentFulfilledCondition(LensController lensController) {
        mLensController = lensController;
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        ArgumentCaptor<LensIntentParams> paramsCaptor =
                ArgumentCaptor.forClass(LensIntentParams.class);
        try {
            verify(mLensController, atLeastOnce())
                    .startLens(any(WindowAndroid.class), paramsCaptor.capture());
        } catch (AssertionError e) {
            return notFulfilled("startLens() not yet invoked");
        }
        List<LensIntentParams> captured = paramsCaptor.getAllValues();
        for (LensIntentParams params : captured) {
            if (params.getLensEntryPoint() == LensEntryPoint.TIPS_NOTIFICATIONS) {
                return fulfilled("invocations=%d", captured.size());
            }
        }
        LensIntentParams last = captured.get(captured.size() - 1);
        return notFulfilled(
                "startLens() called %d time(s), last entryPoint=%d",
                captured.size(), last.getLensEntryPoint());
    }

    @Override
    public String buildDescription() {
        return "Lens was opened.";
    }
}
