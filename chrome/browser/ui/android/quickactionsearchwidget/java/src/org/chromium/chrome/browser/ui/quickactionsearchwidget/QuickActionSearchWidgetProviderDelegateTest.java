// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.quickactionsearchwidget;

import android.app.Activity;
import android.appwidget.AppWidgetManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.IntentUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager.SearchActivityPreferences;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.util.UrlConstants;

/**
 * Tests for the QuickActionSearchWidgetProviderDelegate.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Features.EnableFeatures({ChromeFeatureList.QUICK_ACTION_SEARCH_WIDGET})
public class QuickActionSearchWidgetProviderDelegateTest {
    private static final class TestContext extends AdvancedMockContext {
        public TestContext() {
            super(InstrumentationRegistry.getInstrumentation()
                            .getTargetContext()
                            .getApplicationContext());
        }
    }

    @Rule
    public BaseActivityTestRule<Activity> mActivityTestRule =
            new BaseActivityTestRule<>(Activity.class);

    private View mWidgetView;
    private QuickActionSearchWidgetProviderDelegate mDelegate;
    private TestContext mContext;

    @Before
    public void setUp() {
        ChromeApplicationTestUtils.setUp(InstrumentationRegistry.getTargetContext());

        mContext = new TestContext();

        ComponentName searchActivityComponent = new ComponentName(mContext, SearchActivity.class);

        mDelegate = new QuickActionSearchWidgetProviderDelegate(
                R.layout.quick_action_search_widget_medium_layout, searchActivityComponent,
                IntentHandler.createTrustedOpenNewTabIntent(mContext, /*incognito=*/true),
                createDinoIntent(mContext));

        setUpViews();
    }

    @After
    public void tearDown() {
        ChromeApplicationTestUtils.tearDown(InstrumentationRegistry.getTargetContext());
    }

    @Test
    @SmallTest
    public void testSearchBarClick() throws Exception {
        QuickActionSearchWidgetTestUtils.assertSearchActivityLaunchedAfterAction(
                mActivityTestRule, () -> {
                    QuickActionSearchWidgetTestUtils.clickOnView(
                            mWidgetView, R.id.quick_action_search_widget_search_bar_container);
                }, /*shouldActivityLaunchVoiceMode=*/false);
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
    }

    @Test
    @SmallTest
    public void testIncognitoTabClick() throws Exception {
        QuickActionSearchWidgetTestUtils.assertIncognitoModeLaunchedAfterAction(
                mActivityTestRule, () -> {
                    QuickActionSearchWidgetTestUtils.clickOnView(
                            mWidgetView, R.id.incognito_quick_action_button);
                });
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
    }

    @Test
    @SmallTest
    public void testVoiceButtonClick() throws Exception {
        QuickActionSearchWidgetTestUtils.assertSearchActivityLaunchedAfterAction(
                mActivityTestRule, () -> {
                    QuickActionSearchWidgetTestUtils.clickOnView(
                            mWidgetView, R.id.voice_search_quick_action_button);
                }, /*shouldActivityLaunchVoiceMode=*/true);
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1225949")
    public void testDinoButtonClick() throws Exception {
        QuickActionSearchWidgetTestUtils.assertDinoGameLaunchedAfterAction(
                mActivityTestRule, () -> {
                    QuickActionSearchWidgetTestUtils.clickOnView(
                            mWidgetView, R.id.dino_quick_action_button);
                });
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
    }

    private void setUpViews() {
        FrameLayout parentView = new FrameLayout(mContext);

        AppWidgetManager widgetManager = AppWidgetManager.getInstance(mContext);
        SearchActivityPreferences prefs =
                new SearchActivityPreferences("EngineName", "http://engine", true, true, true);
        mWidgetView = mDelegate.createWidgetRemoteViews(mContext, prefs).apply(mContext, null);
    }

    /**
     * Test copy of {@link QuickActionSearchWidgetProvider#createDinoIntent}.
     */
    private static Intent createDinoIntent(final Context context) {
        String chromeDinoUrl = UrlConstants.CHROME_DINO_URL + "/";

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(chromeDinoUrl));
        intent.setComponent(new ComponentName(context, ChromeLauncherActivity.class));
        intent.putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        intent.putExtra(IntentHandler.EXTRA_INVOKED_FROM_APP_WIDGET, true);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        IntentUtils.addTrustedIntentExtras(intent);

        return intent;
    }
}
