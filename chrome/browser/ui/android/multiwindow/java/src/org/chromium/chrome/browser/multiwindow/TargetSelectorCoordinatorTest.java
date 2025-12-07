// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.contrib.RecyclerViewActions.actionOnItemAtPosition;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.anything;
import static org.hamcrest.Matchers.not;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.url.GURL;

import java.util.Arrays;

/** Unit tests for {@link TargetSelectorCoordinatorTest}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TargetSelectorCoordinatorTest {
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

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.INSTANCE_SWITCHER_V2)
    public void testTargetSelectorCoordinatorTest_moveWindow() throws Exception {
        InstanceInfo[] instances =
                new InstanceInfo[] {
                    new InstanceInfo(
                            /* instanceId= */ 0,
                            /* taskId= */ 57,
                            InstanceInfo.Type.CURRENT,
                            "url0",
                            "title0",
                            /* customTitle= */ null,
                            /* tabCount= */ 1,
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
                            /* tabCount= */ 1,
                            /* incognitoTabCount= */ 1,
                            /* isIncognitoSelected= */ false,
                            /* lastAccessedTime= */ 0,
                            /* closedByUser= */ false)
                };
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final int itemClickCount = itemClickCallbackHelper.getCallCount();
        Callback<InstanceInfo> moveCallback = (item) -> itemClickCallbackHelper.notifyCalled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TargetSelectorCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            moveCallback,
                            Arrays.asList(instances),
                            R.string.menu_move_to_other_window);
                });

        // Choose a target window.
        onData(anything()).inRoot(isDialog()).atPosition(1).perform(click());

        // Click 'move tab'.
        String moveTab =
                mActivityTestRule
                        .getActivity()
                        .getResources()
                        .getString(R.string.target_selector_move);
        onView(withText(moveTab)).perform(click());
        itemClickCallbackHelper.waitForCallback(itemClickCount);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.INSTANCE_SWITCHER_V2)
    public void testTargetSelectorCoordinatorTest_moveWindow_V2() throws Exception {
        InstanceInfo[] instances =
                new InstanceInfo[] {
                    new InstanceInfo(
                            /* instanceId= */ 0,
                            /* taskId= */ 57,
                            InstanceInfo.Type.CURRENT,
                            "url0",
                            "title0",
                            /* customTitle= */ null,
                            /* tabCount= */ 1,
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
                            /* tabCount= */ 1,
                            /* incognitoTabCount= */ 1,
                            /* isIncognitoSelected= */ false,
                            /* lastAccessedTime= */ 0,
                            /* closedByUser= */ false)
                };
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final int itemClickCount = itemClickCallbackHelper.getCallCount();
        Callback<InstanceInfo> moveCallback = (item) -> itemClickCallbackHelper.notifyCalled();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TargetSelectorCoordinator.showDialog(
                            mActivityTestRule.getActivity(),
                            mModalDialogManager,
                            mIconBridge,
                            moveCallback,
                            Arrays.asList(instances),
                            R.string.menu_move_tab_to_other_window);
                });

        // Verify "Move" button is disabled before a selection is made.
        onView(withText(R.string.move)).inRoot(isDialog()).check(matches(not(isEnabled())));

        // Select the first item.
        onView(withId(R.id.targets_list))
                .inRoot(isDialog())
                .perform(actionOnItemAtPosition(0, click()));

        // Click 'move'.
        String moveTab = mActivityTestRule.getActivity().getResources().getString(R.string.move);
        onView(withText(moveTab)).perform(click());
        itemClickCallbackHelper.waitForCallback(itemClickCount);
    }
}
