// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;

/** Unit tests for {@link VoicePlateIntegration}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VoicePlateIntegrationUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private Profile mProfile;
    @Mock private VoicePlateIntegration mMockIntegration;

    private final Context mContext = ApplicationProvider.getApplicationContext();
    private final VoicePlateIntegration mIntegration = new VoicePlateIntegration();

    @Test
    public void isEligible_defaultImplementation_returnsFalse() {
        assertFalse(mIntegration.isEligible(mProfile));
        assertFalse(mIntegration.isEligible(/* profile= */ null));
    }

    @Test
    public void isServiceRefused_defaultImplementation_returnsTrue() {
        assertTrue(mIntegration.isServiceRefused(Activity.RESULT_OK, new Intent()));
        assertTrue(mIntegration.isServiceRefused(Activity.RESULT_CANCELED, new Intent()));
        assertTrue(mIntegration.isServiceRefused(Activity.RESULT_CANCELED, /* data= */ null));
    }

    @Test
    public void isVoiceSearchStopped_defaultImplementation_returnsFalse() {
        assertFalse(mIntegration.isVoiceSearchStopped(new Intent()));
        assertFalse(mIntegration.isVoiceSearchStopped(/* data= */ null));
    }

    @Test
    public void createIntent_defaultImplementation_returnsNull() {
        assertNull(mIntegration.createIntent(mContext, mProfile));
    }

    @Test
    public void getInstance_returnsTestInstance() {
        VoicePlateIntegration.setInstanceForTesting(mMockIntegration);
        assertSame(mMockIntegration, VoicePlateIntegration.getInstance());
    }

    @Test
    public void getInstance_returnsSameInstance() {
        assertSame(VoicePlateIntegration.getInstance(), VoicePlateIntegration.getInstance());
    }
}
