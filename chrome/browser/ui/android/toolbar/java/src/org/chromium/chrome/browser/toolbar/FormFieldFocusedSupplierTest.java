// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;

/** Unit tests for {@link FormFieldFocusedSupplier}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FormFieldFocusedSupplierTest {
    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();
    @Mock private WebContentsImpl mWebContents;
    @Mock private WebContentsImpl mOtherWebContents;
    @Mock private ImeAdapterImpl mImeAdapter;
    private FormFieldFocusedSupplier mFormFieldFocusedSupplier;

    @Before
    public void setUp() {
        doReturn(mImeAdapter).when(mWebContents).getOrSetUserData(eq(ImeAdapterImpl.class), any());
        doReturn(mImeAdapter)
                .when(mOtherWebContents)
                .getOrSetUserData(eq(ImeAdapterImpl.class), any());
        mFormFieldFocusedSupplier = new FormFieldFocusedSupplier();
    }

    @Test
    public void testWebContentsChanged() {
        assertFalse(mFormFieldFocusedSupplier.get());

        mFormFieldFocusedSupplier.onWebContentsChanged(mWebContents);
        verify(mImeAdapter).addEventObserver(mFormFieldFocusedSupplier);

        mFormFieldFocusedSupplier.onNodeAttributeUpdated(true, false);
        assertTrue(mFormFieldFocusedSupplier.get());

        mFormFieldFocusedSupplier.onWebContentsChanged(mOtherWebContents);
        assertFalse(mFormFieldFocusedSupplier.get());

        mFormFieldFocusedSupplier.onNodeAttributeUpdated(true, false);
        assertTrue(mFormFieldFocusedSupplier.get());

        mFormFieldFocusedSupplier.onWebContentsChanged(null);
        verify(mImeAdapter, times(2)).removeEventObserver(mFormFieldFocusedSupplier);
        assertFalse(mFormFieldFocusedSupplier.get());
    }
}
