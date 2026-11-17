// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.JniOnceCallback;
import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/** Unit tests for {@link DrivePickerBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DrivePickerBridgeUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Profile mProfile;
    @Mock private DriveFilePickerClient mPickerClient;
    @Mock private JniOnceCallback<List<DriveAttachmentMetadata>> mCallback;

    @Before
    public void setUp() {
        DriveFilePickerClient.setInstanceForTesting(mPickerClient);
    }

    private Promise<List<DriveAttachmentMetadata>> stubPicker() {
        var promise = new Promise<List<DriveAttachmentMetadata>>();
        when(mPickerClient.launchPicker(
                        mWindowAndroid,
                        mProfile,
                        /* mimeTypes= */ null,
                        /* allowMultiSelect= */ true))
                .thenReturn(promise);
        return promise;
    }

    @Test
    public void isAvailable_delegatesToClient() {
        when(mPickerClient.isAvailable(any(), eq(mProfile))).thenReturn(true);

        assertTrue(DrivePickerBridge.isAvailable(mProfile));
    }

    @Test
    public void start_withoutConsent_launchesPickerAndReturnsFiles() {
        var file = new DriveAttachmentMetadata("id-1", "rk-1", "Doc", "application/pdf");
        var promise = stubPicker();

        DrivePickerBridge bridge =
                DrivePickerBridge.start(
                        mWindowAndroid, mProfile, /* needsConsent= */ false, mCallback);
        assertNotNull(bridge);

        promise.fulfill(List.of(file));
        ShadowLooper.idleMainLooper();

        verify(mCallback).onResult(List.of(file));
    }

    @Test
    public void start_pickerRejected_returnsEmptyList() {
        var promise = stubPicker();

        DrivePickerBridge.start(mWindowAndroid, mProfile, /* needsConsent= */ false, mCallback);
        promise.reject(new RuntimeException("failed"));
        ShadowLooper.idleMainLooper();

        verify(mCallback).onResult(List.of());
    }

    @Test
    public void start_withConsentWhenWindowUnavailable_returnsNullAndEmptyList() {
        DrivePickerBridge bridge =
                DrivePickerBridge.start(
                        mWindowAndroid, mProfile, /* needsConsent= */ true, mCallback);

        assertNull(bridge);
        verify(mPickerClient, never())
                .launchPicker(any(), any(), any(), /* allowMultiSelect= */ eq(true));
        verify(mCallback).onResult(List.of());
    }

    @Test
    public void destroy_whilePickerPending_destroysCallbackWithoutResult() {
        var promise = stubPicker();

        DrivePickerBridge bridge =
                DrivePickerBridge.start(
                        mWindowAndroid, mProfile, /* needsConsent= */ false, mCallback);
        assertNotNull(bridge);
        bridge.destroy();

        promise.fulfill(List.of());
        ShadowLooper.idleMainLooper();

        verify(mCallback).destroy();
        verify(mCallback, never()).onResult(any());
    }
}
