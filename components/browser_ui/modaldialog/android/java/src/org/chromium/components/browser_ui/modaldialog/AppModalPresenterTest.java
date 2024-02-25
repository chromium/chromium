// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;

import static org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils.checkCurrentPresenter;
import static org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils.checkDialogDismissalCause;
import static org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils.checkPendingSize;
import static org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils.createDialog;
import static org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils.createDialogWithDialogStyle;
import static org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils.showDialog;

import android.app.Activity;
import android.graphics.Color;
import android.os.Build;
import android.view.View;
import android.view.Window;
import android.widget.Button;

import androidx.activity.OnBackPressedCallback;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.matcher.BoundedMatcher;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.components.browser_ui.modaldialog.test.R;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

import java.util.concurrent.TimeoutException;

/** Tests for {@link AppModalPresenter}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class AppModalPresenterTest {
    @ClassRule
    public static DisableAnimationsTestRule disableAnimationsRule = new DisableAnimationsTestRule();

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> activityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private class TestObserver implements ModalDialogTestUtils.TestDialogDismissedObserver {
        public final CallbackHelper onDialogDismissedCallback = new CallbackHelper();

        @Override
        public void onDialogDismissed(int dismissalCause) {
            onDialogDismissedCallback.notifyCalled();
            checkDialogDismissalCause(mExpectedDismissalCause, dismissalCause);
        }
    }

    private static Activity sActivity;
    private static ModalDialogManager sManager;
    private TestObserver mTestObserver;
    private Integer mExpectedDismissalCause;

    @BeforeClass
    public static void setupSuite() {
        activityTestRule.launchActivity(null);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity = activityTestRule.getActivity();
                    sManager =
                            new ModalDialogManager(
                                    new AppModalPresenter(sActivity),
                                    ModalDialogManager.ModalDialogType.APP);
                });
    }

    @Before
    public void setupTest() {
        mTestObserver = new TestObserver();
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(sManager::destroy);
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testDismiss_BackPressed() throws Exception {
        PropertyModel dialog1 = createDialog(sActivity, sManager, "1", null);
        PropertyModel dialog2 = createDialog(sActivity, sManager, "2", null);

        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(sManager, ModalDialogType.APP, 0);
        checkCurrentPresenter(sManager, null);

        // Add three dialogs available for showing. The app modal dialog should be shown first.
        showDialog(sManager, dialog1, ModalDialogType.APP);
        showDialog(sManager, dialog2, ModalDialogType.APP);
        checkPendingSize(sManager, ModalDialogType.APP, 1);
        onView(withText("1")).check(matches(isDisplayed()));
        checkCurrentPresenter(sManager, ModalDialogType.APP);

        // Perform back press. The first app modal dialog should be dismissed, and the second one
        // should be shown.
        Espresso.pressBack();
        checkPendingSize(sManager, ModalDialogType.APP, 0);
        onView(withText("1")).check(doesNotExist());
        onView(withText("2")).check(matches(isDisplayed()));
        checkCurrentPresenter(sManager, ModalDialogType.APP);

        // Perform a second back press. The second app modal dialog should be dismissed.
        Espresso.pressBack();
        checkPendingSize(sManager, ModalDialogType.APP, 0);
        onView(withText("2")).check(doesNotExist());
        checkCurrentPresenter(sManager, null);
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testDismiss_DismissalCause_BackPressed() throws Exception {
        PropertyModel dialog = createDialog(sActivity, sManager, "title", mTestObserver);
        mExpectedDismissalCause = DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE;

        showDialog(sManager, dialog, ModalDialogType.APP);

        // Dismiss the tab modal dialog and verify dismissal cause.
        int callCount = mTestObserver.onDialogDismissedCallback.getCallCount();
        Espresso.pressBack();
        mTestObserver.onDialogDismissedCallback.waitForCallback(callCount);

        mExpectedDismissalCause = null;
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testBackPressedCallback_ModalDialogProperty_IsFired() throws TimeoutException {
        PropertyModel dialog = createDialog(sActivity, sManager, "title", null);
        CallbackHelper callbackHelper = new CallbackHelper();
        final OnBackPressedCallback onBackPressedCallback =
                new OnBackPressedCallback(true) {
                    @Override
                    public void handleOnBackPressed() {
                        callbackHelper.notifyCalled();
                    }
                };

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    dialog.set(
                            ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER,
                            onBackPressedCallback);
                });

        showDialog(sManager, dialog, ModalDialogType.APP);

        Espresso.pressBack();
        callbackHelper.waitForCallback(0);
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testButton_negativeButtonFilled() throws Exception {
        PropertyModel dialog =
                createDialog(
                        sActivity,
                        sManager,
                        "title",
                        mTestObserver,
                        ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_FILLED);
        showDialog(sManager, dialog, ModalDialogType.APP);
        onView(withText(R.string.cancel)).check(matches(hasCurrentTextColor(Color.WHITE)));
        onView(withText(R.string.ok)).check(matches(not(hasCurrentTextColor(Color.WHITE))));
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testButton_primaryButtonFilled() throws Exception {
        PropertyModel dialog =
                createDialog(
                        sActivity,
                        sManager,
                        "title",
                        mTestObserver,
                        ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE);
        showDialog(sManager, dialog, ModalDialogType.APP);
        onView(withText(R.string.cancel)).check(matches(not(hasCurrentTextColor(Color.WHITE))));
        onView(withText(R.string.ok)).check(matches(hasCurrentTextColor(Color.WHITE)));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testFullscreenDarkStyle() {
        PropertyModel dialog =
                createDialogWithDialogStyle(
                        sActivity,
                        sManager,
                        "title",
                        mTestObserver,
                        ModalDialogProperties.DialogStyles.FULLSCREEN_DARK_DIALOG);
        showDialog(sManager, dialog, ModalDialogType.APP);
        Window window = ((AppModalPresenter) sManager.getCurrentPresenterForTest()).getWindow();

        assertEquals(
                sActivity.getColor(R.color.toolbar_background_primary_dark),
                window.getStatusBarColor());
        assertEquals(
                sActivity.getColor(R.color.toolbar_background_primary_dark),
                window.getNavigationBarColor());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            assertEquals(
                    sActivity.getColor(R.color.bottom_system_nav_divider_color_light),
                    window.getNavigationBarDividerColor());
        }
    }

    private static Matcher<View> hasCurrentTextColor(int expected) {
        return new BoundedMatcher<View, Button>(Button.class) {
            private int mColor;

            @Override
            public boolean matchesSafely(Button button) {
                mColor = button.getCurrentTextColor();
                return expected == mColor;
            }

            @Override
            public void describeTo(final Description description) {
                description.appendText("Color did not match " + mColor);
            }
        };
    }
}
