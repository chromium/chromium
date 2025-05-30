// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;

/** Unit tests for {@link SecurePaymentConfirmationBottomSheetObserver} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SecurePaymentConfirmationBottomSheetObserverTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private SecurePaymentConfirmationBottomSheetObserver.ControllerDelegate mDelegate;

    private SecurePaymentConfirmationBottomSheetObserver mObserver;

    @Before
    public void setUp() {
        mObserver = new SecurePaymentConfirmationBottomSheetObserver(mBottomSheetController);
        mObserver.begin(mDelegate);
    }

    @Test
    public void testBegin() {
        // mObserver.begin(mDelegate) is called in setUp().
        verify(mBottomSheetController).addObserver(eq(mObserver));
    }

    @Test
    public void testEnd() {
        mObserver.end();

        verify(mBottomSheetController).removeObserver(eq(mObserver));
    }

    @Test
    public void testBeingAfterEnd() {
        mObserver.end();
        mObserver.begin(mDelegate);
    }

    @Test
    public void testSheetClosed_whenInteractionComplete() {
        mObserver.onSheetClosed(StateChangeReason.INTERACTION_COMPLETE);
        verifyNoInteractions(mDelegate);
    }

    @Test
    public void testSheetClosed_whenNotInteractionComplete() {
        mObserver.onSheetClosed(StateChangeReason.SWIPE);
        verify(mDelegate).onCancel();
    }

    @Test
    public void testSheetClosed_whenCalledMultipleTimes() {
        mObserver.onSheetClosed(StateChangeReason.SWIPE);
        clearInvocations(mDelegate);

        for (@StateChangeReason int stateChangeReason = 0;
                stateChangeReason < StateChangeReason.MAX_VALUE;
                stateChangeReason++) {
            mObserver.onSheetClosed(stateChangeReason);
        }

        verifyNoInteractions(mDelegate);
    }
}
