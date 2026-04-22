// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
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
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link AtMemoryBottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class AtMemoryBottomSheetMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AtMemoryBottomSheetCoordinator.Delegate mDelegate;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private AtMemoryBottomSheetContent mContent;

    private PropertyModel mModel;
    private AtMemoryBottomSheetMediator mMediator;

    @Before
    public void setUp() {
        mModel =
                new PropertyModel.Builder(AtMemoryBottomSheetProperties.ALL_KEYS)
                        .with(AtMemoryBottomSheetProperties.VISIBLE, false)
                        .build();
        mMediator = new AtMemoryBottomSheetMediator();
        mMediator.initialize(mDelegate, mModel, mBottomSheetController, mContent);
    }

    @Test
    public void testShow() {
        when(mBottomSheetController.requestShowContent(mContent, true)).thenReturn(true);
        mMediator.show();
        verify(mBottomSheetController).requestShowContent(mContent, true);
        assertTrue(mModel.get(AtMemoryBottomSheetProperties.VISIBLE));
    }

    @Test
    public void testShow_Failed() {
        when(mBottomSheetController.requestShowContent(mContent, true)).thenReturn(false);
        mMediator.show();
        assertFalse(mModel.get(AtMemoryBottomSheetProperties.VISIBLE));
        verify(mDelegate).onDismissed();
    }

    @Test
    public void testOnDismissed() {
        mModel.set(AtMemoryBottomSheetProperties.VISIBLE, true);
        mMediator.onDismissed();
        assertFalse(mModel.get(AtMemoryBottomSheetProperties.VISIBLE));
        verify(mDelegate).onDismissed();
    }
}
