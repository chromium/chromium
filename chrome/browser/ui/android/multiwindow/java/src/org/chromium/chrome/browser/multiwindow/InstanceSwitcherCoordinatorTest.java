// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.anything;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.DummyUiChromeActivityTestCase;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.util.Arrays;

/**
 *  Unit tests for {@link InstanceSwitcherCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class InstanceSwitcherCoordinatorTest extends DummyUiChromeActivityTestCase {
    private LargeIconBridge mIconBridge;

    private ModalDialogManager mModalDialogManager;
    private ModalDialogManager.Presenter mAppModalPresenter;

    @Before
    public void setUp() throws Exception {
        super.setUpTest();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAppModalPresenter = new AppModalPresenter(getActivity());
            mModalDialogManager = new ModalDialogManager(
                    mAppModalPresenter, ModalDialogManager.ModalDialogType.APP);
        });
        mIconBridge = new LargeIconBridge() {
            @Override
            public boolean getLargeIconForUrl(
                    final GURL pageUrl, int desiredSizePx, final LargeIconCallback callback) {
                return true;
            }
        };
    }

    @Test
    @SmallTest
    public void testOpenWindow() throws Exception {
        InstanceInfo[] instances = new InstanceInfo[] {
                new InstanceInfo(0, 57, InstanceInfo.Type.CURRENT, "url0", "title0", 1, 0, false),
                new InstanceInfo(1, 58, InstanceInfo.Type.OTHER, "ur11", "title1", 2, 0, false),
                new InstanceInfo(2, 59, InstanceInfo.Type.OTHER, "url2", "title2", 1, 1, false)};
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final int itemClickCount = itemClickCallbackHelper.getCallCount();
        Callback<InstanceInfo> openCallback = (item) -> itemClickCallbackHelper.notifyCalled();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            InstanceSwitcherCoordinator.showDialog(getActivity(), mModalDialogManager, mIconBridge,
                    openCallback, null, null, false, Arrays.asList(instances));
        });
        onData(anything()).atPosition(1).perform(click());
        itemClickCallbackHelper.waitForCallback(itemClickCount);
    }

    @Test
    @SmallTest
    public void testNewWindow() throws Exception {
        InstanceInfo[] instances = new InstanceInfo[] {
                new InstanceInfo(0, 57, InstanceInfo.Type.CURRENT, "url0", "title0", 1, 0, false),
                new InstanceInfo(1, 58, InstanceInfo.Type.OTHER, "ur11", "title1", 2, 0, false),
                new InstanceInfo(2, 59, InstanceInfo.Type.OTHER, "url2", "title2", 1, 1, false)};
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final int itemClickCount = itemClickCallbackHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            InstanceSwitcherCoordinator.showDialog(getActivity(), mModalDialogManager, mIconBridge,
                    null, null, itemClickCallbackHelper::notifyCalled, true,
                    Arrays.asList(instances));
        });
        // 0 ~ 2: instances. 3: 'new window' command.
        onData(anything()).atPosition(3).perform(click());
        itemClickCallbackHelper.waitForCallback(itemClickCount);
    }

    @Test
    @SmallTest
    public void testCloseWindow() throws Exception {
        InstanceInfo[] instances = new InstanceInfo[] {
                new InstanceInfo(0, 57, InstanceInfo.Type.CURRENT, "url0", "title0", 1, 0, false),
                new InstanceInfo(1, 58, InstanceInfo.Type.OTHER, "ur11", "title1", 2, 0, false),
                new InstanceInfo(2, 59, InstanceInfo.Type.OTHER, "url2", "title2", 1, 1, false)};
        final CallbackHelper itemClickCallbackHelper = new CallbackHelper();
        final int itemClickCount = itemClickCallbackHelper.getCallCount();
        Callback<InstanceInfo> closeCallback = (item) -> itemClickCallbackHelper.notifyCalled();
        InstanceSwitcherCoordinator coordinator = TestThreadUtils.runOnUiThreadBlocking(() -> {
            InstanceSwitcherCoordinator ic = new InstanceSwitcherCoordinator(
                    getActivity(), mModalDialogManager, mIconBridge, null, closeCallback, null);
            ic.show(Arrays.asList(instances), true);
            return ic;
        });

        // Verify that we have only [cancel] button.
        onView(withId(org.chromium.components.browser_ui.modaldialog.R.id.positive_button))
                .check(matches(withEffectiveVisibility(GONE)));
        onView(withText(R.string.cancel)).check(matches(withEffectiveVisibility(VISIBLE)));

        final int itemIndex = 2; // Index of the instance entry to close
        final int closeWindowMenuIndex = 0; // Index of the item 'Close Window' in 3-dot menu.
        onData(anything()).atPosition(itemIndex).onChildView(withId(R.id.more)).perform(click());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> coordinator.clickMoreMenuItemForTesting(itemIndex, closeWindowMenuIndex));

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
        InstanceInfo[] instances = new InstanceInfo[] {
                new InstanceInfo(0, 57, InstanceInfo.Type.CURRENT, "url0", "title0", 1, 0, false),
                new InstanceInfo(1, 58, InstanceInfo.Type.OTHER, "ur11", "title1", 2, 0, false),
                new InstanceInfo(2, 59, InstanceInfo.Type.OTHER, "url2", "title2", 1, 1, false),
                new InstanceInfo(3, 60, InstanceInfo.Type.OTHER, "url3", "title3", 1, 1, false),
                new InstanceInfo(4, 61, InstanceInfo.Type.OTHER, "url4", "title4", 1, 1, false)};

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            InstanceSwitcherCoordinator.showDialog(getActivity(), mModalDialogManager, mIconBridge,
                    null, null, null, false, Arrays.asList(instances));
        });

        // Verify that we show a info message that users can have up to 5 windows when there are
        // already maximum number of windows.
        onData(anything())
                .atPosition(5)
                .onChildView(withText(R.string.max_number_of_windows))
                .check(matches(isDisplayed()));
    }
}
