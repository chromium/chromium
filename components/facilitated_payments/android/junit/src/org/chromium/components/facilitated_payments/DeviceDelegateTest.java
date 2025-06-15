// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.facilitated_payments;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Tests for {@link DeviceDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@SmallTest
public class DeviceDelegateTest {
    private static final String GOOGLE_WALLET_PACKAGE_NAME = "com.google.android.apps.walletnfcrel";
    private static final String GOOGLE_WALLET_ADD_PIX_ACCOUNT_LINK =
            "https://wallet.google.com/gw/app/addbankaccount";

    @Rule public MockitoRule mRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mMockWindowAndroid;
    @Mock private Context mMockContext;

    @Before
    public void setUp() {
        when(mMockWindowAndroid.getContext()).thenReturn(new WeakReference<Context>(mMockContext));
    }

    @Test
    public void testOpenPixAccountLinkingPageInWallet_Success() {
        DeviceDelegate.openPixAccountLinkingPageInWallet(mMockWindowAndroid);

        // Capture the Intent passed to startActivity
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockContext).startActivity(intentCaptor.capture());

        // Assert the properties of the captured Intent
        Intent capturedIntent = intentCaptor.getValue();
        assertEquals(Intent.ACTION_VIEW, capturedIntent.getAction());
        assertEquals(Uri.parse(GOOGLE_WALLET_ADD_PIX_ACCOUNT_LINK), capturedIntent.getData());
        assertEquals(GOOGLE_WALLET_PACKAGE_NAME, capturedIntent.getPackage());
    }

    @Test
    public void testOpenPixAccountLinkingPageInWallet_NullWindowAndroid() {
        DeviceDelegate.openPixAccountLinkingPageInWallet(null);

        // Verify that startActivity() was never called if WindowAndroid is null.
        verify(mMockContext, never()).startActivity(any(Intent.class));
    }

    @Test
    public void testOpenPixAccountLinkingPageInWallet_NullContext() {
        when(mMockWindowAndroid.getContext()).thenReturn(new WeakReference<>(null));

        DeviceDelegate.openPixAccountLinkingPageInWallet(mMockWindowAndroid);

        verify(mMockContext, never()).startActivity(any(Intent.class));
    }

    @Test
    public void testOpenPixAccountLinkingPageInWallet_ActivityNotFound() {
        // Simulate ActivityNotFoundException
        doThrow(new ActivityNotFoundException())
                .when(mMockContext)
                .startActivity(any(Intent.class));

        // Call the method, expecting it to catch the exception
        DeviceDelegate.openPixAccountLinkingPageInWallet(mMockWindowAndroid);

        // Verify startActivity was called (even though it threw).
        verify(mMockContext).startActivity(any(Intent.class));
    }
}
