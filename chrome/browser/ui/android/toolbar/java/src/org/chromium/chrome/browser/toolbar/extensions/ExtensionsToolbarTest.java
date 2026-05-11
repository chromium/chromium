// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.PositionAssertions.isLeftOf;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isActivated;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.isSelected;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.ui.test.util.ViewUtils.VIEW_GONE;
import static org.chromium.ui.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.ui.test.util.ViewUtils.withEventualExpectedViewState;

import android.app.Instrumentation;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.test.espresso.ViewInteraction;
import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.ImportantFormFactors;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.ui.extensions.ExtensionTestMessageListener;
import org.chromium.chrome.browser.ui.extensions.ExtensionTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.extensions.common.ExtensionFeatures;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.ViewUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** End-to-end test for the extension toolbar on Desktop Android. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, ChromeSwitches.TEST_TYPE})
@DisableFeatures(ExtensionFeatures.EXTENSION_DISABLE_UNSUPPORTED_DEVELOPER)
// @ImportantFormFactors may look redundant, but is actually essential to make this test run on CQ
// because android-desktop CQ builders run chrome_public_test_apk_desktop that only covers tests
// specific to desktop.
@ImportantFormFactors(DeviceFormFactor.DESKTOP)
public class ExtensionsToolbarTest {
    @Rule public TemporaryFolder mTempDir = new TemporaryFolder();

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private EmbeddedTestServer mTestServer;
    private Profile mProfile;
    private WebPageStation mPage;
    private final List<String> mInstalledTestExtensions = new ArrayList<>();

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mProfile = ThreadUtils.runOnUiThreadBlocking(ProfileManager::getLastUsedRegularProfile);
        mTestServer = mActivityTestRule.getTestServer();

        mPage = mActivityTestRule.startOnBlankPage();
        mPage =
                mPage.loadWebPageProgrammatically(
                        mTestServer.getURL("/chrome/test/data/android/google.html"));

        // Set the menu button pinning pref to 'pinned'.
        ThreadUtils.runOnUiThreadBlocking(
                () -> UserPrefs.get(mProfile).setBoolean(Pref.PIN_EXTENSIONS_MENU_BUTTON, true));

