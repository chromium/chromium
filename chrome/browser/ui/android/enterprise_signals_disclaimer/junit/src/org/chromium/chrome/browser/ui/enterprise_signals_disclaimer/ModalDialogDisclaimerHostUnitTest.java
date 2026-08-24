// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Unit tests for {@link ModalDialogDisclaimerHost}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ModalDialogDisclaimerHostUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private EnterpriseSignalsDisclaimerView mView;

    private ModalDialogDisclaimerHost mHost;

    @Before
    public void setUp() {
        mHost = new ModalDialogDisclaimerHost(mModalDialogManager, mView);
    }

    @Test
    public void testShow_showsDialogAndSetsActive() {
        Assert.assertFalse(mHost.isActive());

        mHost.show();

        Assert.assertTrue(mHost.isActive());
        verify(mModalDialogManager)
                .showDialog(
                        any(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(ModalDialogManager.ModalDialogPriority.HIGH));
    }

    @Test
    public void testHide_dismissesDialogAndSetsInactive() {
        mHost.show();
        Assert.assertTrue(mHost.isActive());

        mHost.hide();

        Assert.assertFalse(mHost.isActive());
        verify(mModalDialogManager)
                .dismissDialog(any(), eq(DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED));
    }

    @Test
    public void testOnDismiss_setsInactive() {
        mHost.show();
        Assert.assertTrue(mHost.isActive());

        mHost.onDismiss(null, DialogDismissalCause.NAVIGATE_BACK);

        Assert.assertFalse(mHost.isActive());
    }
}
