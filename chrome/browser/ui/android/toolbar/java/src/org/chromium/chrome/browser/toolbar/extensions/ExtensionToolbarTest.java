// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.ui.extensions.ExtensionTestUtils;
import org.chromium.chrome.browser.ui.extensions.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.extensions.common.ExtensionFeatures;
import org.chromium.ui.test.util.ViewUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

/** End-to-end test for the extension toolbar on Desktop Android. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures(ExtensionFeatures.EXTENSION_DISABLE_UNSUPPORTED_DEVELOPER)
public class ExtensionToolbarTest {
    @Rule public TemporaryFolder mTempDir = new TemporaryFolder();

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private Profile mProfile;
    private WebPageStation mPage;

    @Before
    public void setUp() throws IOException {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mProfile = ThreadUtils.runOnUiThreadBlocking(ProfileManager::getLastUsedRegularProfile);

        mPage = mActivityTestRule.startOnBlankPage();

        // Install a test extension.
        writeFile(
                mTempDir.newFile("manifest.json"),
                """
                    {
                      "name": "Test Extension",
                      "manifest_version": 3,
                      "version": "0.1",
                      "action": {
                        "default_title": "Test Action"
                      }
                    }
                """);
        ExtensionTestUtils.loadUnpackedExtension(mProfile, mTempDir.getRoot());
    }

    @Test
    @LargeTest
    public void testExtensionsMenu() {
        // Open the extensions menu.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_button))
                .check(matches(isDisplayed()))
                .perform(click());

        // Verify that the test extension is in the menu.
        // TODO(crbug.com/435305159): The label should be the extension name, not the action name.
        ViewUtils.onViewWaiting(withText("Test Action")).check(matches(isDisplayed()));
    }

    private void writeFile(File file, String contents) throws IOException {
        try (FileOutputStream outputStream = new FileOutputStream(file)) {
            outputStream.write(contents.getBytes(StandardCharsets.UTF_8));
        }
    }
}
