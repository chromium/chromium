// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.ViewUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

/** End-to-end test for the Extension Toolbar on Android. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Starts a fresh browser with specific command line flags")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ExtensionsToolbarTest {
    @Rule public TemporaryFolder mTemporaryFolder = new TemporaryFolder();

    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private File mExtensionDir;

    @Before
    public void setUp() throws IOException {
        // Create a simple extension in the temporary folder.
        // We do this before the activity starts so we can pass the path in the command line.
        mExtensionDir = mTemporaryFolder.newFolder("test_extension");

        String manifestContent =
                "{"
                        + "  \"name\": \"Test Extension\","
                        + "  \"version\": \"1.0\","
                        + "  \"manifest_version\": 3,"
                        + "  \"action\": {"
                        + "    \"default_title\": \"Test Extension Action\""
                        + "  }"
                        + "}";
        File manifestFile = new File(mExtensionDir, "manifest.json");
        try (FileOutputStream fos = new FileOutputStream(manifestFile)) {
            fos.write(manifestContent.getBytes(StandardCharsets.UTF_8));
        }

        CommandLine.getInstance()
                .appendSwitchWithValue("load-extension", mExtensionDir.getAbsolutePath());
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testExtensionIconAppears() {
        mActivityTestRule.startMainActivityOnBlankPage();

        // Open the extensions menu.
        onView(withId(R.id.extensions_menu_button)).check(matches(isDisplayed())).perform(click());

        // Verify that the test extension is in the menu.
        // TODO(crbug.com/435305159): The label should be the extension name, not the action name.
        ViewUtils.onViewWaiting(withText("Test Extension Action")).check(matches(isDisplayed()));
    }
}
