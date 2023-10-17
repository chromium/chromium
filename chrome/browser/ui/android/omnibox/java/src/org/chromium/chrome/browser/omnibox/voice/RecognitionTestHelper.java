// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import android.os.Bundle;
import android.speech.RecognizerIntent;

import org.junit.Assert;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler.VoiceResult;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * A helper class that simplifies creation of {@link VoiceRecognitionHandler}'s test dependencies
 * and provides utility methods for tests.
 */
public class RecognitionTestHelper {
    @Implements(UserPrefs.class)
    public static class ShadowUserPrefs {
        private static PrefService sPrefs;

        static void setPrefService(PrefService prefs) {
            sPrefs = prefs;
        }

        @Implementation
        public static PrefService get(BrowserContextHandle h) {
            return sPrefs;
        }
    }

    /**
     * Creates a test bundle.
     *
     * @param texts the queries representing transcription results
     * @param confidences confidence values for corresponding queries
     */
    public static Bundle createPlaceholderBundle(String[] texts, float[] confidences) {
        Bundle b = new Bundle();

        b.putStringArrayList(
                RecognizerIntent.EXTRA_RESULTS, new ArrayList<String>(Arrays.asList(texts)));
        b.putFloatArray(RecognizerIntent.EXTRA_CONFIDENCE_SCORES, confidences);
        return b;
    }

    public static void assertVoiceResultsAreEqual(
            List<VoiceResult> results, String[] texts, float[] confidences) {
        Assert.assertTrue(
                "Invalid array sizes",
                results.size() == texts.length && texts.length == confidences.length);
        for (int i = 0; i < texts.length; ++i) {
            VoiceResult result = results.get(i);
            Assert.assertEquals("Match text is not equal", texts[i], result.getMatch());
            Assert.assertEquals(
                    "Confidence is not equal", confidences[i], result.getConfidence(), 0);
        }
    }
}