        // Wait until the extensions toolbar is loaded.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_button)).check(matches(isDisplayed()));
    }

    @After
    public void tearDown() {
        for (String id : new ArrayList<>(mInstalledTestExtensions)) {
            uninstallTestExtension(id);
        }
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
        ViewUtils.onViewWaiting(withContentDescription("Test Action 1"))
                .check(matches(isDisplayed()));
        ViewUtils.onViewWaiting(withContentDescription("Test Action 2"))
                .check(matches(isDisplayed()));

        // Disable the extension 2.
        ExtensionTestUtils.disableExtension(mProfile, extension2Id);

        // The extension 2 should disappear.
        onView(isRoot())
                .check(
                        withEventualExpectedViewState(
                                withContentDescription("Test Action 2"), VIEW_GONE | VIEW_NULL));
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
        ViewUtils.onViewWaiting(withContentDescription("Alpha Action"))
                .check(matches(isDisplayed()));
        ViewUtils.onViewWaiting(withContentDescription("Beta Action"))
                .check(matches(isDisplayed()));

        // To start, neither extensions should have any render frames (which here equates to no open
        // popups).
        assertEquals(0, ExtensionTestUtils.getRenderFrameHostCount(mProfile, alphaId));
        assertEquals(0, ExtensionTestUtils.getRenderFrameHostCount(mProfile, betaId));

        // Click on Alpha and wait for it to open the popup.
        try (ExtensionTestMessageListener listener =
                new ExtensionTestMessageListener("alpha popup opened")) {
            clickViewWithContentDescription("Alpha Action");
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
            clickViewWithContentDescription("Beta Action");
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
                                withContentDescription(R.string.acc_name_extensions_button)))
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
        ViewUtils.onViewWaiting(withContentDescription("Test Action"))
                .check(matches(isDisplayed()));

        // Click on the extension and wait for it to open the popup.
        try (ExtensionTestMessageListener listener =
                new ExtensionTestMessageListener("popup opened")) {
            clickViewWithContentDescription("Test Action");
            assertTrue(listener.waitUntilSatisfied());
        }

        // Verify that the extension has an active frame (i.e., popup).
        CriteriaHelper.pollInstrumentationThread(
                () -> ExtensionTestUtils.getRenderFrameHostCount(mProfile, id) == 1,
                "Popup did not open");

        // Uninstall the extension.
        uninstallTestExtension(id);

        // The extension should disappear from the toolbar.
        onView(isRoot())
                .check(
                        withEventualExpectedViewState(
                                withContentDescription("Test Action"), VIEW_GONE | VIEW_NULL));

        // The popup should be gone.
        CriteriaHelper.pollInstrumentationThread(
                () -> ExtensionTestUtils.getRenderFrameHostCount(mProfile, id) == 0,
                "Popup should have closed");
    }

    @Test
    @LargeTest
    public void testUninstallPoppedOutExtension() throws IOException {
        String extensionId = loadPopupExtension("extension", "Extension", "Action", "popup opened");

        // Open the extensions menu.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_button))
                .check(matches(isDisplayed()))
                .perform(click());

        try (ExtensionTestMessageListener listener =
                new ExtensionTestMessageListener("popup opened")) {
            // Click on the extension item.
            ViewUtils.onViewWaiting(withText("Extension")).perform(click());
            assertTrue(listener.waitUntilSatisfied());
        }

        // Ensure the popup has opened.
        CriteriaHelper.pollInstrumentationThread(
                () -> ExtensionTestUtils.getRenderFrameHostCount(mProfile, extensionId) == 1,
                "Popup did not open");

        // Uninstall the extension.
        uninstallTestExtension(extensionId);

        // The extension should disappear from the toolbar.
        onView(isRoot())
                .check(
                        withEventualExpectedViewState(
                                withContentDescription("Test Action"), VIEW_GONE | VIEW_NULL));

        // The popup should be gone.
        CriteriaHelper.pollInstrumentationThread(
                () -> ExtensionTestUtils.getRenderFrameHostCount(mProfile, extensionId) == 0,
                "Popup should have closed");
    }

    @Test
    @LargeTest
    public void testPinnedExtensionContextMenuIcon() throws IOException {
        String extensionName = "Test Extension";
        String extensionId = loadBasicExtension("extension", extensionName, "Test Action");

        // Open the extensions menu.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_button)).perform(click());

        // Find the context menu button for the pinned extension.
        ViewInteraction contextMenuButton =
                onView(
                        allOf(
                                withId(R.id.extensions_menu_item_context_menu),
                                hasSibling(hasDescendant(withText(extensionName)))));

        // Verify the button does not have the activate state, which reflects that the action is
        // unpinned.
        contextMenuButton
                .check(matches(isDisplayed()))
                .check(matches(not(isActivated())))
                .check(matches(not(isSelected())));

        // Pin the extension.
        ExtensionTestUtils.setExtensionActionVisible(mProfile, extensionId, true);

        // Verify the button now has activate state.
        contextMenuButton
                .check(matches(isDisplayed()))
                .check(matches(isActivated()))
                .check(matches(not(isSelected())));
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
        return installTestExtension(dir);
    }

    /** Creates and loads an extension with action that can be triggered with command. */
    private String loadCommandExtension(
            String dirName, String name, String actionTitle, String command, String message)
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
                          "permissions": ["test"],
                          "action": { "default_title": "%s", "default_popup": "popup.html" },
                          "commands": {
                            "_execute_action": {
                              "suggested_key": { "default": "%s" },
                              "description": "A test command"
                            }
                          }
                        }
                        """,
                        name, actionTitle, command));
        writeFile(new File(dir, "popup.html"), "<html><script src=\"popup.js\"></script></html>");
        writeFile(
                new File(dir, "popup.js"),
                String.format("chrome.test.sendMessage('%s');", message));
        return installTestExtension(dir);
    }

    /** Creates and loads an extension with a content script that matches the given host. */
    private String loadContentScriptExtension(
            String dirName, String name, String actionTitle, String host) throws IOException {
        File dir = mTempDir.newFolder(dirName);
        writeFile(
                new File(dir, "manifest.json"),
                String.format(
                        "{"
                                + "  \"name\": \"%s\","
                                + "  \"manifest_version\": 3,"
                                + "  \"version\": \"0.1\","
                                + "  \"permissions\": [\"test\"],"
                                + "  \"host_permissions\": [\"%s\"],"
                                + "  \"content_scripts\": [{"
                                + "    \"matches\": [\"%s\"],"
                                + "    \"js\": [\"content.js\"]"
                                + "  }],"
                                + "  \"action\": { \"default_title\": \"%s\" }"
                                + "}",
                        name, host, host, actionTitle));
        writeFile(new File(dir, "content.js"), "console.log('injected');");
        return installTestExtension(dir);
    }

    /** Creates and loads an extension that requests host permissions on a specific site. */
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
        return installTestExtension(dir);
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
        return installTestExtension(dir);
    }

    private String installTestExtension(File dir) {
        String id = ExtensionTestUtils.loadUnpackedExtension(mProfile, dir);
        mInstalledTestExtensions.add(id);
        return id;
    }

    private void uninstallTestExtension(String id) {
        ExtensionTestUtils.uninstallExtension(mProfile, id);
        mInstalledTestExtensions.remove(id);
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

    /**
     * Simulate keypresses. Unlike {@link KeyUtils.dispatchKeyEventToView}, this sends the key event
     * to the focused window.
     *
     * @param code The key code to send.
     * @param metaState The meta state to use alongside.
     */
    private void sendKeyEvent(int code, int metaState) {
        long eventTime = SystemClock.uptimeMillis();
        KeyEvent downEvent =
                new KeyEvent(eventTime, eventTime, KeyEvent.ACTION_DOWN, code, 0, metaState);
        KeyEvent upEvent =
                new KeyEvent(eventTime, eventTime, KeyEvent.ACTION_UP, code, 0, metaState);
        InstrumentationRegistry.getInstrumentation().sendKeySync(downEvent);
        InstrumentationRegistry.getInstrumentation().sendKeySync(upEvent);
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
                                withContentDescription(R.string.acc_name_extensions_button)))
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

    @Test
    @LargeTest
    public void testMenuButtonVisibilityOnPinningPrefChange() throws IOException {
        loadBasicExtension("extension1", "Test Extension", "Test Action");

        // Update the prefs to pinned.
        ThreadUtils.runOnUiThreadBlocking(
                () -> UserPrefs.get(mProfile).setBoolean(Pref.PIN_EXTENSIONS_MENU_BUTTON, true));

        // Ensure the menu button is visible.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_button)).check(matches(isDisplayed()));

        // Update the prefs to unpinned.
        ThreadUtils.runOnUiThreadBlocking(
                () -> UserPrefs.get(mProfile).setBoolean(Pref.PIN_EXTENSIONS_MENU_BUTTON, false));

        // Ensure the menu button is not visible anymore.
        onView(isRoot())
                .check(
                        withEventualExpectedViewState(
                                withId(R.id.extensions_menu_button), VIEW_GONE | VIEW_NULL));
    }

    @Test
    @LargeTest
    public void testMenuButtonToggle() throws IOException {
        loadBasicExtension("extension1", "Test Extension", "Test Action");

        // Update the prefs to pinned.
        ThreadUtils.runOnUiThreadBlocking(
                () -> UserPrefs.get(mProfile).setBoolean(Pref.PIN_EXTENSIONS_MENU_BUTTON, true));

        // Ensure the menu button is visible.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_button)).check(matches(isDisplayed()));

        // Open the extensions menu.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_button)).perform(click());

        // Unpin the menu icon using the toggle.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_button_pinning_toggle))
                .perform(click());

        // TODO(crbug.com/481457578): Check that the menu button is visible even when unpinned if
        // the menu is open.

        // Close the menu.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_close_button)).perform(click());

        // Verify that the extensions menu button is no longer visible on the toolbar.
        onView(isRoot())
                .check(
                        withEventualExpectedViewState(
                                withId(R.id.extensions_menu_button), VIEW_GONE | VIEW_NULL));

        // Open the extensions menu via the app menu.
        ViewUtils.onViewWaiting(withId(R.id.menu_button_wrapper)).perform(click());
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_menu_id)).perform(click());

        // Pin the menu icon using the toggle.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_button_pinning_toggle))
                .perform(click());

        // Close the menu.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_close_button)).perform(click());

        // Verify the icon is still visible after the menu is closed.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_button)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    public void testRequestAccessButtonGrantsSiteAccess() throws IOException {
        String url = mTestServer.getURL("/chrome/test/data/android/simple.html");
        String extensionId =
                loadContentScriptExtension("extension1", "Test Extension", "Test Action", url);

        ExtensionTestUtils.setWithholdHostPermissions(mProfile, extensionId, true);

        // Verify extension has no site access before
        assertFalse(ExtensionTestUtils.hasGrantedHostPermission(mProfile, extensionId, url));

        // Navigate to the site where the extension requests access.
        mPage = mPage.loadWebPageProgrammatically(url);

        // Add an active request for the extension
        WebContents webContents =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mActivityTestRule.getActivity().getActivityTab().getWebContents());
        ExtensionTestUtils.addHostAccessRequest(mProfile, webContents, extensionId);

        // Verify the request access button is visible and shows the count.
        ViewUtils.onViewWaiting(withId(R.id.extensions_request_access_button))
                .check(matches(isDisplayed()))
                .check(matches(withText("Allow 1?")));

        // Click the request access button.
        ViewUtils.onViewWaiting(withId(R.id.extensions_request_access_button)).perform(click());

        // Verify the button text changes to "Allowed" and it becomes disabled.
        CriteriaHelper.pollUiThread(
                () -> {
                    TextView btn =
                            mActivityTestRule
                                    .getActivity()
                                    .findViewById(R.id.extensions_request_access_button);
                    if (btn == null) return false;
                    String expectedText =
                            mActivityTestRule
                                    .getActivity()
                                    .getString(
                                            R.string
                                                    .extensions_request_access_button_dismissed_text);
                    return !btn.isEnabled() && expectedText.equals(btn.getText().toString());
                });

        // Verify site access is granted after
        assertTrue(ExtensionTestUtils.hasGrantedHostPermission(mProfile, extensionId, url));
    }

    @Test
    @LargeTest
    public void testPinAndUnpinExtensionVisibility() throws IOException {
        String extensionId = loadBasicExtension("extension1", "Test Extension", "Test Action");

        // Verify the extension is not visible on the toolbar initially.
        onView(isRoot())
                .check(
                        withEventualExpectedViewState(
                                withContentDescription("Test Action"), VIEW_GONE | VIEW_NULL));

        // Pin the extension programmatically.
        ExtensionTestUtils.setExtensionActionVisible(mProfile, extensionId, true);

        // Verify the action appears on the toolbar.
        ViewUtils.onViewWaiting(withContentDescription("Test Action"))
                .check(matches(isDisplayed()));

        // Unpin the extension programmatically.
        ExtensionTestUtils.setExtensionActionVisible(mProfile, extensionId, false);

        // Verify the action disappears from the toolbar.
        onView(isRoot())
                .check(
                        withEventualExpectedViewState(
                                withContentDescription("Test Action"), VIEW_GONE | VIEW_NULL));
    }

    @Test
    @LargeTest
    public void testPinStateRetainedAfterDisableAndEnable() throws IOException {
        String extensionId = loadBasicExtension("extension1", "Test Extension", "Test Action");

        // Pin the extension programmatically.
        ExtensionTestUtils.setExtensionActionVisible(mProfile, extensionId, true);

        // Verify the action appears on the toolbar.
        ViewUtils.onViewWaiting(withContentDescription("Test Action"))
                .check(matches(isDisplayed()));

        // Disable the extension.
        ExtensionTestUtils.disableExtension(mProfile, extensionId);

        // Verify the action disappears from the toolbar.
        onView(isRoot())
                .check(
                        withEventualExpectedViewState(
                                withContentDescription("Test Action"), VIEW_GONE | VIEW_NULL));

        // Re-enable the extension.
        ExtensionTestUtils.enableExtension(mProfile, extensionId);

        // Verify the action automatically reappears on the toolbar without needing to be re-pinned.
        ViewUtils.onViewWaiting(withContentDescription("Test Action"))
                .check(matches(isDisplayed()));
    }

    /**
     * Util method to simulate drag on an action.
     *
     * @param iconView The View of the action.
     * @param offset How much the action should be dragged horizontally. Positive value indicates
     *     rightward drag.
     * @param longpress Whether the drag should start with a longpress.
     */
    private void performDrag(View iconView, int offset, boolean longpress) {
        int width = iconView.getWidth();
        int height = iconView.getHeight();

        // We need to drag a bit further than our intended final position for the items to swap.
        int dragOverrun = (offset == 0) ? 0 : (width / 3 * (offset < 0 ? -1 : 1));

        int fromX = width / 2;
        int fromY = height / 2;
        int toX = width / 2 + offset + dragOverrun;
        int toY = height / 2;

        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();

        int[] fromLocation = TestTouchUtils.getAbsoluteLocationFromRelative(iconView, fromX, fromY);
        int[] toLocation = TestTouchUtils.getAbsoluteLocationFromRelative(iconView, toX, toY);

        long downTime = TestTouchUtils.dragStart(instrumentation, fromLocation[0], fromLocation[1]);

        if (longpress) {
            SystemClock.sleep((long) (ViewConfiguration.getLongPressTimeout() * 1.5));
        }

        TestTouchUtils.dragTo(
                instrumentation,
                fromLocation[0],
                toLocation[0],
                fromLocation[1],
                toLocation[1],
                /* stepCount= */ 5,
                downTime);

        TestTouchUtils.dragEnd(instrumentation, toLocation[0], toLocation[1], downTime);
    }

    @Test
    @LargeTest
    public void testDragReorderExtensions() throws IOException {
        String alphaId = loadBasicExtension("alpha", "Alpha Extension", "Alpha Action");
        String betaId = loadBasicExtension("beta", "Beta Extension", "Beta Action");
        String gammaId = loadBasicExtension("gamma", "Gamma Extension", "Gamma Action");

        // Pin all extensions.
        ExtensionTestUtils.setExtensionActionVisible(mProfile, alphaId, true);
        ExtensionTestUtils.setExtensionActionVisible(mProfile, betaId, true);
        ExtensionTestUtils.setExtensionActionVisible(mProfile, gammaId, true);
        ViewUtils.onViewWaiting(withContentDescription("Alpha Action"))
                .check(matches(isDisplayed()));
        ViewUtils.onViewWaiting(withContentDescription("Beta Action"))
                .check(matches(isDisplayed()));
        ViewUtils.onViewWaiting(withContentDescription("Gamma Action"))
                .check(matches(isDisplayed()));

        View root =
                mActivityTestRule
                        .getActivity()
                        .getWindow()
                        .getDecorView()
                        .findViewById(android.R.id.content);

        View alphaIcon =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> findViewWithContentDescription(root, "Alpha Action"));
        View betaIcon =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> findViewWithContentDescription(root, "Beta Action"));
        View gammaIcon =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> findViewWithContentDescription(root, "Gamma Action"));

        // The order should be [A] [B] [G].
        onView(withContentDescription("Alpha Action"))
                .check(isLeftOf(withContentDescription("Beta Action")));
        onView(withContentDescription("Beta Action"))
                .check(isLeftOf(withContentDescription("Gamma Action")));
        assertArrayEquals(
                "The data order should match Views.",
                new String[] {alphaId, betaId, gammaId},
                ExtensionTestUtils.getPinnedActionIds(mProfile));

        // Drag [A] to the position of [B].
        performDrag(alphaIcon, (int) (betaIcon.getX() - alphaIcon.getX()), /* longpress= */ true);

        // The order now should be [B] [A] [G].
        onView(withContentDescription("Beta Action"))
                .check(isLeftOf(withContentDescription("Alpha Action")));
        onView(withContentDescription("Alpha Action"))
                .check(isLeftOf(withContentDescription("Gamma Action")));
        assertArrayEquals(
                "The data order should match Views.",
                new String[] {betaId, alphaId, gammaId},
                ExtensionTestUtils.getPinnedActionIds(mProfile));

        // Drag [G] to the position of [B].
        performDrag(gammaIcon, (int) (betaIcon.getX() - gammaIcon.getX()), /* longpress= */ true);

        // The order now should be [G] [B] [A].
        onView(withContentDescription("Gamma Action"))
                .check(isLeftOf(withContentDescription("Beta Action")));
        onView(withContentDescription("Beta Action"))
                .check(isLeftOf(withContentDescription("Alpha Action")));
        assertArrayEquals(
                "The data order should match Views.",
                new String[] {gammaId, betaId, alphaId},
                ExtensionTestUtils.getPinnedActionIds(mProfile));
    }

    @Test
    @LargeTest
    public void testInstantDragWithTouchDoesNotReorder() throws IOException {
        String alphaId = loadBasicExtension("alpha", "Alpha Extension", "Alpha Action");
        String betaId = loadBasicExtension("beta", "Beta Extension", "Beta Action");

        // Pin both extensions.
        ExtensionTestUtils.setExtensionActionVisible(mProfile, alphaId, true);
        ExtensionTestUtils.setExtensionActionVisible(mProfile, betaId, true);
        ViewUtils.onViewWaiting(withContentDescription("Alpha Action"))
                .check(matches(isDisplayed()));
        ViewUtils.onViewWaiting(withContentDescription("Beta Action"))
                .check(matches(isDisplayed()));

        View root =
                mActivityTestRule
                        .getActivity()
                        .getWindow()
                        .getDecorView()
                        .findViewById(android.R.id.content);

        View alphaIcon =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> findViewWithContentDescription(root, "Alpha Action"));
        View betaIcon =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> findViewWithContentDescription(root, "Beta Action"));

        // The order should be [A] [B].
        onView(withContentDescription("Alpha Action"))
                .check(isLeftOf(withContentDescription("Beta Action")));
        assertArrayEquals(
                "The data order should match Views.",
                new String[] {alphaId, betaId},
                ExtensionTestUtils.getPinnedActionIds(mProfile));

        // Try to drag [A] to the position of [B] but without longpress.
        performDrag(alphaIcon, (int) (betaIcon.getX() - alphaIcon.getX()), /* longpress= */ false);

        // Confirm that the order has not changed.
        onView(withContentDescription("Alpha Action"))
                .check(isLeftOf(withContentDescription("Beta Action")));
        assertArrayEquals(
                "The data order should match Views.",
                new String[] {alphaId, betaId},
                ExtensionTestUtils.getPinnedActionIds(mProfile));
    }

    @Test
    @LargeTest
    public void testManifestSpecifiedExtensionCommand() throws IOException {
        String extensionId =
                loadCommandExtension(
                        "extension", "Extension", "Action", "Ctrl+Shift+1", "popup opened");
        ExtensionTestUtils.setExtensionActionVisible(mProfile, extensionId, true);

        // Listen for the background script to echo the command back.
        try (ExtensionTestMessageListener listener =
                new ExtensionTestMessageListener("popup opened")) {
            // Send key events to trigger extension action.
            sendKeyEvent(KeyEvent.KEYCODE_1, KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON);
            assertTrue(listener.waitUntilSatisfied());
        }

        // Ensure that the popup has opened.
        CriteriaHelper.pollInstrumentationThread(
                () -> ExtensionTestUtils.getRenderFrameHostCount(mProfile, extensionId) == 1,
                "Popup did not open.");
    }

    @Test
    @LargeTest
    public void testExtensionCommandClosesPopupIfOpen() throws IOException {
        String extensionId =
                loadCommandExtension(
                        "extension", "Extension", "Action", "Ctrl+Shift+1", "popup opened");
        ExtensionTestUtils.setExtensionActionVisible(mProfile, extensionId, true);

        // Listen for the background script to echo the command back.
        try (ExtensionTestMessageListener listener =
                new ExtensionTestMessageListener("popup opened")) {
            // Send key events to trigger extension action.
            sendKeyEvent(KeyEvent.KEYCODE_1, KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON);
            assertTrue(listener.waitUntilSatisfied());
        }

        // Ensure the popup is open.
        CriteriaHelper.pollInstrumentationThread(
                () -> ExtensionTestUtils.getRenderFrameHostCount(mProfile, extensionId) == 1,
                "Popup did not open");

        // Send the same keypresses again.
        sendKeyEvent(KeyEvent.KEYCODE_1, KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON);

        // Confirm that the popup has closed.
        CriteriaHelper.pollInstrumentationThread(
                () -> ExtensionTestUtils.getRenderFrameHostCount(mProfile, extensionId) == 0,
                "Popup did not close.");
    }

    @Test
    @LargeTest
    public void testNonExtensionCommandIsSentToApplicationWindow() throws IOException {
        String extensionId = loadPopupExtension("extension", "Extension", "Action", "popup opened");
        ExtensionTestUtils.setExtensionActionVisible(mProfile, extensionId, true);
        ViewUtils.onViewWaiting(withContentDescription("Action")).check(matches(isDisplayed()));

        // Listen for the background script to echo the command back.
        try (ExtensionTestMessageListener listener =
                new ExtensionTestMessageListener("popup opened")) {
            // Click on the icon to show the popup.
            clickViewWithContentDescription("Action");
            assertTrue(listener.waitUntilSatisfied());
        }

        // Ensure the popup is open before sending the key event.
        CriteriaHelper.pollInstrumentationThread(
                () -> ExtensionTestUtils.getRenderFrameHostCount(mProfile, extensionId) == 1,
                "Popup did not open");

        // Send Ctrl+T.
        sendKeyEvent(KeyEvent.KEYCODE_T, KeyEvent.META_CTRL_ON);

        // The Ctrl+T command should be routed to the main window, causing a new tab to be created
        // and the popup to be dismissed.
        CriteriaHelper.pollInstrumentationThread(
                () -> ExtensionTestUtils.getRenderFrameHostCount(mProfile, extensionId) == 0,
                "Popup should have closed after Ctrl+T was pressed and tab model has changed.");
    }

    @Test
    @LargeTest
    public void testTriggerUnpinnedActionFromExtensionsMenu() throws IOException {
        String extensionId = loadPopupExtension("extension", "Extension", "Action", "popup opened");

        // Open the extensions menu.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_button))
                .check(matches(isDisplayed()))
                .perform(click());

        try (ExtensionTestMessageListener listener =
                new ExtensionTestMessageListener("popup opened")) {
            // Click on the extension item.
            ViewUtils.onViewWaiting(withText("Extension")).perform(click());
            assertTrue(listener.waitUntilSatisfied());
        }

        // Ensure the popup has opened.
        CriteriaHelper.pollInstrumentationThread(
                () -> ExtensionTestUtils.getRenderFrameHostCount(mProfile, extensionId) == 1,
                "Popup did not open");
    }

    @Test
    @LargeTest
    public void testTriggerPinnedActionFromExtensionsMenu() throws IOException {
        String extensionId = loadPopupExtension("extension", "Extension", "Action", "popup opened");
        ExtensionTestUtils.setExtensionActionVisible(mProfile, extensionId, true);
        ViewUtils.onViewWaiting(withContentDescription("Action")).check(matches(isDisplayed()));

        // Open the extensions menu.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_button))
                .check(matches(isDisplayed()))
                .perform(click());

        try (ExtensionTestMessageListener listener =
                new ExtensionTestMessageListener("popup opened")) {
            // Click on the extension item.
            ViewUtils.onViewWaiting(withText("Extension")).perform(click());
            assertTrue(listener.waitUntilSatisfied());
        }

        // Ensure the popup has opened.
        CriteriaHelper.pollInstrumentationThread(
                () -> ExtensionTestUtils.getRenderFrameHostCount(mProfile, extensionId) == 1,
                "Popup did not open");
    }

    @Test
    @LargeTest
    // TODO(crbug.com/511895956): Re-enable this test.
    @DisableIf.Device(DeviceFormFactor.DESKTOP)
    public void testNewDialogDismissesExtensionsMenu() throws IOException {
        loadBasicExtension("extension1", "Test Extension", "Test Action");

        // Open the extensions menu.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_button))
                .check(matches(isDisplayed()))
                .perform(click());

        // Show "Remove extension" dialog.
        ViewUtils.onViewWaiting(withId(R.id.extensions_menu_item_context_menu)).perform(click());
        ViewUtils.onViewWaiting(withText("Remove from Chromium")).perform(click());

        // The dialog should trigger dismiss on the extensions menu.
        onView(isRoot())
                .check(
                        withEventualExpectedViewState(
                                withId(R.id.extensions_menu_close_button), VIEW_GONE | VIEW_NULL));
    }

    @Test
    @LargeTest
    public void testNewDialogDismissesExtensionPopup() throws IOException, TimeoutException {
        String alphaId =
                loadPopupExtension(
                        "alpha", "Alpha Extension", "Alpha Action", "alpha popup opened");
        ExtensionTestUtils.setExtensionActionVisible(mProfile, alphaId, true);
        ViewUtils.onViewWaiting(withContentDescription("Alpha Action"))
                .check(matches(isDisplayed()));

        // Click on Alpha and wait for it to open the popup.
        try (ExtensionTestMessageListener listener =
                new ExtensionTestMessageListener("alpha popup opened")) {
            clickViewWithContentDescription("Alpha Action");
            assertTrue(listener.waitUntilSatisfied());
        }
        CriteriaHelper.pollInstrumentationThread(
                () -> ExtensionTestUtils.getRenderFrameHostCount(mProfile, alphaId) == 1,
                "Alpha popup did not open.");

        // Display alert with JavaScript asynchronously.
        WebContents webContents =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mActivityTestRule.getActivity().getActivityTab().getWebContents());
        ThreadUtils.runOnUiThread(
                () -> {
                    webContents.evaluateJavaScriptForTests("alert('JavaScript alert');", null);
                });

        // Wait for the dialog to appear and click OK.
        ViewUtils.onViewWaiting(withText("OK")).perform(click());

        // Verify the extension popup was dismissed by the alert dialog.
        CriteriaHelper.pollInstrumentationThread(
                () -> ExtensionTestUtils.getRenderFrameHostCount(mProfile, alphaId) == 0,
                "Alpha popup did not close.");
    }
}
