// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import static org.junit.Assert.assertFalse;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.ui.base.WindowAndroid;

/** Robolectric unit tests for {@link PermissionDialogCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PermissionDialogCoordinatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PermissionDialogCoordinator.Delegate mCoordinatorDelegate;
    @Mock private PermissionDialogDelegate mDialogDelegate;
    @Mock private WindowAndroid mWindowAndroid;

    private PermissionDialogCoordinator mCoordinator;

    @Before
    public void setUp() {
        when(mDialogDelegate.getWindow()).thenReturn(mWindowAndroid);
        mCoordinator = new PermissionDialogCoordinator(mCoordinatorDelegate);
    }

    @Test
    public void testDismissFromNative_whenModalDialogManagerNull() {
        when(mWindowAndroid.getModalDialogManager()).thenReturn(null);
        doAnswer(
                        invocation -> {
                            mCoordinator.dismissFromNative();
                            return null;
                        })
                .when(mDialogDelegate)
                .onDismiss(DismissalType.AUTODISMISS_NO_DIALOG_MANAGER);

        assertFalse(mCoordinator.showDialog(mDialogDelegate));
        verify(mCoordinatorDelegate).onPermissionDialogResult(ContentSetting.DEFAULT);
        verify(mCoordinatorDelegate).onPermissionDialogEnded();
    }
}
