// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.view.View;

import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.RenderTestRule;

/**
 * A TestRule for creating Render Tests for Chrome.
 *
 * <pre>
 * {@code
 *
 * @RunWith(ChromeJUnit4ClassRunner.class)
 * @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
 * public class MyTest {
 *     // Provide RenderTestRule with the path from src/ to the golden directory.
 *     @Rule
 *     public ChromeRenderTestRule mRenderTestRule = new ChromeRenderTestRule.Builder()
 *             // Required. If using ANDROID_RENDER_TESTS_PUBLIC, the Builder can be created with
 *             // the shorthand ChromeRenderTestRule.Builder.withPublicCorpus().
 *             .setCorpus(ChromeRenderTestRule.Corpus.ANDROID_RENDER_TESTS_PUBLIC)
 *             // Optional, only necessary once a CL lands that should invalidate previous golden
 *             // images, e.g. a UI rework.
 *             .setRevision(2)
 *             // Optional, only necessary if you want a message to be associated with these
 *             // golden images and shown in the Gold web UI, e.g. the reason why the revision was
 *             // incremented.
 *             .setDescription("Material design rework")
 *             .build();
 *
 *     @Test
 *     // The test must have the feature "RenderTest" for the bots to display renders.
 *     @Feature({"RenderTest"})
 *     public void testViewAppearance() {
 *         // Setup the UI.
 *         ...
 *
 *         // Render UI Elements.
 *         mRenderTestRule.render(bigWidgetView, "big_widget");
 *         mRenderTestRule.render(smallWidgetView, "small_widget");
 *     }
 * }
 *
 * }
 * </pre>
 */
public class ChromeRenderTestRule extends RenderTestRule {
    protected ChromeRenderTestRule(int revision, @RenderTestRule.Corpus String corpus,
            String description, boolean failOnUnsupportedConfigs) {
        super(revision, corpus, description, failOnUnsupportedConfigs);
    }

    /**
     * Searches the View hierarchy and modifies the Views to provide better stability in tests. For
     * example it will disable the blinking cursor in EditTexts.
     */
    public static void sanitize(View view) {
        TestThreadUtils.runOnUiThreadBlocking(() -> RenderTestRule.sanitize(view));
    }

    /**
     * Builder to create a ChromeRenderTestRule.
     */
    public static class Builder extends RenderTestRule.BaseBuilder<Builder> {
        @Override
        public ChromeRenderTestRule build() {
            return new ChromeRenderTestRule(
                    mRevision, mCorpus, mDescription, mFailOnUnsupportedConfigs);
        }

        /**
         * Creates a Builder with the default public corpus.
         */
        public static Builder withPublicCorpus() {
            return new Builder().setCorpus(Corpus.ANDROID_RENDER_TESTS_PUBLIC);
        }
    }
}
