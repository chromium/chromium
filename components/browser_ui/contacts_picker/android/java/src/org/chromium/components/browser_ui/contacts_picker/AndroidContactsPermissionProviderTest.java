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

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.ContactsPermissionProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link AndroidContactsPermissionProviderImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@SuppressWarnings("DoNotMock") // TODO(567604165): Remove mocking of Views / Activities
public class AndroidContactsPermissionProviderTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebContents mWebContents;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Activity mActivity;
    @Mock private ContactsPermissionProvider.Callback mCallback;

    private AndroidContactsPermissionProviderImpl mProvider;

    @Before
    public void setUp() {
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mActivity));
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mActivity.getContentResolver()).thenReturn(null); // Not used in this path

        ContactsPickerFeatureMap.setSystemContactsPickerEnabledForTesting(true);

        mProvider = new AndroidContactsPermissionProviderImpl();
    }

    @Test
    public void testPermissionSkippedWhenSystemPickerEnabled() {
        mProvider.run(mWebContents, mCallback);

        // Should allow without checking or requesting permissions.
        verify(mCallback).onAllowed(any());
        verify(mWindowAndroid, never()).hasPermission(Manifest.permission.READ_CONTACTS);
        verify(mWindowAndroid, never()).requestPermissions(any(), any());
    }

    @Test
    public void testPermissionRequestedWhenSystemPickerDisabled() {
        ContactsPickerFeatureMap.setSystemContactsPickerEnabledForTesting(false);
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
