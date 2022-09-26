// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.prefs.PrefService;

/**
 * Tests for {@link AutofillAssistantPreferenceManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AutofillAssistantPreferenceManagerTest {
    @Mock
    private PrefService mPrefService;

    private AutofillAssistantPreferenceManager mPreferenceManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mPreferenceManager = new AutofillAssistantPreferenceManager(mPrefService);
    }

    @Test
    public void acceptOnboardingSetsCorrectPreferences() {
        mPreferenceManager.setOnboardingAccepted(true);
        verify(mPrefService)
                .setBoolean(AutofillAssistantPreferenceManager.AUTOFILL_ASSISTANT_CONSENT, true);
        verify(mPrefService)
                .setBoolean(AutofillAssistantPreferenceManager.AUTOFILL_ASSISTANT_ENABLED, true);
    }

    @Test
    public void declineOnboardingSetsCorrectPreferences() {
        mPreferenceManager.setOnboardingAccepted(false);
        verify(mPrefService)
                .setBoolean(AutofillAssistantPreferenceManager.AUTOFILL_ASSISTANT_CONSENT, false);
        verify(mPrefService, never())
                .setBoolean(AutofillAssistantPreferenceManager.AUTOFILL_ASSISTANT_ENABLED, true);
    }

    @Test
    public void getOnboardingConsent() {
        boolean expected = true;
        doReturn(expected)
                .when(mPrefService)
                .getBoolean(AutofillAssistantPreferenceManager.AUTOFILL_ASSISTANT_CONSENT);

        assertEquals(expected, mPreferenceManager.getOnboardingConsent());
    }
}
