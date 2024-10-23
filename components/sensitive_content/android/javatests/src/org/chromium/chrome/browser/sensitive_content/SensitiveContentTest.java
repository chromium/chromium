// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sensitive_content;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;

import android.os.Build;
import android.view.View;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.BlankCTATabInitialStatePublicTransitRule;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.sensitive_content.SensitiveContentFeatures;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.List;

/** Tests that the content sensitivity of is set properly. The test fixture uses a tab. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures(SensitiveContentFeatures.SENSITIVE_CONTENT)
@MinAndroidSdkLevel(Build.VERSION_CODES.VANILLA_ICE_CREAM)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SensitiveContentTest {
    private static class TestSensitiveContentClientObserver
            implements SensitiveContentClient.Observer {
        private boolean mContentIsSensitive;

        @Override
        public void onContentSensitivityChanged(boolean contentIsSensitive) {
            mContentIsSensitive = contentIsSensitive;
        }

        public boolean getContentSensitivity() {
            return mContentIsSensitive;
        }
    }

    public static final String SENSITIVE_FILE =
            "/chrome/test/data/autofill/autofill_creditcard_form_with_autocomplete_attributes.html";
    public static final String NOT_SENSITIVE_FILE =
            "/chrome/test/data/autofill/autocomplete_simple_form.html";

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStatePublicTransitRule mInitialStateRule =
            new BlankCTATabInitialStatePublicTransitRule(sActivityTestRule);

    private WebPageStation mPage;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mPage = mInitialStateRule.startOnBlankPage();
        mTestServer = sActivityTestRule.getTestServer();
    }

    @Test
    @MediumTest
    public void testTabHasSensitiveContentWhileSensitiveFieldsArePresent() throws Exception {
        assertEquals(
                "Initially, the tab does not have sensitive content",
                getContentViewOfCurrentTab().getContentSensitivity(),
                View.CONTENT_SENSITIVITY_AUTO);

        sActivityTestRule.loadUrl(mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(
                () ->
                        getContentViewOfCurrentTab().getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_SENSITIVE);

        sActivityTestRule.loadUrl(mTestServer.getURL(NOT_SENSITIVE_FILE));
        pollUiThread(
                () ->
                        getContentViewOfCurrentTab().getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_NOT_SENSITIVE);
    }

    @Test
    @MediumTest
    public void testSensitiveContentClientObserver() throws Exception {
        assertEquals(
                "Initially, the tab does not have sensitive content",
                getContentViewOfCurrentTab().getContentSensitivity(),
                View.CONTENT_SENSITIVITY_AUTO);

        final SensitiveContentClient client =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                SensitiveContentClient.fromWebContents(
                                        sActivityTestRule.getWebContents()));
        final TestSensitiveContentClientObserver observer =
                new TestSensitiveContentClientObserver();
        ThreadUtils.runOnUiThreadBlocking(() -> client.addObserver(observer));

        assertFalse(observer.getContentSensitivity());
        sActivityTestRule.loadUrl(mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(
                () ->
                        getContentViewOfCurrentTab().getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_SENSITIVE);
        assertTrue(observer.getContentSensitivity());

        sActivityTestRule.loadUrl(mTestServer.getURL(NOT_SENSITIVE_FILE));
        pollUiThread(
                () ->
                        getContentViewOfCurrentTab().getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_NOT_SENSITIVE);
        assertFalse(observer.getContentSensitivity());

        // After observation is removed, the observer will not be notified anymore.
        ThreadUtils.runOnUiThreadBlocking(() -> client.removeObserver(observer));
        sActivityTestRule.loadUrl(mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(
                () ->
                        getContentViewOfCurrentTab().getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_SENSITIVE);
        assertFalse(observer.getContentSensitivity());
    }

    @Test
    @MediumTest
    @EnableFeatures(SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)
    public void testTabHasSensitiveContentAttributeIsUpdated() throws Exception {
        assertEquals(
                "Initially, the tab does not have sensitive content",
                getContentViewOfCurrentTab().getContentSensitivity(),
                View.CONTENT_SENSITIVITY_AUTO);

        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        assertFalse(tab.getTabHasSensitiveContent());

        sActivityTestRule.loadUrl(mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(
                () ->
                        getContentViewOfCurrentTab().getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_SENSITIVE);
        assertTrue(tab.getTabHasSensitiveContent());

        sActivityTestRule.loadUrl(mTestServer.getURL(NOT_SENSITIVE_FILE));
        pollUiThread(
                () ->
                        getContentViewOfCurrentTab().getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_NOT_SENSITIVE);
        assertFalse(tab.getTabHasSensitiveContent());
    }

    @Test
    @LargeTest
    @EnableFeatures(SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)
    public void testRegularTabSwitcherBecomesSensitive() throws Exception {
        // Open a second tab.
        PageStation page = mPage.openGenericAppMenu().openNewTab();
        Tab secondTab = page.getLoadedTab();
        // Load sensitive content only into the second tab.
        sActivityTestRule.loadUrl(mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(() -> secondTab.getTabHasSensitiveContent());
        // Open the tab switcher.
        RegularTabSwitcherStation regularTabSwitcher = page.openRegularTabSwitcher();
        // Check that the tab switcher is sensitive.
        assertEquals(
                getFocusedTabSwitcherPane().getRootView().getContentSensitivity(),
                View.CONTENT_SENSITIVITY_SENSITIVE);

        // Close the second tab (the only tab with sensitive content).
        regularTabSwitcher = regularTabSwitcher.closeTabAtIndex(1, RegularTabSwitcherStation.class);
        // Select the only remaining tab.
        page = regularTabSwitcher.selectTabAtIndex(0, WebPageStation.newBuilder());
        // Open the tab switcher.
        regularTabSwitcher = page.openRegularTabSwitcher();
        // Check that the tab switcher is not sensitive anymore.
        assertEquals(
                getFocusedTabSwitcherPane().getRootView().getContentSensitivity(),
                View.CONTENT_SENSITIVITY_NOT_SENSITIVE);

        // Go back to a tab to cleanup tab state.
        regularTabSwitcher.selectTabAtIndex(0, WebPageStation.newBuilder());
    }

    @Test
    @LargeTest
    @EnableFeatures(SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)
    public void testIncognitoTabSwitcherBecomesSensitive() throws Exception {
        // Open the first incognito tab.
        PageStation page = mPage.openGenericAppMenu().openNewIncognitoTab();
        // Open the second incognito tab.
        page = page.openGenericAppMenu().openNewIncognitoTab();
        Tab secondIncognitoTab = page.getLoadedTab();
        // Load sensitive content only into the second incognito tab.
        sActivityTestRule.loadUrl(mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(() -> secondIncognitoTab.getTabHasSensitiveContent());
        // Open the incognito tab switcher.
        IncognitoTabSwitcherStation incognitoTabSwitcher = page.openIncognitoTabSwitcher();
        // Check that the incognito tab switcher is sensitive.
        assertEquals(
                getFocusedTabSwitcherPane().getRootView().getContentSensitivity(),
                View.CONTENT_SENSITIVITY_SENSITIVE);

        // Close the second incognito tab (the only tab with sensitive content).
        incognitoTabSwitcher =
                incognitoTabSwitcher.closeTabAtIndex(1, IncognitoTabSwitcherStation.class);
        // Select the only remaining incognito tab.
        page = incognitoTabSwitcher.selectTabAtIndex(0, WebPageStation.newBuilder());
        // Open the incognito tab switcher.
        incognitoTabSwitcher = page.openIncognitoTabSwitcher();
        // Check that the incognito tab switcher is not sensitive anymore.
        assertEquals(
                getFocusedTabSwitcherPane().getRootView().getContentSensitivity(),
                View.CONTENT_SENSITIVITY_NOT_SENSITIVE);

        // Go back to a tab to cleanup tab state.
        incognitoTabSwitcher.selectTabAtIndex(0, WebPageStation.newBuilder());
    }

    @Test
    @LargeTest
    @EnableFeatures(SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)
    public void testRegularTabSwitcherBecomesSensitiveWithTabGroups() throws Exception {
        Tab firstTab = mPage.getLoadedTab();
        // Open a second tab.
        PageStation page = mPage.openGenericAppMenu().openNewTab();
        Tab secondTab = page.getLoadedTab();
        // Load sensitive content only into the second tab.
        sActivityTestRule.loadUrl(mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(() -> secondTab.getTabHasSensitiveContent());
        // Group the tabs.
        TabUiTestHelper.createTabGroup(
                sActivityTestRule.getActivity(), false, List.of(firstTab, secondTab));
        // Open the tab switcher.
        RegularTabSwitcherStation regularTabSwitcher = page.openRegularTabSwitcher();
        // Check that the tab switcher is sensitive.
        assertEquals(
                getFocusedTabSwitcherPane().getRootView().getContentSensitivity(),
                View.CONTENT_SENSITIVITY_SENSITIVE);

        // Go back to a tab to cleanup tab state. It is easier to open a new tab than to go to an
        // existing tab.
        regularTabSwitcher.openAppMenu().openNewTab();
    }

    @Test
    @LargeTest
    @EnableFeatures(SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)
    public void testIncognitoTabSwitcherBecomesSensitiveWithTabGroups() throws Exception {
        // Open the first incognito tab.
        PageStation page = mPage.openGenericAppMenu().openNewIncognitoTab();
        Tab firstIncognitoTab = page.getLoadedTab();
        // Open the second incognito tab.
        page = page.openGenericAppMenu().openNewIncognitoTab();
        Tab secondIncognitoTab = page.getLoadedTab();
        // Load sensitive content only into the second incognito tab.
        sActivityTestRule.loadUrl(mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(() -> secondIncognitoTab.getTabHasSensitiveContent());
        // Group the incognito tabs.
        TabUiTestHelper.createTabGroup(
                sActivityTestRule.getActivity(),
                true,
                List.of(firstIncognitoTab, secondIncognitoTab));
        // Open the incognito tab switcher.
        IncognitoTabSwitcherStation incognitoTabSwitcher = page.openIncognitoTabSwitcher();
        // Check that the incognito tab switcher is sensitive.
        assertEquals(
                getFocusedTabSwitcherPane().getRootView().getContentSensitivity(),
                View.CONTENT_SENSITIVITY_SENSITIVE);

        // Go back to a tab to cleanup tab state. It is easier to open a new tab than to go to an
        // existing tab.
        incognitoTabSwitcher.openAppMenu().openNewTab();
    }

    @Test
    @LargeTest
    @EnableFeatures(SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)
    public void testTabGroupUiOpenedFromBottomToolbarBecomesSensitive() throws Exception {
        // Load sensitive content only into the first tab.
        Tab firstTab = mPage.getLoadedTab();
        sActivityTestRule.loadUrl(mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(() -> firstTab.getTabHasSensitiveContent());
        // Open a second tab.
        PageStation page = mPage.openGenericAppMenu().openNewTab();
        Tab secondTab = page.getLoadedTab();
        // Group the tabs.
        TabUiTestHelper.createTabGroup(
                sActivityTestRule.getActivity(), false, List.of(firstTab, secondTab));

        // Click on the "arrow button" from the bottom toolbar to display the tab group UI.
        onView(allOf(withId(R.id.toolbar_show_group_dialog_button))).perform(click());
        // Check that the tab group UI view is sensitive.
        onView(allOf(withId(R.id.dialog_parent_view)))
                .check(
                        (view, noMatchException) -> {
                            if (noMatchException != null) throw noMatchException;
                            assertEquals(
                                    view.getContentSensitivity(),
                                    View.CONTENT_SENSITIVITY_SENSITIVE);
                        });
        // Check that the content view is not sensitive. This ensures that the screen won't be
        // redacted if the tab group UI closes.
        assertNotEquals(
                getContentViewOfCurrentTab().getContentSensitivity(),
                View.CONTENT_SENSITIVITY_SENSITIVE);
    }

    private Pane getFocusedTabSwitcherPane() {
        return (Pane)
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                sActivityTestRule
                                        .getActivity()
                                        .getHubProvider()
                                        .getHubManagerSupplier()
                                        .get()
                                        .getPaneManager()
                                        .getFocusedPaneSupplier()
                                        .get());
    }

    private View getContentViewOfCurrentTab() {
        return sActivityTestRule.getActivity().getActivityTab().getContentView();
    }
}
