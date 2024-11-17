// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.app.ActivityOptions;
import android.app.Instrumentation;
import android.content.Intent;
import android.os.Bundle;
import android.provider.Browser;
import android.text.TextUtils;

import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Assert;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.password_manager.PasswordManagerTestHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.WaitForFocusHelper;

import java.util.concurrent.TimeoutException;

/** Custom ActivityTestRule for tests using ChromeTabbedActivity */
public class ChromeTabbedActivityTestRule extends ChromeActivityTestRule<ChromeTabbedActivity> {
    private static final String TAG = "ChromeTabbedATR";

    public ChromeTabbedActivityTestRule() {
        super(ChromeTabbedActivity.class);
    }

    private Bundle noAnimationLaunchOptions() {
        return ActivityOptions.makeCustomAnimation(getActivity(), 0, 0).toBundle();
    }

    public void resumeMainActivityFromLauncher() throws Exception {
        Assert.assertNotNull(getActivity());
        Assert.assertTrue(
                ApplicationStatus.getStateForActivity(getActivity()) == ActivityState.STOPPED
                        || ApplicationStatus.getStateForActivity(getActivity())
                                == ActivityState.PAUSED);

        Intent launchIntent =
                getActivity()
                        .getPackageManager()
                        .getLaunchIntentForPackage(getActivity().getPackageName());
        launchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        launchIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        getActivity().startActivity(launchIntent, noAnimationLaunchOptions());
        ApplicationTestUtils.waitForActivityState(getActivity(), Stage.RESUMED);
    }

    /** Simulates starting Main Activity from launcher, blocks until it is started. */
    public void startMainActivityFromLauncher() {
        startMainActivityWithURL(null);
    }

    /**
     * Starts the Main activity on the specified URL. Passing a null URL ensures the default page is
     * loaded, which is the NTP with a new profile .
     */
    public void startMainActivityWithURL(String url) {
        // Only launch Chrome.
        Intent intent =
                new Intent(TextUtils.isEmpty(url) ? Intent.ACTION_MAIN : Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        startMainActivityFromIntent(intent, url);
    }

    /**
     * Starts the Main activity and open a blank page.
     * This is faster and less flakiness-prone than starting on the NTP.
     */
    public void startMainActivityOnBlankPage() {
        startMainActivityWithURL("about:blank");
    }

    /**
     * Starts the Main activity as if it was started from an external application, on the
     * specified URL.
     */
    public void startMainActivityFromExternalApp(String url, String appId) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        if (appId != null) {
            intent.putExtra(Browser.EXTRA_APPLICATION_ID, appId);
        }
        startMainActivityFromIntent(intent, url);
    }

    /**
     * Starts the Main activity using the passed intent, and using the specified URL. This method
     * waits for DEFERRED_STARTUP to fire as well as a subsequent idle-sync of the main looper
     * thread, and the initial tab must either complete its load or it must crash before this method
     * will return.
     */
    public void startMainActivityFromIntent(Intent intent, String url) {
        // Sets up password store. This fakes the Google Play Services password store for
        // integration tests.
        PasswordManagerTestHelper.setUpGmsCoreFakeBackends();
        prepareUrlIntent(intent, url);
        startActivityCompletely(intent);
        if (!getActivity().isInOverviewMode()) {
            waitForFirstFrame();
        }
    }

    @Override
    public void waitForActivityCompletelyLoaded() {
        CriteriaHelper.pollUiThread(
                () -> getActivity().getActivityTab() != null || getActivity().isInOverviewMode(),
                "Tab never selected/initialized and no overview page is showing.");

        if (!getActivity().isInOverviewMode()) {
            super.waitForActivityCompletelyLoaded();
        } else {
            Assert.assertTrue(waitForDeferredStartup());
        }
    }

    /**
     * Open an incognito tab by invoking the 'new incognito' menu item.
     * Returns when receiving the 'PAGE_LOAD_FINISHED' notification.
     */
    public Tab newIncognitoTabFromMenu() {
        final CallbackHelper createdCallback = new CallbackHelper();
        final CallbackHelper selectedCallback = new CallbackHelper();

        TabModel incognitoTabModel = getActivity().getTabModelSelector().getModel(true);
        TabModelObserver observer =
                new TabModelObserver() {
                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        createdCallback.notifyCalled();
                    }

                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        selectedCallback.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(() -> incognitoTabModel.addObserver(observer));

        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                getActivity(),
                R.id.new_incognito_tab_menu_id);

        try {
            createdCallback.waitForCallback(0);
        } catch (TimeoutException ex) {
            throw new AssertionError("Never received tab created event", ex);
        }
        try {
            selectedCallback.waitForCallback(0);
        } catch (TimeoutException ex) {
            throw new AssertionError("Never received tab selected event", ex);
        }
        ThreadUtils.runOnUiThreadBlocking(() -> incognitoTabModel.removeObserver(observer));

        Tab tab = getActivity().getActivityTab();

        ChromeTabUtils.waitForTabPageLoaded(tab, (String) null);
        NewTabPageTestUtils.waitForNtpLoaded(tab);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Log.d(TAG, "newIncognitoTabFromMenu <<");
        return tab;
    }

    /**
     * New multiple incognito tabs by invoking the 'new incognito' menu item n times.
     * @param n The number of tabs you want to create.
     */
    public void newIncognitoTabsFromMenu(int n) {
        while (n > 0) {
            newIncognitoTabFromMenu();
            --n;
        }
    }

    /**
     * Looks up the Omnibox in the view hierarchy and types the specified text into it, requesting
     * focus and using an inter-character delay of 200ms.
     *
     * @param oneCharAtATime Whether to type text one character at a time or all at once.
     */
    public void typeInOmnibox(String text, boolean oneCharAtATime) throws InterruptedException {
        final UrlBar urlBar = getActivity().findViewById(R.id.url_bar);
        Assert.assertNotNull(urlBar);

        WaitForFocusHelper.acquireFocusForView(urlBar);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (!oneCharAtATime) {
                        urlBar.setText(text);
                    }
                });

        if (oneCharAtATime) {
            final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
            for (int i = 0; i < text.length(); ++i) {
                instrumentation.sendStringSync(text.substring(i, i + 1));
                // Let's put some delay between key strokes to simulate a user pressing the keys.
                Thread.sleep(20);
            }
        }
    }
}
