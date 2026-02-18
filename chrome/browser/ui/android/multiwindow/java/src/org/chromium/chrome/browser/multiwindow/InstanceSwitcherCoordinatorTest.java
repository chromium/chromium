// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
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
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;

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
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Collections;
import java.util.concurrent.TimeoutException;

/** Unit tests for {@link InstanceSwitcherCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT)
public class InstanceSwitcherCoordinatorTest {
    private static final int MAX_INSTANCE_COUNT = 5;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock InstanceSwitcherActionsDelegate mDelegate;

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
    public void testShowDialog_ListsAreSorted() throws Exception {
        InstanceInfo[] instances =
                new InstanceInfo[] {
                    new InstanceInfo(
                            /* instanceId= */ 0,
                            /* taskId= */ 57,
                            InstanceInfo.Type.OTHER,
                            "url0",
                            "title0",
                            /* customTitle= */ null,
                            /* tabCount= */ 0,
                            /* incognitoTabCount= */ 0,
                            /* isIncognitoSelected= */ false,
                            /* lastAccessedTime= */ getDaysAgoMillis(2),
                            /* closureTime= */ 0),
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
                            /* lastAccessedTime= */ getDaysAgoMillis(1),
                            /* closureTime= */ 0),
                    new InstanceInfo(
                            /* instanceId= */ 2,
                            /* taskId= */ 59,
                            InstanceInfo.Type.CURRENT,
                            "url2",
                            "title2",
                            /* customTitle= */ null,
                            /* tabCount= */ 0,
                            /* incognitoTabCount= */ 0,
                            /* isIncognitoSelected= */ false,
                            /* lastAccessedTime= */ getDaysAgoMillis(0),
                            /* closureTime= */ 0),
                    new InstanceInfo(
                            /* instanceId= */ 3,
                            /* taskId= */ -1,
                            InstanceInfo.Type.OTHER,
                            "url3",
                            "title3",
                            /* customTitle= */ null,
                            /* tabCount= */ 0,
                            /* incognitoTabCount= */ 0,
                            /* isIncognitoSelected= */ false,
                            /* lastAccessedTime= */ getDaysAgoMillis(1),
                            /* closureTime= */ 0),
                    new InstanceInfo(
                            /* instanceId= */ 4,
                            /* taskId= */ -1,
                            InstanceInfo.Type.OTHER,
                            "url4",
                            "title4",
                            /* customTitle= */ null,
                            /* tabCount= */ 0,
                            /* incognitoTabCount= */ 0,
                            /* isIncognitoSelected= */ false,
                            /* lastAccessedTime= */ getDaysAgoMillis(3),
                            /* closureTime= */ getDaysAgoMillis(2))
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
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
                                                        withText("Current window"),
                                                        isDisplayed())))));

        // Verify that the "Yesterday" string is at position 1.
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .check(
                        matches(
                                atPosition(
                                        1,
                                        hasDescendant(
                                                allOf(
                                                        withId(R.id.last_accessed),
                                                        withText("Yesterday"),
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
                                                        withText("2 days ago"),
                                                        isDisplayed())))));

        // Switch to the the inactive instances tab.
        onView(allOf(withText("Inactive (2)"), isDescendantOfA(withId(R.id.tabs))))
                .perform(click());

        // Verify that the "Yesterday" string is at position 0.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .check(
                        matches(
                                atPosition(
                                        0,
                                        hasDescendant(
                                                allOf(
                                                        withId(R.id.last_accessed),
                                                        withText("Yesterday"),
                                                        isDisplayed())))));

        // Verify that the "2 days ago" string is at position 1.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .check(
                        matches(
                                atPosition(
                                        1,
                                        hasDescendant(
                                                allOf(
                                                        withId(R.id.last_accessed),
                                                        withText("2 days ago"),
                                                        isDisplayed())))));
    }

    @Test
    @SmallTest
    public void testOpenWindow() throws Exception {
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 3, /* numInactiveInstances= */ 0);
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final int itemClickCount = itemClickCallbackHelper.getCallCount();
        doAnswer(
                        invocation -> {
                            itemClickCallbackHelper.notifyCalled();
                            return null;
                        })
                .when(mDelegate)
                .openInstance(anyInt());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
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
        verify(mDelegate).openInstance(instances[1].instanceId);
    }

    @Test
    @SmallTest
    public void testRestoreWindow() throws Exception {
        // Initialize instance list with 2 active instances and 1 inactive instance.
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 2, /* numInactiveInstances= */ 1);
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final int itemClickCount = itemClickCallbackHelper.getCallCount();
        doAnswer(
                        invocation -> {
                            itemClickCallbackHelper.notifyCalled();
                            return null;
                        })
                .when(mDelegate)
                .openInstance(anyInt());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
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
        verify(mDelegate).openInstance(instances[2].instanceId);
    }

    @Test
    @SmallTest
    public void testRestoreButtonDisabledWhenSelectedInstanceClosed() throws Exception {
        // Initialize instance list with 1 active instance and 3 inactive instances.
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 1, /* numInactiveInstances= */ 3);
        final CallbackHelper closeCallbackHelper = new CallbackHelper();
        doAnswer(
                        invocation -> {
                            closeCallbackHelper.notifyCalled();
                            return null;
                        })
                .when(mDelegate)
                .closeInstances(any());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
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
        closeInstanceAt(
                0,
                /* isActiveInstance= */ false,
                closeCallbackHelper,
                /* expectCloseConfirmation= */ true);
        verify(mDelegate).closeInstances(Collections.singletonList(instances[1].instanceId));

        // Verify "Restore" button is now disabled.
        onView(allOf(withId(R.id.positive_button), withText(R.string.restore)))
                .inRoot(isDialog())
                .check(matches(not(isEnabled())));
    }

    @Test
    @SmallTest
    public void testRestoreButtonEnabledWhenOtherInstanceClosed() throws Exception {
        // Initialize instance list with 1 active instance and 3 inactive instances.
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 1, /* numInactiveInstances= */ 3);
        final CallbackHelper closeCallbackHelper = new CallbackHelper();
        doAnswer(
                        invocation -> {
                            closeCallbackHelper.notifyCalled();
                            return null;
                        })
                .when(mDelegate)
                .closeInstances(any());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
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
        closeInstanceAt(
                0,
                /* isActiveInstance= */ false,
                closeCallbackHelper,
                /* expectCloseConfirmation= */ true);
        verify(mDelegate).closeInstances(Collections.singletonList(instances[1].instanceId));

        // Verify "Restore" button is still enabled.
        onView(allOf(withId(R.id.positive_button), withText(R.string.restore)))
                .inRoot(isDialog())
                .check(matches(isEnabled()));
    }

    @Test
    @SmallTest
    public void testBlockInstanceRestorationAtLimit() throws Exception {
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
                            mDelegate,
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
    public void testActiveInactiveTabSwitch() throws Exception {
        // Initialize instance list with 2 active instances and 1 inactive instance.
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 2, /* numInactiveInstances= */ 1);
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final int itemClickCount = itemClickCallbackHelper.getCallCount();
        doAnswer(
                        invocation -> {
                            itemClickCallbackHelper.notifyCalled();
                            return null;
                        })
                .when(mDelegate)
                .openInstance(anyInt());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
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
        verify(mDelegate).openInstance(instances[2].instanceId);
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
        doAnswer(
                        invocation -> {
                            itemClickCallbackHelper.notifyCalled();
                            return null;
                        })
                .when(mDelegate)
                .openNewWindow(anyBoolean());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
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
        verify(mDelegate).openNewWindow(isIncognitoWindow);
    }

    @Test
    @SmallTest
    public void testCloseWindow() throws Exception {
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 3, /* numInactiveInstances= */ 0);
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final int itemClickCount = itemClickCallbackHelper.getCallCount();
        doAnswer(
                        invocation -> {
                            itemClickCallbackHelper.notifyCalled();
                            return null;
                        })
                .when(mDelegate)
                .closeInstances(any());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Verify that we have [cancel] and [open] buttons visible.
        onView(allOf(withId(R.id.positive_button), withText(R.string.open)))
                .inRoot(isDialog())
                .check(matches(withEffectiveVisibility(VISIBLE)));
        onView(allOf(withText(R.string.cancel)))
                .inRoot(isDialog())
                .check(matches(withEffectiveVisibility(VISIBLE)));

        clickMoreButtonAtPosition(/* instanceIndex= */ 2, "title2");
        onView(withText(R.string.close))
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .perform(click());

        // Verify that we have [cancel] and [close] buttons visible now.
        onView(withText(R.string.close)).check(matches(withEffectiveVisibility(VISIBLE)));
        onView(withText(R.string.cancel)).check(matches(withEffectiveVisibility(VISIBLE)));

        onView(withText(R.string.close)).perform(click());
        itemClickCallbackHelper.waitForCallback(itemClickCount);
        verify(mDelegate).closeInstances(Collections.singletonList(instances[2].instanceId));
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
        doAnswer(
                        invocation -> {
                            closeCallbackHelper.notifyCalled();
                            return null;
                        })
                .when(mDelegate)
                .closeInstances(any());
        doAnswer(
                        invocation -> {
                            newWindowCallbackHelper.notifyCalled();
                            return null;
                        })
                .when(mDelegate)
                .openNewWindow(anyBoolean());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Verify that we show max info message when there are more than maximum number of windows.
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

        closeInstanceAt(
                0,
                /* isActiveInstance= */ false,
                closeCallbackHelper,
                /* expectCloseConfirmation= */ true);
        verify(mDelegate).closeInstances(Collections.singletonList(instances[5].instanceId));

        // Switch to the active instance tab.
        onView(
                        allOf(
                                withText(String.format("Active (%d)", MAX_INSTANCE_COUNT)),
                                isDescendantOfA(withId(R.id.tabs))))
                .perform(click());

        // Close an active instance (e.g., the third one, at index 2).
        closeInstanceAt(
                2,
                /* isActiveInstance= */ true,
                closeCallbackHelper,
                /* expectCloseConfirmation= */ true);
        verify(mDelegate).closeInstances(Collections.singletonList(instances[2].instanceId));

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
        verify(mDelegate).openNewWindow(false);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT)
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
                            mDelegate,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Verify that we show max info message when there are more than maximum number of windows.
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
    public void testDeselectWindow() throws Exception {
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 3, /* numInactiveInstances= */ 0);
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final int itemClickCount = itemClickCallbackHelper.getCallCount();
        doAnswer(
                        invocation -> {
                            itemClickCallbackHelper.notifyCalled();
                            return null;
                        })
                .when(mDelegate)
                .openInstance(anyInt());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
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
        verify(mDelegate).openInstance(instances[1].instanceId);
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
                            /* lastAccessedTime= */ 3,
                            /* closureTime= */ 0),
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
                            /* lastAccessedTime= */ 2,
                            /* closureTime= */ 0),
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
                            /* lastAccessedTime= */ 1,
                            /* closureTime= */ 0)
                };
        final CallbackHelper closeCallbackHelper = new CallbackHelper();
        doAnswer(
                        invocation -> {
                            closeCallbackHelper.notifyCalled();
                            return null;
                        })
                .when(mDelegate)
                .closeInstances(any());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Closing a hidden, tab-less instance skips the confirmation.
        closeInstanceAt(
                /* position= */ 2,
                /* isActiveInstance= */ true,
                closeCallbackHelper,
                /* expectCloseConfirmation= */ false);
        verify(mDelegate).closeInstances(Collections.singletonList(instances[2].instanceId));

        // Verify that the close callback skips the confirmation when the skip checkbox
        // was ticked on.
        InstanceSwitcherCoordinator.setSkipCloseConfirmation();
        closeInstanceAt(
                /* position= */ 1,
                /* isActiveInstance= */ true,
                closeCallbackHelper,
                /* expectCloseConfirmation= */ false);
        verify(mDelegate).closeInstances(Collections.singletonList(instances[1].instanceId));
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
                            mDelegate,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        clickMoreButtonAtPosition(/* instanceIndex= */ 2, "title2");
        onView(withText(R.string.close))
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .perform(click());
        onView(withText(R.string.instance_switcher_close_confirm_header))
                .inRoot(isDialog())
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
                            mDelegate,
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
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT)
    public void testRenameWindow() throws Exception {
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 3, /* numInactiveInstances= */ 0);
        final CallbackHelper renameCallbackHelper = new CallbackHelper();
        final int renameCallbackCount = renameCallbackHelper.getCallCount();
        doAnswer(
                        invocation -> {
                            renameCallbackHelper.notifyCalled();
                            return null;
                        })
                .when(mDelegate)
                .renameInstance(anyInt(), anyString());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
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
        verify(mDelegate).renameInstance(instances[1].instanceId, newName);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT)
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
                            mDelegate,
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
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT)
    public void testRenameWindowWithEmptyName() throws Exception {
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 3, /* numInactiveInstances= */ 0);
        final CallbackHelper renameCallbackHelper = new CallbackHelper();
        final int renameCallbackCount = renameCallbackHelper.getCallCount();
        doAnswer(
                        invocation -> {
                            renameCallbackHelper.notifyCalled();
                            return null;
                        })
                .when(mDelegate)
                .renameInstance(anyInt(), anyString());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
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
        verify(mDelegate).renameInstance(instances[1].instanceId, "");

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
        ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT,
        ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW
    })
    @Restriction({
        DeviceFormFactor.TABLET_OR_DESKTOP,
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
        DeviceRestriction.RESTRICTION_TYPE_NON_FOLDABLE
    })
    public void testClearIncognitoWindowName() throws Exception {
        InstanceInfo[] instances =
                new InstanceInfo[] {
                    new InstanceInfo(
                            /* instanceId= */ 0,
                            1,
                            InstanceInfo.Type.CURRENT,
                            "url0",
                            "title0",
                            /* customTitle= */ "Window 1",
                            /* tabCount= */ 0,
                            /* incognitoTabCount= */ 1,
                            /* isIncognitoSelected= */ true,
                            /* lastAccessedTime= */ 0,
                            /* closureTime= */ 0)
                };
        final CallbackHelper renameCallbackHelper = new CallbackHelper();
        final int renameCallbackCount = renameCallbackHelper.getCallCount();
        doAnswer(
                        invocation -> {
                            renameCallbackHelper.notifyCalled();
                            return null;
                        })
                .when(mDelegate)
                .renameInstance(anyInt(), anyString());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ true);
                });

        // Click on the 'more' button for the instance.
        clickMoreButtonAtPosition(0, "Window 1");

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

        // Check that the instance title is updated to the default title for an incognito window.
        String defaultTitle = "Incognito window";
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .check(matches(atPosition(0, hasDescendant(withText(defaultTitle)))));

        // Check that the rename callback was called.
        assertEquals(renameCallbackCount + 1, renameCallbackHelper.getCallCount());
        verify(mDelegate).renameInstance(instances[0].instanceId, "");

        // Reopen the name window dialog.
        clickMoreButtonAtPosition(0, defaultTitle);
        onView(withText(R.string.instance_switcher_name_window))
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .check(matches(isDisplayed()))
                .perform(click());

        // Check that the input text field is updated.
        onView(withId(R.id.title_input_text))
                .inRoot(isDialog())
                .check(matches(withText(defaultTitle)));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT)
    public void testCancelRenameWindow() throws Exception {
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 3, /* numInactiveInstances= */ 0);
        final CallbackHelper renameCallbackHelper = new CallbackHelper();
        doAnswer(
                        invocation -> {
                            renameCallbackHelper.notifyCalled();
                            return null;
                        })
                .when(mDelegate)
                .renameInstance(anyInt(), anyString());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
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
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT + ":bulk_close/true")
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
                            mDelegate,
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
                            mDelegate,
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

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT + ":bulk_close/true")
    public void testTitleUpdateOnSelection() throws Exception {
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 3, /* numInactiveInstances= */ 0);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Default title
        onView(
                        allOf(
                                withId(R.id.title),
                                withClassName(containsString("DialogTitle")),
                                isDescendantOfA(withId(R.id.title_container))))
                .inRoot(isDialog())
                .check(matches(withText(R.string.instance_switcher_header)));

        // Select one item
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(0, click()));

        // Verify title for 1 item
        String title1 =
                mActivityTestRule
                        .getActivity()
                        .getResources()
                        .getQuantityString(
                                R.plurals.instance_switcher_windows_selected_header, 1, "1");
        onView(
                        allOf(
                                withId(R.id.title),
                                withClassName(containsString("DialogTitle")),
                                isDescendantOfA(withId(R.id.title_container))))
                .inRoot(isDialog())
                .check(matches(withText(title1)));

        // Select another item
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(1, click()));

        // Verify title for 2 items
        String title2 =
                mActivityTestRule
                        .getActivity()
                        .getResources()
                        .getQuantityString(
                                R.plurals.instance_switcher_windows_selected_header, 2, "2");
        onView(
                        allOf(
                                withId(R.id.title),
                                withClassName(containsString("DialogTitle")),
                                isDescendantOfA(withId(R.id.title_container))))
                .inRoot(isDialog())
                .check(matches(withText(title2)));

        // Deselect one item
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(0, click()));

        // Verify title for 1 item again
        onView(
                        allOf(
                                withId(R.id.title),
                                withClassName(containsString("DialogTitle")),
                                isDescendantOfA(withId(R.id.title_container))))
                .inRoot(isDialog())
                .check(matches(withText(title1)));

        // Deselect the last item
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(1, click()));

        // Verify title is back to default
        onView(
                        allOf(
                                withId(R.id.title),
                                withClassName(containsString("DialogTitle")),
                                isDescendantOfA(withId(R.id.title_container))))
                .inRoot(isDialog())
                .check(matches(withText(R.string.instance_switcher_header)));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT + ":bulk_close/true")
    public void testSelectAll() throws Exception {
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 3, /* numInactiveInstances= */ 0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Select one item to make the more menu visible.
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(0, click()));

        // Click on the 'more' button in the title.
        onView(allOf(withId(R.id.title_more_button), isDisplayed()))
                .inRoot(isDialog())
                .perform(click());

        // Click "Select all".
        onView(withText(R.string.instance_switcher_select_all))
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .perform(click());

        // Verify all items are selected.
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .check(matches(atPosition(0, isSelected())))
                .check(matches(atPosition(1, isSelected())))
                .check(matches(atPosition(2, isSelected())));

        // Verify title is updated.
        String expectedTitle =
                mActivityTestRule
                        .getActivity()
                        .getResources()
                        .getQuantityString(
                                R.plurals.instance_switcher_windows_selected_header, 3, "3");
        onView(
                        allOf(
                                withId(R.id.title),
                                withClassName(containsString("DialogTitle")),
                                isDisplayed()))
                .inRoot(isDialog())
                .check(matches(withText(expectedTitle)));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT + ":bulk_close/true")
    public void testDeselectAll() throws Exception {
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 3, /* numInactiveInstances= */ 0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Select all items.
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(0, click()))
                .perform(actionOnItemAtPosition(1, click()))
                .perform(actionOnItemAtPosition(2, click()));

        // Verify all are selected
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .check(matches(atPosition(0, isSelected())))
                .check(matches(atPosition(1, isSelected())))
                .check(matches(atPosition(2, isSelected())));
        String expectedTitle =
                mActivityTestRule
                        .getActivity()
                        .getResources()
                        .getQuantityString(
                                R.plurals.instance_switcher_windows_selected_header, 3, "3");
        onView(
                        allOf(
                                withId(R.id.title),
                                withClassName(containsString("DialogTitle")),
                                isDisplayed()))
                .inRoot(isDialog())
                .check(matches(withText(expectedTitle)));

        // Click on the 'more' button in the title.
        onView(allOf(withId(R.id.title_more_button), isDisplayed()))
                .inRoot(isDialog())
                .perform(click());

        // Click "Deselect all".
        onView(withText(R.string.instance_switcher_deselect_all))
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .perform(click());

        // Verify all items are deselected.
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .check(matches(atPosition(0, not(isSelected()))))
                .check(matches(atPosition(1, not(isSelected()))))
                .check(matches(atPosition(2, not(isSelected()))));

        // Verify title is updated back to default.
        onView(
                        allOf(
                                withId(R.id.title),
                                withClassName(containsString("DialogTitle")),
                                isDisplayed()))
                .inRoot(isDialog())
                .check(matches(withText(R.string.instance_switcher_header)));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT + ":bulk_close/true")
    public void testCloseSelectedInstances() throws Exception {
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 4, /* numInactiveInstances= */ 0);
        InstanceSwitcherCoordinator.setSkipCloseConfirmation();

        final CallbackHelper closeCallbackHelper = new CallbackHelper();
        int initialCloseCallCount = closeCallbackHelper.getCallCount();
        doAnswer(
                        invocation -> {
                            closeCallbackHelper.notifyCalled();
                            return null;
                        })
                .when(mDelegate)
                .closeInstances(any());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Select items at position 1 and 3.
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(1, click()))
                .perform(actionOnItemAtPosition(3, click()));

        // Click on the 'more' button in the title.
        onView(allOf(withId(R.id.title_more_button), isDisplayed()))
                .inRoot(isDialog())
                .perform(click());

        // Click "Close windows".
        onView(withText(R.string.instance_switcher_close_windows))
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .perform(click());

        // Expect exactly 1 call to closeCallback.
        closeCallbackHelper.waitForCallback(initialCloseCallCount, 1);
        assertEquals(initialCloseCallCount + 1, closeCallbackHelper.getCallCount());

        // After closing 2 instances, there should be 2 left.
        onView(withId(R.id.active_instance_list)).check(matches(withItemCount(2)));

        // Check remaining items are correct
        onView(withId(R.id.active_instance_list))
                .inRoot(isDialog())
                .check(matches(atPosition(0, hasDescendant(withText("title0")))))
                .check(matches(atPosition(1, hasDescendant(withText("title2")))));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT + ":bulk_close/true")
    public void testMoreButtonHiddenWhenListIsEmpty() throws Exception {
        // 1 active, 1 inactive instance.
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ 1, /* numInactiveInstances= */ 1);

        final CallbackHelper closeCallbackHelper = new CallbackHelper();
        doAnswer(
                        invocation -> {
                            closeCallbackHelper.notifyCalled();
                            return null;
                        })
                .when(mDelegate)
                .closeInstances(any());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });
        onView(withId(R.id.active_instance_list)).inRoot(isDialog()).check(matches(isDisplayed()));
        // Switch to inactive list.
        onView(allOf(withText("Inactive (1)"), isDescendantOfA(withId(R.id.tabs))))
                .perform(click());

        // The "more" button should be visible since there's one item.
        onView(allOf(withId(R.id.title_more_button), isDisplayed()))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        // Close the single inactive instance. This will show a confirmation dialog.
        onView(withId(R.id.inactive_instance_list))
                .inRoot(isDialog())
                .perform(
                        actionOnItemAtPosition(
                                0,
                                new ViewAction() {
                                    @Override
                                    public Matcher<View> getConstraints() {
                                        return isDisplayed();
                                    }

                                    @Override
                                    public String getDescription() {
                                        return "Click on the close button of an item.";
                                    }

                                    @Override
                                    public void perform(UiController uiController, View view) {
                                        view.findViewById(R.id.close_button).performClick();
                                    }
                                }));

        // Handle confirmation dialog
        onView(withText(R.string.instance_switcher_close_confirm_header))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withText(R.string.close)).inRoot(isDialog()).perform(click());

        closeCallbackHelper.waitForCallback(0);

        // After closing, the inactive list should be empty.
        onView(withId(R.id.inactive_instance_list)).check(matches(withItemCount(0)));

        // Verify the "No inactive windows" message is displayed.
        onView(withText(R.string.instance_switcher_no_inactive_windows))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        // The "more" button should now be hidden.
        onView(allOf(withId(R.id.title_more_button), isDisplayed())).check(doesNotExist());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT)
    public void testCommandUiAtInstanceLimit_RobustWindowManagement() throws Exception {
        // Simulate persistence of (MAX_INSTANCE_COUNT - 1) active instances and 1 inactive
        // instance.
        InstanceInfo[] instances =
                createPersistedInstances(
                        /* numActiveInstances= */ MAX_INSTANCE_COUNT - 1,
                        /* numInactiveInstances= */ 1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstanceSwitcherCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            mDelegate,
                            MAX_INSTANCE_COUNT,
                            Arrays.asList(instances),
                            /* isIncognitoWindow= */ false);
                });

        // Verify the "+ New window" command is displayed, since # of active instances is less than
        // the instance limit.
        onView(withId(R.id.new_window)).inRoot(isDialog()).check(matches(isDisplayed()));
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
                        /* lastAccessedTime= */ getDaysAgoMillis(0),
                        /* closureTime= */ 0);

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
                            /* closureTime= */ 0);
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
                            /* lastAccessedTime= */ getDaysAgoMillis(i),
                            /* closureTime= */ 3);
        }

        return instances;
    }

    /* Returns the millisecond timestamp for a given number of days in the past. */
    private long getDaysAgoMillis(int lastAccessedDaysAgo) {
        return TimeUtils.currentTimeMillis() - lastAccessedDaysAgo * 24L * 60L * 60L * 1000L;
    }

    private void closeInstanceAt(
            int position,
            boolean isActiveInstance,
            CallbackHelper closeCallbackHelper,
            boolean expectCloseConfirmation)
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

        if (expectCloseConfirmation) {
            onView(withText(R.string.instance_switcher_close_confirm_header))
                    .inRoot(isDialog())
                    .check(matches(isDisplayed()));
            onView(withText(R.string.close)).inRoot(isDialog()).perform(click());
        }

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

    private static Matcher<View> withItemCount(final int count) {
        return new BoundedMatcher<View, RecyclerView>(RecyclerView.class) {
            @Override
            public void describeTo(Description description) {
                description.appendText("has " + count + " items");
            }

            @Override
            protected boolean matchesSafely(RecyclerView recyclerView) {
                return recyclerView.getAdapter().getItemCount() == count;
            }
        };
    }
}
