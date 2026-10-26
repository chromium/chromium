// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.ComponentName;
import android.content.IntentFilter;
import android.speech.RecognizerIntent;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit Test for {@link VoiceRecognitionUtil}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VoiceRecognitionUtilUnitTest {
    private static final String RECOGNITION_PACKAGE_NAME = "com.some.package";
    private static final ComponentName RECOGNITION_ACTIVITY =
            new ComponentName(RECOGNITION_PACKAGE_NAME, "com.some.package.RecognitionActivity");
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
}
