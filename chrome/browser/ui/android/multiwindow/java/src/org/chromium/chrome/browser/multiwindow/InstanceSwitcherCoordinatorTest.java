// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.contrib.RecyclerViewActions.actionOnItemAtPosition;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.isSelected;
import static androidx.test.espresso.matcher.ViewMatchers.withClassName;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.anything;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;

import android.util.Pair;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.matcher.BoundedMatcher;
import androidx.test.filters.SmallTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.concurrent.TimeoutException;

/** Unit tests for {@link InstanceSwitcherCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures({
    ChromeFeatureList.INSTANCE_SWITCHER_V2,
    ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT
})
public class InstanceSwitcherCoordinatorTest {
    private static final int MAX_INSTANCE_COUNT = 5;

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private LargeIconBridge mIconBridge;

    private ModalDialogManager mModalDialogManager;
    private ModalDialogManager.Presenter mAppModalPresenter;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(null);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAppModalPresenter = new AppModalPresenter(mActivityTestRule.getActivity());
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
                createPersistedInstances(
                        /* numActiveInstances= */ 3, /* numInactiveInstances= */ 0);
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final int itemClickCount = itemClickCallbackHelper.getCallCount();
        Callback<InstanceInfo> openCallback = (item) -> itemClickCallbackHelper.notifyCalled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            openCallback,
                            null,
                            null,
                            null,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });
        onData(anything()).inRoot(isDialog()).atPosition(1).perform(click());
        itemClickCallbackHelper.waitForCallback(itemClickCount);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.INSTANCE_SWITCHER_V2)
    public void testOpenWindow_InstanceSwitcherV2() throws Exception {
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 3, /* numInactiveInstances= */ 0);
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final int itemClickCount = itemClickCallbackHelper.getCallCount();
        Callback<InstanceInfo> openCallback = (item) -> itemClickCallbackHelper.notifyCalled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            openCallback,
                            null,
                            null,
                            null,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Verify "Open" button is disabled before a selection is made.
        onView(allOf(withId(R.id.positive_button), withText(R.string.open)))
                .inRoot(isDialog())
                .check(matches(not(isEnabled())));

        // Select the second item.
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(1, click()));

        // Switch to the the inactive instance tab, this should deselect the item.
        onView(allOf(withText("Inactive (0)"), isDescendantOfA(withId(R.id.tabs))))
                .perform(click());

        // Switch back to the active instance tab and select the same item.
        onView(allOf(withText("Active (3)"), isDescendantOfA(withId(R.id.tabs)))).perform(click());
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(1, click()));

        // Verify "Open" button is now enabled and open the selected instance.
        onView(allOf(withId(R.id.positive_button), withText(R.string.open)))
                .inRoot(isDialog())
                .check(matches(isEnabled()))
                .perform(click());
        itemClickCallbackHelper.waitForCallback(itemClickCount);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.INSTANCE_SWITCHER_V2)
    public void testRestoreWindow_InstanceSwitcherV2() throws Exception {
        // Initialize instance list with 2 active instances and 1 inactive instance.
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 2, /* numInactiveInstances= */ 1);
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final int itemClickCount = itemClickCallbackHelper.getCallCount();
        Callback<InstanceInfo> openCallback = (item) -> itemClickCallbackHelper.notifyCalled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            openCallback,
                            null,
                            null,
                            null,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Verify active list is showing when the menu is initially displayed.
        onView(withId(R.id.active_instance_list)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.positive_button), withText(R.string.open)))
                .inRoot(isDialog())
                .check(matches(not(isEnabled())));

        // Switch to inactive list.
        onView(allOf(withText("Inactive (1)"), isDescendantOfA(withId(R.id.tabs))))
                .perform(click());

        // Verify "Restore" button is disabled before a selection is made.
        onView(allOf(withId(R.id.positive_button), withText(R.string.restore)))
                .inRoot(isDialog())
                .check(matches(not(isEnabled())));

        // Select the first item.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(0, click()));

        // Verify "Restore" button is now enabled and restore the selected instance.
        onView(allOf(withId(R.id.positive_button), withText(R.string.restore)))
                .inRoot(isDialog())
                .check(matches(isEnabled()))
                .perform(click());
        itemClickCallbackHelper.waitForCallback(itemClickCount);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.INSTANCE_SWITCHER_V2)
    public void testRestoreButtonDisabledWhenSelectedInstanceClosed_InstanceSwitcherV2()
            throws Exception {
        // Initialize instance list with 1 active instance and 3 inactive instances.
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 1, /* numInactiveInstances= */ 3);
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final CallbackHelper closeCallbackHelper = new CallbackHelper();
        Callback<InstanceInfo> openCallback = (item) -> itemClickCallbackHelper.notifyCalled();
        Callback<InstanceInfo> closeCallback = (item) -> closeCallbackHelper.notifyCalled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            openCallback,
                            closeCallback,
                            null,
                            null,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });
        onView(withId(R.id.active_instance_list)).inRoot(isDialog()).check(matches(isDisplayed()));
        // Switch to inactive list.
        onView(allOf(withText("Inactive (3)"), isDescendantOfA(withId(R.id.tabs))))
                .perform(click());

        // Verify "Restore" button is disabled before a selection is made.
        onView(allOf(withId(R.id.positive_button), withText(R.string.restore)))
                .inRoot(isDialog())
                .check(matches(not(isEnabled())));

        // Select the first item.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(0, click()));

        // Verify "Restore" button is now enabled.
        onView(allOf(withId(R.id.positive_button), withText(R.string.restore)))
                .inRoot(isDialog())
                .check(matches(isEnabled()));

        // Close the selected instance.
        closeInstanceAt(0, /* isActiveInstance= */ false, closeCallbackHelper);

        // Verify "Restore" button is now disabled.
        onView(allOf(withId(R.id.positive_button), withText(R.string.restore)))
                .inRoot(isDialog())
                .check(matches(not(isEnabled())));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.INSTANCE_SWITCHER_V2)
    public void testRestoreButtonEnabledWhenOtherInstanceClosed_InstanceSwitcherV2()
            throws Exception {
        // Initialize instance list with 1 active instance and 3 inactive instances.
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 1, /* numInactiveInstances= */ 3);
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final CallbackHelper closeCallbackHelper = new CallbackHelper();
        Callback<InstanceInfo> openCallback = (item) -> itemClickCallbackHelper.notifyCalled();
        Callback<InstanceInfo> closeCallback = (item) -> closeCallbackHelper.notifyCalled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            openCallback,
                            closeCallback,
                            null,
                            null,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });
        onView(withId(R.id.active_instance_list)).inRoot(isDialog()).check(matches(isDisplayed()));
        // Switch to inactive list.
        onView(allOf(withText("Inactive (3)"), isDescendantOfA(withId(R.id.tabs))))
                .perform(click());

        // Verify "Restore" button is disabled before a selection is made.
        onView(allOf(withId(R.id.positive_button), withText(R.string.restore)))
                .inRoot(isDialog())
                .check(matches(not(isEnabled())));

        // Select the second item.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(1, click()));

        // Verify "Restore" button is now enabled.
        onView(allOf(withId(R.id.positive_button), withText(R.string.restore)))
                .inRoot(isDialog())
                .check(matches(isEnabled()));

        // Close the first instance.
        closeInstanceAt(0, /* isActiveInstance= */ false, closeCallbackHelper);

        // Verify "Restore" button is still enabled.
        onView(allOf(withId(R.id.positive_button), withText(R.string.restore)))
                .inRoot(isDialog())
                .check(matches(isEnabled()));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.INSTANCE_SWITCHER_V2)
    public void testBlockInstanceRestorationAtLimit_InstanceSwitcherV2() throws Exception {
        // Initialize instance list with MAX_INSTANCE_COUNT active instances, 1 inactive instance.
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ MAX_INSTANCE_COUNT,
                        /* numInactiveInstances= */ 1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null,
                            null,
                            null,
                            null,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Verify that the active list is showing when the menu is initially displayed.
        onView(withId(R.id.active_instance_list)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.positive_button), withText(R.string.open)))
                .inRoot(isDialog())
                .check(matches(not(isEnabled())));

        // Switch to the inactive list.
        onView(allOf(withText("Inactive (1)"), isDescendantOfA(withId(R.id.tabs))))
                .perform(click());

        // Verify that the "Restore" button is disabled before a selection is made.
        onView(allOf(withId(R.id.positive_button), withText(R.string.restore)))
                .inRoot(isDialog())
                .check(matches(not(isEnabled())));

        // Click on the inactive list item.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(0, click()));

        // Verify that the "Restore" button is still disabled.
        onView(allOf(withId(R.id.positive_button), withText(R.string.restore)))
                .inRoot(isDialog())
                .check(matches(not(isEnabled())));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.INSTANCE_SWITCHER_V2)
    public void testActiveInactiveTabSwitch_InstanceSwitcherV2() throws Exception {
        // Initialize instance list with 2 active instances and 1 inactive instance.
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 2, /* numInactiveInstances= */ 1);
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final int itemClickCount = itemClickCallbackHelper.getCallCount();
        Callback<InstanceInfo> openCallback = (item) -> itemClickCallbackHelper.notifyCalled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            openCallback,
                            null,
                            null,
                            null,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        onView(withId(R.id.active_instance_list)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.positive_button), withText(R.string.open)))
                .inRoot(isDialog())
                .check(matches(not(isEnabled())));
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));

        onView(allOf(withText("Inactive (1)"), isDescendantOfA(withId(R.id.tabs))))
                .perform(click());

        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(0, click()));

        // Verify "Restore" button is enabled.
        onView(allOf(withId(R.id.positive_button), withText(R.string.restore)))
                .inRoot(isDialog())
                .check(matches(isEnabled()))
                .perform(click());
        itemClickCallbackHelper.waitForCallback(itemClickCount);
    }

    @Test
    @SmallTest
    public void testNewRegularWindow() throws Exception {
        testNewWindow(/* isIncognitoWindow= */ false, R.string.menu_new_window);
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW,
        ChromeFeatureList.INSTANCE_SWITCHER_V2,
        ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT
    })
    public void testNewIncognitoWindow() throws Exception {
        testNewWindow(/* isIncognitoWindow= */ true, R.string.menu_new_incognito_window);
    }

    private void testNewWindow(boolean isIncognitoWindow, int stringId) throws Exception {
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 3, /* numInactiveInstances= */ 0);
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final int itemClickCount = itemClickCallbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null,
                            null,
                            null,
                            itemClickCallbackHelper::notifyCalled,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            isIncognitoWindow);
                });

        // 0 ~ 2: instances. 3: 'new window' command.
        onView(withId(R.id.new_window))
                .inRoot(isDialog())
                .check(matches(hasDescendant(withText(stringId))))
                .perform(click());
        itemClickCallbackHelper.waitForCallback(itemClickCount);
    }

    @Test
    @SmallTest
    public void testCloseWindow() throws Exception {
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 3, /* numInactiveInstances= */ 0);
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final int itemClickCount = itemClickCallbackHelper.getCallCount();
        Callback<InstanceInfo> closeCallback = (item) -> itemClickCallbackHelper.notifyCalled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null,
                            closeCallback,
                            null,
                            null,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
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
                createPersistedInstances(
                        /* numActiveInstances= */ MAX_INSTANCE_COUNT,
                        /* numInactiveInstances= */ 0);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null,
                            null,
                            null,
                            null,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Verify that we show a info message that users can have up to 5 windows when there are
        // already maximum number of windows.
        String text =
                mActivityTestRule
                        .getActivity()
                        .getResources()
                        .getString(R.string.max_number_of_windows, MAX_INSTANCE_COUNT);
        onData(anything())
                .inRoot(isDialog())
                .atPosition(5)
                .onChildView(withText(text))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testExceedsMaxNumberOfWindows() throws Exception {
        // Simulate persistence of MAX_INSTANCE_COUNT active instances and 1 inactive instance.
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ MAX_INSTANCE_COUNT,
                        /* numInactiveInstances= */ 1);

        final CallbackHelper newWindowCallbackHelper = new CallbackHelper();
        final int newWindowClickCount = newWindowCallbackHelper.getCallCount();

        final CallbackHelper closeCallbackHelper = new CallbackHelper();
        Callback<InstanceInfo> closeCallback = (item) -> closeCallbackHelper.notifyCalled();
        InstanceSwitcherCoordinator.setSkipCloseConfirmation();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null,
                            closeCallback,
                            null,
                            newWindowCallbackHelper::notifyCalled,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Verify that we show info message that users can have up to 5 windows when there are more
        // than maximum number of windows.
        String text =
                mActivityTestRule
                        .getActivity()
                        .getResources()
                        .getString(R.string.max_number_of_windows, MAX_INSTANCE_COUNT);
        onData(anything())
                .inRoot(isDialog())
                .atPosition(6)
                .onChildView(withText(text))
                .check(matches(isDisplayed()));

        // Close an instance.
        closeInstanceAt(2, closeCallbackHelper);

        // Verify that we show info message that users can have up to 5 windows when there are
        // maximum number of windows.
        onData(anything())
                .inRoot(isDialog())
                .atPosition(5)
                .onChildView(withText(text))
                .check(matches(isDisplayed()));

        // Close another instance.
        closeInstanceAt(2, closeCallbackHelper);

        // List positions 0 ~ 3: instances. 4: 'new window' command.
        onData(anything()).inRoot(isDialog()).atPosition(4).perform(click());
        newWindowCallbackHelper.waitForCallback(newWindowClickCount);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.INSTANCE_SWITCHER_V2)
    @DisabledTest(message = "crbug.com/430445069")
    public void testExceedsMaxNumberOfWindows_InstanceSwitcherV2() throws Exception {
        // Simulate persistence of MAX_INSTANCE_COUNT active instances and 1 inactive instance.
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ MAX_INSTANCE_COUNT,
                        /* numInactiveInstances= */ 1);

        final CallbackHelper newWindowCallbackHelper = new CallbackHelper();
        final int newWindowClickCount = newWindowCallbackHelper.getCallCount();

        final CallbackHelper closeCallbackHelper = new CallbackHelper();
        Callback<InstanceInfo> closeCallback = (item) -> closeCallbackHelper.notifyCalled();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null,
                            closeCallback,
                            null,
                            newWindowCallbackHelper::notifyCalled,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Verify that we show max info message that users can have up to 5 windows when there are
        // more than maximum number of windows.
        String activeMaxInfoText =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string.max_number_of_windows_instance_switcher_v2_active_tab,
                                MAX_INSTANCE_COUNT - 1);

        // Verify that we show the max info message for the active tab.
        onView(withId(R.id.max_instance_info))
                .inRoot(isDialog())
                .check(matches(withText(activeMaxInfoText)))
                .check(matches(isDisplayed()));

        // Verify the "+ New window" command is not displayed.
        onView(withId(R.id.new_window))
                .inRoot(isDialog())
                .check(matches(withEffectiveVisibility(GONE)));

        // Generate the expected max info text for the inactive tab.
        String inactiveMaxInfoText =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string.max_number_of_windows_instance_switcher_v2_inactive_tab,
                                MAX_INSTANCE_COUNT - 1);

        // Switch to the inactive instance tab.
        onView(
                        allOf(
                                withText(String.format("Inactive (%d)", 1)),
                                isDescendantOfA(withId(R.id.tabs))))
                .perform(click());

        // Verify we show the max instance info message in the inactive list.
        onView(withId(R.id.max_instance_info))
                .inRoot(isDialog())
                .check(matches(withText(inactiveMaxInfoText)))
                .check(matches(isDisplayed()));

        closeInstanceAt(0, /* isActiveInstance= */ false, closeCallbackHelper);

        // Switch to the active instance tab.
        onView(
                        allOf(
                                withText(String.format("Active (%d)", MAX_INSTANCE_COUNT)),
                                isDescendantOfA(withId(R.id.tabs))))
                .perform(click());

        // Close an active instance (e.g., the third one, at index 2).
        closeInstanceAt(2, /* isActiveInstance= */ true, closeCallbackHelper);

        // Verify max instance info message is gone.
        onView(withId(R.id.max_instance_info))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));

        // Verify the "+ New window" command is now displayed and click it.
        onView(withId(R.id.new_window))
                .inRoot(isDialog())
                .check(matches(isDisplayed())) // Assert it's now visible
                .perform(click());
        newWindowCallbackHelper.waitForCallback(newWindowClickCount);
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.INSTANCE_SWITCHER_V2,
        ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT
    })
    public void testMaxInfoTextRes_RobustWindowManagement() throws Exception {
        // Simulate persistence of MAX_INSTANCE_COUNT active instances and 1 inactive instance.
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ MAX_INSTANCE_COUNT,
                        /* numInactiveInstances= */ 1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null,
                            null,
                            null,
                            null,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Verify that we show max info message that users can have up to 5 windows when there are
        // more than maximum number of windows.
        String activeMaxInfoText =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string.max_number_of_windows_instance_switcher_active_tab,
                                MAX_INSTANCE_COUNT - 1);

        // Verify that we show the max info message for the active tab.
        onView(withId(R.id.max_instance_info))
                .inRoot(isDialog())
                .check(matches(withText(activeMaxInfoText)))
                .check(matches(isDisplayed()));

        // Verify the "+ New window" command is not displayed.
        onView(withId(R.id.new_window))
                .inRoot(isDialog())
                .check(matches(withEffectiveVisibility(GONE)));

        // Generate the expected max info text for the inactive tab.
        String inactiveMaxInfoText =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string.max_number_of_windows_instance_switcher_inactive_tab,
                                MAX_INSTANCE_COUNT - 1);

        // Switch to the inactive instance tab.
        onView(
                        allOf(
                                withText(String.format("Inactive (%d)", 1)),
                                isDescendantOfA(withId(R.id.tabs))))
                .perform(click());

        // Verify we show the max instance info message in the inactive list.
        onView(withId(R.id.max_instance_info))
                .inRoot(isDialog())
                .check(matches(withText(inactiveMaxInfoText)))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.INSTANCE_SWITCHER_V2)
    public void testDeselectWindow() throws Exception {
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 3, /* numInactiveInstances= */ 0);
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final int itemClickCount = itemClickCallbackHelper.getCallCount();
        Callback<InstanceInfo> openCallback = (item) -> itemClickCallbackHelper.notifyCalled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            openCallback,
                            null,
                            null,
                            null,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Verify "Open" button is disabled before a selection is made.
        onView(allOf(withId(R.id.positive_button), withText(R.string.open)))
                .inRoot(isDialog())
                .check(matches(not(isEnabled())));

        // Select the second item.
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(1, click()));

        // Select the same item again, this should deselect the item.
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(1, click()));

        // Verify "Open" button is now disabled.
        onView(allOf(withId(R.id.positive_button), withText(R.string.open)))
                .inRoot(isDialog())
                .check(matches(not(isEnabled())));

        // Select the same item again.
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(1, click()));

        // Verify "Open" button is now enabled and open the selected instance.
        onView(allOf(withId(R.id.positive_button), withText(R.string.open)))
                .inRoot(isDialog())
                .check(matches(isEnabled()))
                .perform(click());
        itemClickCallbackHelper.waitForCallback(itemClickCount);
    }

    @Test
    @SmallTest
    public void testSkipCloseConfirmation() throws Exception {
        InstanceInfo[] instances =
                new InstanceInfo[] {
                    new InstanceInfo(
                            /* instanceId= */ 0,
                            /* taskId= */ 57,
                            InstanceInfo.Type.CURRENT,
                            "url0",
                            "title0",
                            /* customTitle= */ null,
                            /* tabCount= */ 0,
                            /* incognitoTabCount= */ 0,
                            /* isIncognitoSelected= */ false,
                            /* lastAccessedTime= */ 0,
                            /* closedByUser= */ false),
                    new InstanceInfo(
                            /* instanceId= */ 1,
                            /* taskId= */ 58,
                            InstanceInfo.Type.OTHER,
                            "ur11",
                            "title1",
                            /* customTitle= */ null,
                            /* tabCount= */ 2,
                            /* incognitoTabCount= */ 0,
                            /* isIncognitoSelected= */ false,
                            /* lastAccessedTime= */ 0,
                            /* closedByUser= */ false),
                    new InstanceInfo(
                            /* instanceId= */ 2,
                            /* taskId= */ 59,
                            InstanceInfo.Type.OTHER,
                            "url2",
                            "title2",
                            /* customTitle= */ null,
                            /* tabCount= */ 0,
                            /* incognitoTabCount= */ 0,
                            /* isIncognitoSelected= */ false,
                            /* lastAccessedTime= */ 0,
                            /* closedByUser= */ false)
                };
        final CallbackHelper closeCallbackHelper = new CallbackHelper();
        Callback<InstanceInfo> closeCallback = (item) -> closeCallbackHelper.notifyCalled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null,
                            closeCallback,
                            null,
                            null,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Closing a hidden, tab-less instance skips the confirmation.
        closeInstanceAt(2, closeCallbackHelper);

        // Verify that the close callback skips the confirmation when the skip checkbox
        // was ticked on.
        InstanceSwitcherCoordinator.setSkipCloseConfirmation();
        closeInstanceAt(1, closeCallbackHelper);
    }

    @Test
    @SmallTest
    public void testCancelButton() throws Exception {
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 3, /* numInactiveInstances= */ 0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null,
                            null,
                            null,
                            null,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
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

        onView(withText(R.string.cancel)).perform(click());
        // The cancel button does not close the instance switcher dialog.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mModalDialogManager.isShowing(), Matchers.is(true));
                });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.INSTANCE_SWITCHER_V2)
    public void testLastAccessedStringsForInstances() {
        final String expectedCurrentString = "Current window";
        final String expectedOtherString = "2 days ago";

        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 3, /* numInactiveInstances= */ 0);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null,
                            null,
                            null,
                            null,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Verify that the "Current window" string is at position 0.
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .check(
                        matches(
                                atPosition(
                                        0,
                                        hasDescendant(
                                                allOf(
                                                        withId(R.id.last_accessed),
                                                        withText(expectedCurrentString),
                                                        isDisplayed())))));

        // Verify that the "2 days ago" string is at position 2.
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .check(
                        matches(
                                atPosition(
                                        2,
                                        hasDescendant(
                                                allOf(
                                                        withId(R.id.last_accessed),
                                                        withText(expectedOtherString),
                                                        isDisplayed())))));
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.INSTANCE_SWITCHER_V2,
        ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT
    })
    public void testRenameWindow() throws Exception {
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 3, /* numInactiveInstances= */ 0);
        final CallbackHelper renameCallbackHelper = new CallbackHelper();
        final int renameCallbackCount = renameCallbackHelper.getCallCount();
        Callback<Pair<Integer, String>> renameCallback =
                (result) -> {
                    renameCallbackHelper.notifyCalled();
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null, // openCallback
                            null, // closeCallback
                            renameCallback,
                            null, // newWindowAction
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Click on the 'more' button for the second instance.
        clickMoreButtonAtPosition(1, "title1");

        // Check that "Name" is an option and click it.
        onView(withText(R.string.instance_switcher_name_window))
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .check(matches(isDisplayed()))
                .perform(click());

        // Check that the "Name this window" dialog is shown.
        onView(withText(R.string.instance_switcher_name_window_confirm_header))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        // Enter a new name and save.
        final String newName = "test name";
        onView(withId(R.id.title_input_text)).inRoot(isDialog()).perform(replaceText(newName));
        onView(withText(R.string.save))
                .inRoot(isDialog())
                .check(matches(isDisplayed()))
                .perform(click());

        // Check that the instance title is updated in the list.
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .check(matches(atPosition(1, hasDescendant(withText(newName)))));

        // Reopen the name window dialog.
        clickMoreButtonAtPosition(1, newName);
        onView(withText(R.string.instance_switcher_name_window))
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .check(matches(isDisplayed()))
                .perform(click());

        // Check that the input text field is updated.
        onView(withId(R.id.title_input_text)).inRoot(isDialog()).check(matches(withText(newName)));

        // Check that the rename callback was called.
        assertEquals(renameCallbackCount + 1, renameCallbackHelper.getCallCount());
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.INSTANCE_SWITCHER_V2,
        ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT
    })
    public void testRenameWindow_inactiveInstance() {
        // Initialize instance list with 2 active instances and 1 inactive instance.
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 2, /* numInactiveInstances= */ 1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null, // openCallback
                            null, // closeCallback
                            null, // renameCallback
                            null, // newWindowAction
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        onView(withId(R.id.active_instance_list)).inRoot(isDialog()).check(matches(isDisplayed()));

        // Switch to inactive list.
        onView(allOf(withText("Inactive (1)"), isDescendantOfA(withId(R.id.tabs))))
                .perform(click());

        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        // Check to make sure the more button is not visible.
        onView(allOf(withId(R.id.more), isDescendantOfA(withId(R.id.inactive_instance_list))))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));

        // Verify content description of the close button on the single inactive instance.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .check(
                        matches(
                                atPosition(
                                        0,
                                        hasDescendant(
                                                allOf(
                                                        withId(R.id.close_button),
                                                        withContentDescription("Close title2"))))));
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.INSTANCE_SWITCHER_V2,
        ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT
    })
    public void testRenameWindowWithEmptyName() throws Exception {
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 3, /* numInactiveInstances= */ 0);
        final CallbackHelper renameCallbackHelper = new CallbackHelper();
        final int renameCallbackCount = renameCallbackHelper.getCallCount();
        Callback<Pair<Integer, String>> renameCallback =
                (result) -> {
                    renameCallbackHelper.notifyCalled();
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null, // openCallback
                            null, // closeCallback
                            renameCallback,
                            null, // newWindowAction
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Click on the 'more' button for the second instance.
        clickMoreButtonAtPosition(1, "title1");

        // Check that "Name" is an option and click it.
        onView(withText(R.string.instance_switcher_name_window))
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .check(matches(isDisplayed()))
                .perform(click());

        // Check that the "Name this window" dialog is shown.
        onView(withText(R.string.instance_switcher_name_window_confirm_header))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        // Enter an empty name and save.
        onView(withId(R.id.title_input_text)).inRoot(isDialog()).perform(replaceText(""));
        onView(withText(R.string.save)).inRoot(isDialog()).perform(click());

        // Check that the rename callback was called.
        assertEquals(renameCallbackCount + 1, renameCallbackHelper.getCallCount());

        // Check that the instance title is updated to the default name in the list.
        String defaultName = instances[1].title;
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .check(matches(atPosition(1, hasDescendant(withText(defaultName)))));

        // Reopen the name window dialog.
        clickMoreButtonAtPosition(1, defaultName);
        onView(withText(R.string.instance_switcher_name_window))
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .check(matches(isDisplayed()))
                .perform(click());

        // Check that the input text field is updated.
        onView(withId(R.id.title_input_text))
                .inRoot(isDialog())
                .check(matches(withText(defaultName)));
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.INSTANCE_SWITCHER_V2,
        ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT
    })
    public void testCancelRenameWindow() throws Exception {
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 3, /* numInactiveInstances= */ 0);
        final CallbackHelper renameCallbackHelper = new CallbackHelper();
        Callback<Pair<Integer, String>> renameCallback =
                (result) -> {
                    renameCallbackHelper.notifyCalled();
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null, // openCallback
                            null, // closeCallback
                            renameCallback,
                            null, // newWindowAction
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Click on the 'more' button for the second instance.
        clickMoreButtonAtPosition(1, "title1");

        // Check that "Name" is an option and click it.
        onView(withText(R.string.instance_switcher_name_window))
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .check(matches(isDisplayed()))
                .perform(click());

        // Check that the "Name this window" dialog is shown.
        onView(withText(R.string.instance_switcher_name_window_confirm_header))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        // Click the cancel button.
        onView(withText(R.string.cancel)).inRoot(isDialog()).perform(click());

        // Check that the "Name this window" dialog is dismissed.
        onView(withText(R.string.instance_switcher_header))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        // Check that the rename callback was not called.
        assertEquals(0, renameCallbackHelper.getCallCount());
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.INSTANCE_SWITCHER_V2,
        ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT + ":bulk_close/true"
    })
    public void testMultiSelectInactiveWindows_robustWindowManagement() throws Exception {
        // Initialize instance list with 2 active instances and 3 inactive instances.
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 2, /* numInactiveInstances= */ 3);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null, // openCallback
                            null, // closeCallback
                            null, // renameCallback
                            null, // newWindowAction
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        onView(withId(R.id.active_instance_list)).inRoot(isDialog()).check(matches(isDisplayed()));
        // Switch to inactive list.
        onView(allOf(withText("Inactive (3)"), isDescendantOfA(withId(R.id.tabs))))
                .perform(click());

        // Verify "Restore" button is disabled before a selection is made.
        onView(allOf(withId(R.id.positive_button), withText(R.string.restore)))
                .inRoot(isDialog())
                .check(matches(not(isEnabled())));

        // Select the first item.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(0, click()));

        // Verify "Restore" button is now enabled.
        onView(allOf(withId(R.id.positive_button), withText(R.string.restore)))
                .inRoot(isDialog())
                .check(matches(isEnabled()));

        // Verify the first item is selected.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .check(matches(atPosition(0, isSelected())));

        // Verify the close buttons are enabled.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .check(matches(atPosition(0, hasDescendant(allOf(withId(R.id.close_button), isEnabled())))))
                .check(matches(atPosition(1, hasDescendant(allOf(withId(R.id.close_button), isEnabled())))));

        // Select the second item.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(1, click()));

        // Verify "Restore" button is now disabled because more than one item is selected.
        onView(allOf(withId(R.id.positive_button), withText(R.string.restore)))
                .inRoot(isDialog())
                .check(matches(not(isEnabled())));

        // Verify both items are selected.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .check(matches(atPosition(0, isSelected())))
                .check(matches(atPosition(1, isSelected())));

        // Verify the close buttons are disabled.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .check(matches(atPosition(0, hasDescendant(allOf(withId(R.id.close_button), not(isEnabled()))))))
                .check(matches(atPosition(1, hasDescendant(allOf(withId(R.id.close_button), not(isEnabled()))))));

        // Deselect the first item.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(0, click()));

        // Verify "Restore" button is enabled again as only one item is selected.
        onView(allOf(withId(R.id.positive_button), withText(R.string.restore)))
                .inRoot(isDialog())
                .check(matches(isEnabled()));

        // Verify the first item is not selected, and the second one is.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .check(matches(atPosition(0, not(isSelected()))))
                .check(matches(atPosition(1, isSelected())));

        // Verify the close buttons are enabled again.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .check(matches(atPosition(0, hasDescendant(allOf(withId(R.id.close_button), isEnabled())))))
                .check(matches(atPosition(1, hasDescendant(allOf(withId(R.id.close_button), isEnabled())))));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.INSTANCE_SWITCHER_V2)
    public void testSingleSelectInactiveWindows_noRobustWindowManagement() throws Exception {
        // Initialize instance list with 2 active instances and 3 inactive instances.
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 2, /* numInactiveInstances= */ 3);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            null, // openCallback
                            null, // closeCallback
                            null, // renameCallback
                            null, // newWindowAction
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });
        onView(withId(R.id.active_instance_list)).inRoot(isDialog()).check(matches(isDisplayed()));
        // Switch to inactive list.
        onView(allOf(withText("Inactive (3)"), isDescendantOfA(withId(R.id.tabs))))
                .perform(click());

        // Verify "Restore" button is disabled before a selection is made.
        onView(allOf(withId(R.id.positive_button), withText(R.string.restore)))
                .inRoot(isDialog())
                .check(matches(not(isEnabled())));

        // Select the first item.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(0, click()));

        // Verify "Restore" button is now enabled.
        onView(allOf(withId(R.id.positive_button), withText(R.string.restore)))
                .inRoot(isDialog())
                .check(matches(isEnabled()));

        // Verify the first item is selected.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .check(matches(atPosition(0, isSelected())));

        // Verify close button is enabled.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .check(matches(atPosition(0, hasDescendant(allOf(withId(R.id.close_button), isEnabled())))));

        // Select the second item.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(1, click()));

        // Verify "Restore" button is still enabled.
        onView(allOf(withId(R.id.positive_button), withText(R.string.restore)))
                .inRoot(isDialog())
                .check(matches(isEnabled()));

        // Verify the second item is selected, and the first one is not.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .check(matches(atPosition(0, not(isSelected()))))
                .check(matches(atPosition(1, isSelected())));

        // Verify close button is still enabled.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .check(matches(atPosition(0, hasDescendant(allOf(withId(R.id.close_button), isEnabled())))));
    }

    private InstanceInfo[] createPersistedInstances(
            int numActiveInstances, int numInactiveInstances) {
        int totalInstances = numActiveInstances + numInactiveInstances;
        InstanceInfo[] instances = new InstanceInfo[totalInstances];

        int taskId = 50;
        // Set instance0 as the current instance.
        instances[0] =
                new InstanceInfo(
                        /* instanceId= */ 0,
                        taskId++,
                        InstanceInfo.Type.CURRENT,
                        "url0",
                        "title0",
                        /* customTitle= */ null,
                        /* tabCount= */ 1,
                        /* incognitoTabCount= */ 1,
                        /* isIncognitoSelected= */ false,
                        /* lastAccessedTime= */ 0,
                        /* closedByUser= */ false);

        // Create other active instances.
        for (int i = 1; i < numActiveInstances; i++) {
            instances[i] =
                    new InstanceInfo(
                            /* instanceId= */ i,
                            taskId++,
                            InstanceInfo.Type.OTHER,
                            "url" + i,
                            "title" + i,
                            /* customTitle= */ null,
                            /* tabCount= */ 1,
                            /* incognitoTabCount= */ 0,
                            /* isIncognitoSelected= */ false,
                            /* lastAccessedTime= */ getDaysAgoMillis(i),
                            /* closedByUser= */ false);
        }

        // Create inactive instances.
        for (int i = numActiveInstances; i < totalInstances; i++) {
            instances[i] =
                    new InstanceInfo(
                            /* instanceId= */ i,
                            /* taskId= */ -1,
                            InstanceInfo.Type.OTHER,
                            "url" + i,
                            "title" + i,
                            /* customTitle= */ null,
                            /* tabCount= */ 1,
                            /* incognitoTabCount= */ 0,
                            /* isIncognitoSelected= */ false,
                            /* lastAccessedTime= */ 0,
                            /* closedByUser= */ false);
        }

        return instances;
    }

    /* Returns the millisecond timestamp for a given number of days in the past. */
    private long getDaysAgoMillis(int lastAccessedDaysAgo) {
        return TimeUtils.currentTimeMillis() - lastAccessedDaysAgo * 24L * 60L * 60L * 1000L;
    }

    /* For use in instance switcher v1. */
    private void closeInstanceAt(int position, CallbackHelper closeCallbackHelper)
            throws TimeoutException {
        int closeCallbackCount = closeCallbackHelper.getCallCount();
        onData(anything())
                .inRoot(isDialog())
                .atPosition(position)
                .onChildView(withId(R.id.more))
                .perform(click());
        onView(withText(R.string.instance_switcher_close_window))
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .perform(click());
        closeCallbackHelper.waitForCallback(closeCallbackCount);
    }

    /* For use in instance switcher v2. */
    private void closeInstanceAt(
            int position, boolean isActiveInstance, CallbackHelper closeCallbackHelper)
            throws TimeoutException {
        int closeCallbackCount = closeCallbackHelper.getCallCount();
        onView(withId(isActiveInstance ? R.id.active_instance_list : R.id.inactive_instance_list))
                .inRoot(isDialog())
                .perform(
                        actionOnItemAtPosition(
                                position,
                                new ViewAction() {
                                    @Override
                                    public Matcher<View> getConstraints() {
                                        return null;
                                    }

                                    @Override
                                    public String getDescription() {
                                        return "Click on the close button.";
                                    }

                                    @Override
                                    public void perform(UiController uiController, View view) {
                                        if (isActiveInstance) {
                                            View v = view.findViewById(R.id.more);
                                            v.performClick();

                                        } else {
                                            View v = view.findViewById(R.id.close_button);
                                            v.performClick();
                                        }
                                    }
                                }));

        if (isActiveInstance) {
            onView(withText(R.string.close))
                    .inRoot(withDecorView(withClassName(containsString("Popup"))))
                    .perform(click());
        }

        onView(withText(R.string.instance_switcher_close_confirm_header))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        onView(withText(R.string.close)).perform(click());
        closeCallbackHelper.waitForCallback(closeCallbackCount);
    }

    private static Matcher<View> atPosition(final int position, final Matcher<View> itemMatcher) {
        return new BoundedMatcher<>(RecyclerView.class) {
            @Override
            public void describeTo(Description description) {
                description.appendText("has item at position " + position + ": ");
                itemMatcher.describeTo(description);
            }

            @Override
            protected boolean matchesSafely(final RecyclerView view) {
                RecyclerView.ViewHolder viewHolder =
                        view.findViewHolderForAdapterPosition(position);
                if (viewHolder == null) {
                    // Has no item at this position.
                    return false;
                }
                return itemMatcher.matches(viewHolder.itemView);
            }
        };
    }

    private void clickMoreButtonAtPosition(int instanceIndex, String itemTitle) {
        // Verify content description of the more button on the list item.
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .check(
                        matches(
                                atPosition(
                                        instanceIndex,
                                        hasDescendant(
                                                allOf(
                                                        withId(R.id.more),
                                                        withContentDescription(
                                                                "More options for "
                                                                        + itemTitle))))));
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .perform(
                        actionOnItemAtPosition(
                                instanceIndex,
                                new ViewAction() {
                                    @Override
                                    public Matcher<View> getConstraints() {
                                        return isDisplayed();
                                    }

                                    @Override
                                    public String getDescription() {
                                        return "Click on the more button.";
                                    }

                                    @Override
                                    public void perform(UiController uiController, View view) {
                                        view.findViewById(R.id.more).performClick();
                                    }
                                }));
    }
}
