// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;
import android.os.Build;
import android.os.Looper;

import com.google.android.gms.identitycredentials.CreateCredentialHandle;
import com.google.android.gms.identitycredentials.IdentityCredentialClient;
import com.google.android.gms.tasks.TaskCompletionSource;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.webid.IdentityCredentialsDelegate.DigitalCredential;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link DigitalCredentialsCreationDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = {Build.VERSION_CODES.TIRAMISU, Build.VERSION_CODES.UPSIDE_DOWN_CAKE})
public class DigitalCredentialsCreationDelegateTest {

    @Test
    public void testCreateWithDeadActivityReturnsRejected() {
        WindowAndroid windowAndroid = mock(WindowAndroid.class);
        when(windowAndroid.getActivity()).thenReturn(new ImmutableWeakReference<>(null));

        DigitalCredentialsCreationDelegate delegate = new DigitalCredentialsCreationDelegate();
        Promise<DigitalCredential> promise =
                delegate.create(windowAndroid, "https://rp.example", "{\"requests\":[]}");

        assertTrue(promise.isRejected());
    }

    @Test
    public void testCreateWithDestroyedWindowReturnsRejected() {
        WindowAndroid windowAndroid = mock(WindowAndroid.class);
        when(windowAndroid.isDestroyed()).thenReturn(true);

        DigitalCredentialsCreationDelegate delegate = new DigitalCredentialsCreationDelegate();
        Promise<DigitalCredential> promise =
                delegate.create(windowAndroid, "https://rp.example", "{\"requests\":[]}");

        assertTrue(promise.isRejected());
    }

    @Test
    public void testCreateWhenWindowDestroyedInFlightRejectsAndSkipsIntent() {
        Activity activity = Robolectric.buildActivity(Activity.class).get();
        WindowAndroid windowAndroid = mock(WindowAndroid.class);
        when(windowAndroid.getActivity()).thenReturn(new ImmutableWeakReference<>(activity));
        when(windowAndroid.isDestroyed()).thenReturn(false);

        IdentityCredentialClient mockClient = mock(IdentityCredentialClient.class);
        TaskCompletionSource<CreateCredentialHandle> taskSource = new TaskCompletionSource<>();
        when(mockClient.createCredential(any())).thenReturn(taskSource.getTask());

        DigitalCredentialsCreationDelegate delegate =
                new DigitalCredentialsCreationDelegate() {
                    @Override
                    IdentityCredentialClient getIdentityCredentialClient(Activity window) {
                        return mockClient;
                    }
                };

        Promise<DigitalCredential> promise =
                delegate.create(windowAndroid, "https://rp.example", "{\"requests\":[]}");
        assertTrue(promise.isPending());

        when(windowAndroid.isDestroyed()).thenReturn(true);
        PendingIntent pendingIntent =
                PendingIntent.getActivity(activity, 0, new Intent(), PendingIntent.FLAG_IMMUTABLE);
        taskSource.setResult(new CreateCredentialHandle(pendingIntent, null));
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        assertTrue(promise.isRejected());
        verify(windowAndroid, never()).showCancelableIntent(any(PendingIntent.class), any(), any());
    }
}
