// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withClassName;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.anything;
import static org.hamcrest.Matchers.containsString;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.url.GURL;

import java.util.Arrays;

/** Unit tests for {@link InstanceSwitcherCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class InstanceSwitcherCoordinatorTest extends BlankUiTestActivityTestCase {
    private LargeIconBridge mIconBridge;

    private ModalDialogManager mModalDialogManager;
    private ModalDialogManager.Presenter mAppModalPresenter;

    @Before
    public void setUp() throws Exception {
        super.setUpTest();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAppModalPresenter = new AppModalPresenter(getActivity());
                    mModalDialogManager =
                            new ModalDialogManager(
                                    mAppModalPresenter, ModalDialogManager.ModalDialogType.APP);
                });
        mIconBridge =
                new LargeIconBridge() {
                    @Override
                    public boolean getLargeIconForUrl(
                            final GURL pageUrl,
                            int desiredSizePx,
                            final LargeIconCallback callback) {
                        return true;
                    }
                };
    }

    @After
    public void tearDown() throws Exception {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.MULTI_INSTANCE_CLOSE_WINDOW_SKIP_CONFIRM, false);
    }

    @Test
    @SmallTest
    public void testOpenWindow() throws Exception {
        InstanceInfo[] instances =
                new InstanceInfo[] {
                    new InstanceInfo(
                            0, 57, InstanceInfo.Type.CURRENT, "url0", "title0", 1, 0, false),
                    new InstanceInfo(1, 58, InstanceInfo.Type.OTHER, "ur11", "title1", 2, 0, false),
                    new InstanceInfo(2, 59, InstanceInfo.Type.OTHER, "url2", "title2", 0, 0, false)
                };
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final int itemClickCount = itemClickCallbackHelper.getCallCount();
        Callback<InstanceInfo> openCallback = (item) -> itemClickCallbackHelper.notifyCalled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            openCallback,
                            null,
                            null,
                            false,
                            Arrays.asList(instances));
                });
        onData(anything()).inRoot(isDialog()).atPosition(1).perform(click());
        itemClickCallbackHelper.waitForCallback(itemClickCount);
    }

    @Test
    @SmallTest
    public void testNewWindow() throws Exception {
        InstanceInfo[] instances =
                new InstanceInfo[] {
                    new InstanceInfo(
                            0, 57, InstanceInfo.Type.CURRENT, "url0", "title0", 1, 0, false),
                    new InstanceInfo(1, 58, InstanceInfo.Type.OTHER, "ur11", "title1", 2, 0, false),
                    new InstanceInfo(2, 59, InstanceInfo.Type.OTHER, "url2", "title2", 0, 0, false)
                };
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final int itemClickCount = itemClickCallbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null,
                            null,
                            itemClickCallbackHelper::notifyCalled,
                            true,
                            Arrays.asList(instances));
                });
        // 0 ~ 2: instances. 3: 'new window' command.
        onData(anything()).inRoot(isDialog()).atPosition(3).perform(click());
        itemClickCallbackHelper.waitForCallback(itemClickCount);
    }

    @Test
    @SmallTest
    public void testCloseWindow() throws Exception {
        InstanceInfo[] instances =
                new InstanceInfo[] {
                    new InstanceInfo(
                            0, 57, InstanceInfo.Type.CURRENT, "url0", "title0", 1, 0, false),
                    new InstanceInfo(1, 58, InstanceInfo.Type.OTHER, "ur11", "title1", 2, 0, false),
                    new InstanceInfo(2, 59, InstanceInfo.Type.OTHER, "url2", "title2", 1, 1, false)
                };
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final int itemClickCount = itemClickCallbackHelper.getCallCount();
        Callback<InstanceInfo> closeCallback = (item) -> itemClickCallbackHelper.notifyCalled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null,
                            closeCallback,
                            null,
                            true,
                            Arrays.asList(instances));
                });

        // Verify that we have only [cancel] button.
        onView(withId(R.id.positive_button))
                .inRoot(isDialog())
                .check(matches(withEffectiveVisibility(GONE)));
        onView(withText(R.string.cancel)).check(matches(withEffectiveVisibility(VISIBLE)));

        onData(anything()).atPosition(2).onChildView(withId(R.id.more)).perform(click());
        onView(withText(R.string.instance_switcher_close_window))
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .perform(click());

        // Verify that we have both [cancel] [close] buttons now.
        onView(withText(R.string.close)).check(matches(withEffectiveVisibility(VISIBLE)));
        onView(withText(R.string.cancel)).check(matches(withEffectiveVisibility(VISIBLE)));

        onView(withText(R.string.close)).perform(click());
        itemClickCallbackHelper.waitForCallback(itemClickCount);
    }

    @Test
    @SmallTest
    @SuppressWarnings("unchecked")
    public void testMaxNumberOfWindows() throws Exception {
        InstanceInfo[] instances =
                new InstanceInfo[] {
                    new InstanceInfo(
                            0, 57, InstanceInfo.Type.CURRENT, "url0", "title0", 1, 0, false),
                    new InstanceInfo(1, 58, InstanceInfo.Type.OTHER, "ur11", "title1", 2, 0, false),
                    new InstanceInfo(2, 59, InstanceInfo.Type.OTHER, "url2", "title2", 1, 1, false),
                    new InstanceInfo(3, 60, InstanceInfo.Type.OTHER, "url3", "title3", 1, 1, false),
                    new InstanceInfo(4, 61, InstanceInfo.Type.OTHER, "url4", "title4", 1, 1, false)
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null,
                            null,
                            null,
                            false,
                            Arrays.asList(instances));
                });

        // Verify that we show a info message that users can have up to 5 windows when there are
        // already maximum number of windows.
        onData(anything())
                .inRoot(isDialog())
                .atPosition(5)
                .onChildView(withText(R.string.max_number_of_windows))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSkipCloseConfirmation() throws Exception {
        InstanceInfo[] instances =
                new InstanceInfo[] {
                    new InstanceInfo(
                            0, 57, InstanceInfo.Type.CURRENT, "url0", "title0", 1, 0, false),
                    new InstanceInfo(1, 58, InstanceInfo.Type.OTHER, "ur11", "title1", 2, 0, false),
                    new InstanceInfo(2, 59, InstanceInfo.Type.OTHER, "url2", "title2", 0, 0, false)
                };
        final CallbackHelper closeCallbackHelper = new CallbackHelper();
        int itemClickCount = closeCallbackHelper.getCallCount();
        Callback<InstanceInfo> closeCallback = (item) -> closeCallbackHelper.notifyCalled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null,
                            closeCallback,
                            null,
                            true,
                            Arrays.asList(instances));
                });

        // Closing a hidden, tab-less instance skips the confirmation.
        onData(anything())
                .inRoot(isDialog())
                .atPosition(2)
                .onChildView(withId(R.id.more))
                .perform(click());
        onView(withText(R.string.instance_switcher_close_window))
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .perform(click());
        closeCallbackHelper.waitForCallback(itemClickCount);

        // Verify that the close callback skips the confirmation when the skip checkbox
        // was ticked on.
        InstanceSwitcherCoordinator.setSkipCloseConfirmation();
        itemClickCount = closeCallbackHelper.getCallCount();
        onData(anything()).atPosition(1).onChildView(withId(R.id.more)).perform(click());
        onView(withText(R.string.instance_switcher_close_window))
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .perform(click());
        closeCallbackHelper.waitForCallback(itemClickCount);
    }

    @Test
    @SmallTest
    public void testBackOnConfirmDialog() throws Exception {
        InstanceInfo[] instances =
                new InstanceInfo[] {
                    new InstanceInfo(
                            0, 57, InstanceInfo.Type.CURRENT, "url0", "title0", 1, 0, false),
                    new InstanceInfo(1, 58, InstanceInfo.Type.OTHER, "ur11", "title1", 2, 0, false),
                    new InstanceInfo(2, 59, InstanceInfo.Type.OTHER, "url2", "title2", 1, 1, false)
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null,
                            null,
                            null,
                            true,
                            Arrays.asList(instances));
                });

        onData(anything())
                .inRoot(isDialog())
                .atPosition(2)
                .onChildView(withId(R.id.more))
                .perform(click());
        onView(withText(R.string.instance_switcher_close_window))
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .perform(click());
        onView(allOf(withText(R.string.instance_switcher_close_confirm_header)))
                .check(matches(isDisplayed()));

        onView(allOf(withId(R.id.title_icon), withEffectiveVisibility(VISIBLE))).perform(click());
        onView(allOf(withText(R.string.instance_switcher_header))).check(matches(isDisplayed()));
    }
}
