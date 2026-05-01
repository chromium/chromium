// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.Manifest;
import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.content_public.browser.ContactsPermissionProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link AndroidContactsPermissionProviderImpl}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AndroidContactsPermissionProviderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebContents mWebContents;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Activity mActivity;
    @Mock private ContactsPermissionProvider.Callback mCallback;
    @Mock private ContactsPickerFeatureMap mMockFeatureMap;

    private AndroidContactsPermissionProviderImpl mProvider;

    @Before
    public void setUp() {
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mActivity));
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mActivity.getContentResolver()).thenReturn(null); // Not used in this path

        FakeAconfigFlaggedApiDelegate fakeDelegate = new FakeAconfigFlaggedApiDelegate();
        fakeDelegate.setSystemContactsPickerEnabled(true);
        AconfigFlaggedApiDelegate.setInstanceForTesting(fakeDelegate);

        ContactsPickerFeatureMap.setInstanceForTesting(mMockFeatureMap);

        mProvider = new AndroidContactsPermissionProviderImpl();
    }

    @Test
    @SmallTest
    public void testPermissionSkippedWhenSystemPickerEnabled() {
        // Feature is enabled.
        when(mMockFeatureMap.isEnabledInNative(
                        ContactsPickerFeatureList.ANDROID_SYSTEM_CONTACTS_PICKER))
                .thenReturn(true);

        mProvider.run(mWebContents, mCallback);

        // Should allow without checking or requesting permissions.
        verify(mCallback).onAllowed(any());
        verify(mWindowAndroid, never()).hasPermission(Manifest.permission.READ_CONTACTS);
        verify(mWindowAndroid, never()).requestPermissions(any(), any());
    }

    @Test
    @SmallTest
    public void testPermissionRequestedWhenSystemPickerDisabled() {
        // Feature is disabled.
        when(mMockFeatureMap.isEnabledInNative(
                        ContactsPickerFeatureList.ANDROID_SYSTEM_CONTACTS_PICKER))
                .thenReturn(false);
        // Assume permission not granted initially.
        when(mWindowAndroid.hasPermission(Manifest.permission.READ_CONTACTS)).thenReturn(false);
        when(mWindowAndroid.canRequestPermission(Manifest.permission.READ_CONTACTS))
                .thenReturn(true);

        mProvider.run(mWebContents, mCallback);

        // Should request permissions.
        verify(mWindowAndroid).requestPermissions(any(), any());
        // callback.onAllowed is NOT called yet (waiting for async result).
        verify(mCallback, never()).onAllowed(any());
    }
}
