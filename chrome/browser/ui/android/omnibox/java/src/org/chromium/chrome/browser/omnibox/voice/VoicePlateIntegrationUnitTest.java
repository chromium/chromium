// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;

import android.content.Context;

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
    @Mock private VoicePlateIntegration mIntegration;

    private final Context mContext = ApplicationProvider.getApplicationContext();
    private final VoicePlateIntegration mDefaultIntegration = new VoicePlateIntegration();

    @Test
    public void createIntent_defaultImplementation_returnsNull() {
        assertNull(mDefaultIntegration.createIntent(mContext, mProfile));
    }

    @Test
    public void getInstance_returnsTestInstance() {
        VoicePlateIntegration.setInstanceForTesting(mIntegration);
        assertSame(mIntegration, VoicePlateIntegration.getInstance());
    }

    @Test
    public void getInstance_returnsSameInstance() {
        assertSame(VoicePlateIntegration.getInstance(), VoicePlateIntegration.getInstance());
    }
}
