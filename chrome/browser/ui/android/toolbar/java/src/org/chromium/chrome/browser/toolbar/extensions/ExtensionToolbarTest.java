// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.ui.test.util.ViewUtils.VIEW_GONE;
import static org.chromium.ui.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.ui.test.util.ViewUtils.withEventualExpectedViewState;

import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.ui.extensions.ExtensionTestMessageListener;
import org.chromium.chrome.browser.ui.extensions.ExtensionTestUtils;
import org.chromium.chrome.browser.ui.extensions.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.extensions.common.ExtensionFeatures;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.ViewUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

/** End-to-end test for the extension toolbar on Desktop Android. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, ChromeSwitches.TEST_TYPE})
@DisableFeatures(ExtensionFeatures.EXTENSION_DISABLE_UNSUPPORTED_DEVELOPER)
public class ExtensionToolbarTest {
    @Rule public TemporaryFolder mTempDir = new TemporaryFolder();

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private EmbeddedTestServer mTestServer;
    private Profile mProfile;
    private WebPageStation mPage;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mProfile = ThreadUtils.runOnUiThreadBlocking(ProfileManager::getLastUsedRegularProfile);
        mTestServer = mActivityTestRule.getTestServer();

        mPage = mActivityTestRule.startOnBlankPage();
        mPage =
                mPage.loadWebPageProgrammatically(
                        mTestServer.getURL("/chrome/test/data/android/google.html"));

        // Wait until the extensions toolbar is loaded.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_button)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    public void testExtensionsMenu() throws IOException {
        loadBasicExtension("extension1", "Test Extension", "Test Action");

        // Open the extensions menu.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_button))
                .check(matches(isDisplayed()))
                .perform(click());

        // Verify that the test extension is in the menu.
        ViewUtils.onViewWaiting(withText("Test Extension")).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    public void testPinnedActions() throws IOException {
        String extension1Id = loadBasicExtension("extension1", "Test Extension 1", "Test Action 1");
        String extension2Id = loadBasicExtension("extension2", "Test Extension 2", "Test Action 2");

        // Pin both extensions to the toolbar.
        ExtensionTestUtils.setExtensionActionVisible(mProfile, extension1Id, true);
        ExtensionTestUtils.setExtensionActionVisible(mProfile, extension2Id, true);

        // Ensure visibility of the toolbar actions.
        // TODO(crbug.com/435305159): The content description should be the action name, not the
        // extension name.
        ViewUtils.onViewWaiting(withContentDescription("Test Extension 1"))
                .check(matches(isDisplayed()));
        ViewUtils.onViewWaiting(withContentDescription("Test Extension 2"))
                .check(matches(isDisplayed()));

        // Disable the extension 2.
        ExtensionTestUtils.disableExtension(mProfile, extension2Id);

        // The extension 2 should disappear.
        // TODO(crbug.com/435305159): The content description should be the action name, not the
        // extension name.
        onView(isRoot())
                .check(
                        withEventualExpectedViewState(
                                withContentDescription("Test Extension 2"), VIEW_GONE | VIEW_NULL));
    }

    /**
     * Tests that clicking on a second extension action will close a first if its popup was open.
     *
     * <p>This test corresponds to ClickingOnASecondActionClosesTheFirst test in
     * chrome/browser/ui/views/extensions/extensions_toolbar_desktop_interactive_uitest.cc.
     */
    @Test
    @LargeTest
    public void testClickingOnASecondActionClosesTheFirst() throws IOException {
        String alphaId =
                loadPopupExtension(
                        "alpha", "Alpha Extension", "Alpha Action", "alpha popup opened");
        String betaId =
                loadPopupExtension("beta", "Beta Extension", "Beta Action", "beta popup opened");

        // Pin both extensions.
        ExtensionTestUtils.setExtensionActionVisible(mProfile, alphaId, true);
        ExtensionTestUtils.setExtensionActionVisible(mProfile, betaId, true);
        ViewUtils.onViewWaiting(withContentDescription("Alpha Extension"))
                .check(matches(isDisplayed()));
        ViewUtils.onViewWaiting(withContentDescription("Beta Extension"))
                .check(matches(isDisplayed()));

        // To start, neither extensions should have any render frames (which here equates to no open
        // popups).
        assertEquals(0, ExtensionTestUtils.getRenderFrameHostCount(mProfile, alphaId));
        assertEquals(0, ExtensionTestUtils.getRenderFrameHostCount(mProfile, betaId));

        // Click on Alpha and wait for it to open the popup.
        try (ExtensionTestMessageListener listener =
                new ExtensionTestMessageListener("alpha popup opened")) {
            clickViewWithContentDescription("Alpha Extension");
            assertTrue(listener.waitUntilSatisfied());
        }
        // Verify that Alpha (and only Alpha) has an active frame (i.e., popup).
        CriteriaHelper.pollInstrumentationThread(
                () -> ExtensionTestUtils.getRenderFrameHostCount(mProfile, alphaId) == 1,
                "Alpha popup did not open");
        CriteriaHelper.pollInstrumentationThread(
                () -> ExtensionTestUtils.getRenderFrameHostCount(mProfile, betaId) == 0,
                "Beta popup should not be open");

        // Click on Beta. This should result in Beta's popup opening and Alpha's closing.
        try (ExtensionTestMessageListener listener =
                new ExtensionTestMessageListener("beta popup opened")) {
            clickViewWithContentDescription("Beta Extension");
            assertTrue(listener.waitUntilSatisfied());
        }
        // Beta (and only Beta) should have an active popup.
        CriteriaHelper.pollInstrumentationThread(
                () -> ExtensionTestUtils.getRenderFrameHostCount(mProfile, betaId) == 1,
                "Beta popup did not open");
        CriteriaHelper.pollInstrumentationThread(
                () -> ExtensionTestUtils.getRenderFrameHostCount(mProfile, alphaId) == 0,
                "Alpha popup should have closed");
    }

    @Test
    @LargeTest
    public void testExtensionsMenuButtonState() throws IOException {
        loadBasicExtension("extension1", "Test Extension", "Test Action");

        // Check the default state of the button.
        ViewUtils.onViewWaiting(
                        allOf(
                                withId(R.id.extensions_menu_button),
                                withContentDescription(R.string.accessibility_btn_extensions)))
                .check(matches(isDisplayed()));

        // Open the extensions menu.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_button))
                .check(matches(isDisplayed()))
                .perform(click());

        // Verify that the test extension is in the menu.
        ViewUtils.onViewWaiting(withText("Test Extension")).check(matches(isDisplayed()));

        // Block all extensions.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_site_settings_toggle)).perform(click());

        // Close the menu so Espresso can find views in the activity window again.
        androidx.test.espresso.Espresso.pressBack();

        // Check the button state after blocking all extensions.
        ViewUtils.onViewWaiting(
                        allOf(
                                withId(R.id.extensions_menu_button),
                                withContentDescription(
                                        R.string
                                                .acc_name_extensions_button_all_extensions_blocked)))
                .check(matches(isDisplayed()));
    }

    /**
     * Tests that uninstalling an extension while its popup is open works correctly and closes the
     * popup.
     *
     * <p>This test corresponds to UninstallExtensionWithActivelyShownWidget test in
     * chrome/browser/ui/views/extensions/extensions_toolbar_desktop_interactive_uitest.cc.
     */
    @Test
    @LargeTest
    public void testUninstallExtensionWithActivelyShownWidget() throws IOException {
        String id =
                loadPopupExtension("extension", "Test Extension", "Test Action", "popup opened");

        // Pin the extension.
        ExtensionTestUtils.setExtensionActionVisible(mProfile, id, true);
        ViewUtils.onViewWaiting(withContentDescription("Test Extension"))
                .check(matches(isDisplayed()));

        // Click on the extension and wait for it to open the popup.
        try (ExtensionTestMessageListener listener =
                new ExtensionTestMessageListener("popup opened")) {
            clickViewWithContentDescription("Test Extension");
            assertTrue(listener.waitUntilSatisfied());
        }

        // Verify that the extension has an active frame (i.e., popup).
        CriteriaHelper.pollInstrumentationThread(
                () -> ExtensionTestUtils.getRenderFrameHostCount(mProfile, id) == 1,
                "Popup did not open");

        // Uninstall the extension.
        ExtensionTestUtils.uninstallExtension(mProfile, id);

        // The extension should disappear from the toolbar.
        onView(isRoot())
                .check(
                        withEventualExpectedViewState(
                                withContentDescription("Test Extension"), VIEW_GONE | VIEW_NULL));

        // The popup should be gone.
        CriteriaHelper.pollInstrumentationThread(
                () -> ExtensionTestUtils.getRenderFrameHostCount(mProfile, id) == 0,
                "Popup should have closed");
    }

    private String loadBasicExtension(String dirName, String name, String actionTitle)
            throws IOException {
        File dir = mTempDir.newFolder(dirName);
        writeFile(
                new File(dir, "manifest.json"),
                String.format(
                        """
                        {
                          "name": "%s",
                          "manifest_version": 3,
                          "version": "0.1",
                          "action": { "default_title": "%s" }
                        }
                        """,
                        name, actionTitle));
        return ExtensionTestUtils.loadUnpackedExtension(mProfile, dir);
    }

    /** Loads an extension that requests host permissions on a specific site. */
    private String loadHostPermissionsExtension(
            String dirName, String name, String actionTitle, String host) throws IOException {
        File dir = mTempDir.newFolder(dirName);
        writeFile(
                new File(dir, "manifest.json"),
                String.format(
                        """
                        {
                          "name": "%s",
                          "manifest_version": 3,
                          "version": "0.1",
                          "permissions": ["test"],
                          "host_permissions": ["%s"]
                        }
                        """,
                        name, host));
        return ExtensionTestUtils.loadUnpackedExtension(mProfile, dir);
    }

    private String loadPopupExtension(
            String dirName, String name, String actionTitle, String message) throws IOException {
        File dir = mTempDir.newFolder(dirName);
        writeFile(
                new File(dir, "manifest.json"),
                String.format(
                        """
                        {
                          "name": "%s",
                          "manifest_version": 3,
                          "version": "0.1",
                          "permissions": ["test"],
                          "action": { "default_title": "%s", "default_popup": "popup.html" }
                        }
                        """,
                        name, actionTitle));
        writeFile(new File(dir, "popup.html"), "<html><script src=\"popup.js\"></script></html>");
        writeFile(
                new File(dir, "popup.js"),
                String.format("chrome.test.sendMessage('%s');", message));
        return ExtensionTestUtils.loadUnpackedExtension(mProfile, dir);
    }

    private void writeFile(File file, String contents) throws IOException {
        try (FileOutputStream outputStream = new FileOutputStream(file)) {
            outputStream.write(contents.getBytes(StandardCharsets.UTF_8));
        }
    }

    /**
     * Finds a view with the given content description in the current activity's view hierarchy and
     * performs a click on it directly on the UI thread.
     *
     * <p>This method is used to bypass Espresso's restrictions, specifically the {@code
     * RootViewWithoutFocusException} which occurs when a focusable popup (like an extension popup)
     * is open. In such cases, Espresso refuses to interact with the underlying activity window,
     * necessitating this manual view traversal and click execution.
     */
    private void clickViewWithContentDescription(String contentDescription) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View root =
                            mActivityTestRule
                                    .getActivity()
                                    .getWindow()
                                    .getDecorView()
                                    .findViewById(android.R.id.content);
                    View target = findViewWithContentDescription(root, contentDescription);
                    if (target == null) {
                        throw new RuntimeException(
                                "View with content description '"
                                        + contentDescription
                                        + "' not found");
                    }
                    target.performClick();
                });
    }

    private View findViewWithContentDescription(View view, String contentDescription) {
        ThreadUtils.assertOnUiThread();
        if (contentDescription.equals(view.getContentDescription())) {
            return view;
        }
        if (view instanceof ViewGroup group) {
            for (int i = 0; i < group.getChildCount(); i++) {
                View child =
                        findViewWithContentDescription(group.getChildAt(i), contentDescription);
                if (child != null) {
                    return child;
                }
            }
        }
        return null;
    }

    @Test
    @LargeTest
    public void testExtensionsMenuButtonStateOnTabChange() throws IOException {
        String url1 = mTestServer.getURL("/chrome/test/data/android/simple.html");
        loadHostPermissionsExtension(
                "extension1", "Test Extension", "Test Action", url1.toString());
        String url2 = "about:blank";

        mPage = mPage.loadWebPageProgrammatically(url1);

        // Check the default state of the button (should be allowed since extension requested access
        // on url1).
        ViewUtils.onViewWaiting(
                        allOf(
                                withId(R.id.extensions_menu_button),
                                withContentDescription(
                                        R.string
                                                .acc_name_extensions_button_any_extension_has_access)))
                .check(matches(isDisplayed()));

        // Open the extensions menu and block all extensions on site 1.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_button)).perform(click());
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_site_settings_toggle)).perform(click());

        // Close the menu so Espresso can find views in the activity window again.
        androidx.test.espresso.Espresso.pressBack();

        // Check the button state after blocking all extensions.
        ViewUtils.onViewWaiting(
                        allOf(
                                withId(R.id.extensions_menu_button),
                                withContentDescription(
                                        R.string
                                                .acc_name_extensions_button_all_extensions_blocked)))
                .check(matches(isDisplayed()));

        // Navigate to site 2 (where extensions are not blocked).
        mPage = mPage.loadWebPageProgrammatically(url2);

        // Check that button state returns to default for the new site since extension doesn't
        // request access on url2.
        ViewUtils.onViewWaiting(
                        allOf(
                                withId(R.id.extensions_menu_button),
                                withContentDescription(R.string.accessibility_btn_extensions)))
                .check(matches(isDisplayed()));

        // Navigate back to site 1 (where extensions are blocked).
        mPage = mPage.loadWebPageProgrammatically(url1);

        // Check the button state is blocked again.
        ViewUtils.onViewWaiting(
                        allOf(
                                withId(R.id.extensions_menu_button),
                                withContentDescription(
                                        R.string
                                                .acc_name_extensions_button_all_extensions_blocked)))
                .check(matches(isDisplayed()));
    }
}
