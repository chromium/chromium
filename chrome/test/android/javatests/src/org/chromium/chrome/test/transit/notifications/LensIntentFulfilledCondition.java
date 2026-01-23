// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.notifications;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;

import static org.chromium.base.test.transit.Condition.whether;

import org.mockito.ArgumentCaptor;

import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.InstrumentationThreadCondition;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;

/** Condition fulfilled when the Google Lens intent is launched. */
public class LensIntentFulfilledCondition extends InstrumentationThreadCondition {
    private final LensController mLensController;
    private final ArgumentCaptor<LensIntentParams> mLensIntentParamsCaptor =
            ArgumentCaptor.forClass(LensIntentParams.class);

    public LensIntentFulfilledCondition(LensController lensController) {
        mLensController = lensController;
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        try {
            verify(mLensController).startLens(any(), mLensIntentParamsCaptor.capture());
            return whether(
                    mLensIntentParamsCaptor.getValue() != null
                            && mLensIntentParamsCaptor.getValue().getLensEntryPoint()
                                    == LensEntryPoint.TIPS_NOTIFICATIONS);
        } catch (AssertionError e) {
            return notFulfilled();
        }
    }

    @Override
    public String buildDescription() {
        return "Lens was opened.";
    }
}
