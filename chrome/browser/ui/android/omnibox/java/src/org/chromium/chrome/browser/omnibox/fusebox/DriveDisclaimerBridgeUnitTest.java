// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.contextual_search.DisclaimerStatus;

/** Unit tests for {@link DriveDisclaimerBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DriveDisclaimerBridgeUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private DriveDisclaimerBridge.Natives mBridgeJni;
    @Mock private Profile mProfile;
    @Mock private Callback<@DisclaimerStatus Integer> mCallback;

    @Before
    public void setUp() {
        DriveDisclaimerBridgeJni.setInstanceForTesting(mBridgeJni);
    }

    @Test
    public void checkConsentStatus_delegatesToNative() {
        DriveDisclaimerBridge.checkConsentStatus(mProfile, mCallback);
        verify(mBridgeJni).checkConsentStatus(mProfile, mCallback);
    }
}
