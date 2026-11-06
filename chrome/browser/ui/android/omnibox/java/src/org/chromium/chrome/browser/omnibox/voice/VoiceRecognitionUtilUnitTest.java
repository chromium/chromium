// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import android.Manifest;
import android.content.ComponentName;
import android.content.IntentFilter;
import android.speech.RecognizerIntent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.permissions.AndroidPermissionDelegate;

/** Unit Test for {@link VoiceRecognitionUtil}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VoiceRecognitionUtilUnitTest {
    private static final String RECOGNITION_PACKAGE_NAME = "com.some.package";
    private static final ComponentName RECOGNITION_ACTIVITY =
            new ComponentName(RECOGNITION_PACKAGE_NAME, "com.some.package.RecognitionActivity");

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private Profile mProfile1;
    @Mock private Profile mProfile2;
    @Mock private PrefService mPrefs1;
    @Mock private PrefService mPrefs2;
    @Mock private UserPrefs.Natives mUserPrefsJni;
    @Mock private AndroidPermissionDelegate mPermissionDelegate;

    private ShadowPackageManager mShadowPackageManager;

    @Before
    public void setUp() {
        mShadowPackageManager =
                Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager());
    }

    @After
    public void tearDown() {
        setSpeechRecognitionIntentHandlerAvailable(false);
    }

    private void setSpeechRecognitionIntentHandlerAvailable(boolean isAvailable) {
        if (isAvailable) {
            mShadowPackageManager.addActivityIfNotPresent(RECOGNITION_ACTIVITY);
            mShadowPackageManager.addIntentFilterForActivity(
                    RECOGNITION_ACTIVITY,
                    new IntentFilter(RecognizerIntent.ACTION_RECOGNIZE_SPEECH));
        } else {
            mShadowPackageManager.removeActivity(RECOGNITION_ACTIVITY);
        }
    }

    @Test
    public void testSpeechFeatureAvailable() {
        setSpeechRecognitionIntentHandlerAvailable(true);
        assertTrue(VoiceRecognitionUtil.isRecognitionIntentPresent(/* useCachedValue= */ false));
    }

    @Test
    public void testSpeechFeatureUnavailable() {
        setSpeechRecognitionIntentHandlerAvailable(false);
        assertFalse(VoiceRecognitionUtil.isRecognitionIntentPresent(/* useCachedValue= */ false));
    }

    @Test
    public void testCachedSpeechFeatureAvailability() {
        // Initial call will cache the fact that speech is recognized.
        setSpeechRecognitionIntentHandlerAvailable(true);
        assertTrue(VoiceRecognitionUtil.isRecognitionIntentPresent(/* useCachedValue= */ false));

        // Pass a context that does not recognize speech, but use cached result
        // which does recognize speech.
        setSpeechRecognitionIntentHandlerAvailable(false);
        assertTrue(VoiceRecognitionUtil.isRecognitionIntentPresent(/* useCachedValue= */ true));

        // Check if we can turn cached result off again.
        assertFalse(VoiceRecognitionUtil.isRecognitionIntentPresent(/* useCachedValue= */ false));
    }

    @Test
    public void testIsVoiceSearchPermittedByPolicy_perProfileIsolation() {
        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);
        doReturn(mProfile1).when(mProfile1).getOriginalProfile();
        doReturn(mProfile2).when(mProfile2).getOriginalProfile();
        doReturn(mPrefs1).when(mUserPrefsJni).get(mProfile1);
        doReturn(mPrefs2).when(mUserPrefsJni).get(mProfile2);
        doReturn(true).when(mPrefs1).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);
        doReturn(false).when(mPrefs2).getBoolean(Pref.AUDIO_CAPTURE_ALLOWED);

        assertTrue(
                VoiceRecognitionUtil.isVoiceSearchPermittedByPolicy(
                        /* profile= */ null, /* strictPolicyCheck= */ false));
        assertFalse(
                VoiceRecognitionUtil.isVoiceSearchPermittedByPolicy(
                        /* profile= */ null, /* strictPolicyCheck= */ true));
        assertTrue(
                VoiceRecognitionUtil.isVoiceSearchPermittedByPolicy(
                        mProfile1, /* strictPolicyCheck= */ true));
        assertFalse(
                VoiceRecognitionUtil.isVoiceSearchPermittedByPolicy(
                        mProfile2, /* strictPolicyCheck= */ true));

        setSpeechRecognitionIntentHandlerAvailable(true);
        VoiceRecognitionUtil.isRecognitionIntentPresent(/* useCachedValue= */ false);
        doReturn(true).when(mPermissionDelegate).hasPermission(Manifest.permission.RECORD_AUDIO);

        assertTrue(VoiceRecognitionUtil.isVoiceSearchEnabled(mPermissionDelegate, mProfile1));
        assertFalse(VoiceRecognitionUtil.isVoiceSearchEnabled(mPermissionDelegate, mProfile2));
    }

    @Test
    public void testIsVoiceSearchPermittedByPolicy_profileShutdownStarted() {
        doReturn(true).when(mProfile1).shutdownStarted();
        assertTrue(
                VoiceRecognitionUtil.isVoiceSearchPermittedByPolicy(
                        mProfile1, /* strictPolicyCheck= */ false));
        assertFalse(
                VoiceRecognitionUtil.isVoiceSearchPermittedByPolicy(
                        mProfile1, /* strictPolicyCheck= */ true));
    }
}
