// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import static androidx.core.view.WindowInsetsCompat.Type.systemBars;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils.checkCurrentPresenter;
import static org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils.checkDialogDismissalCause;
import static org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils.checkPendingSize;
import static org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils.createDialog;
import static org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils.createDialogWithDialogStyle;
import static org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils.showDialogInRoot;

import android.app.Activity;
import android.graphics.Color;
import android.os.Build;
import android.view.View;
import android.view.Window;
import android.widget.Button;

import androidx.activity.OnBackPressedCallback;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;
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

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.components.browser_ui.modaldialog.test.R;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.TimeoutException;

/** Tests for {@link AppModalPresenter}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class AppModalPresenterTest {
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
    private static InsetObserver sInsetObserver;
    private TestObserver mTestObserver;
    private Integer mExpectedDismissalCause;

    @BeforeClass
    public static void setupSuite() {
        activityTestRule.launchActivity(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity = activityTestRule.getActivity();
                    sManager =
                            new ModalDialogManager(
                                    new AppModalPresenter(sActivity),
                                    ModalDialogManager.ModalDialogType.APP);
                    sInsetObserver =
                            new InsetObserver(
                                    new ImmutableWeakReference<>(
                                            sActivity.getWindow().getDecorView().getRootView()));
                    sManager.setInsetObserver(sInsetObserver);
                });
    }

    @Before
    public void setupTest() {
        mTestObserver = new TestObserver();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(sManager::destroy);
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
        showDialogInRoot(sManager, dialog1, ModalDialogType.APP);
        showDialogInRoot(sManager, dialog2, ModalDialogType.APP);
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

        showDialogInRoot(sManager, dialog, ModalDialogType.APP);

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

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    dialog.set(
                            ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER,
                            onBackPressedCallback);
                });

        showDialogInRoot(sManager, dialog, ModalDialogType.APP);

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
        showDialogInRoot(sManager, dialog, ModalDialogType.APP);
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
        showDialogInRoot(sManager, dialog, ModalDialogType.APP);
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
        showDialogInRoot(sManager, dialog, ModalDialogType.APP);
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

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testDialogDimensionsWithNonZeroSystemBarsInsets() {
        doTestDialogDimensions(
                /* leftInset= */ 50,
                /* topInset= */ 80,
                /* rightInset= */ 40,
                /* bottomInset= */ 64);
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testDialogDimensionsWithNoSystemBarsInsets() {
        doTestDialogDimensions(
                /* leftInset= */ 0, /* topInset= */ 0, /* rightInset= */ 0, /* bottomInset= */ 0);
    }

    private void doTestDialogDimensions(
            int leftInset, int topInset, int rightInset, int bottomInset) {
        ModalDialogFeatureMap.setModalDialogLayoutWithSystemInsetsEnabledForTesting(true);
        PropertyModel dialog =
                createDialog(
                        sActivity,
                        sManager,
                        "title",
                        mTestObserver,
                        ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE);

        var displayMetrics = sActivity.getResources().getDisplayMetrics();
        var windowWidth = displayMetrics.widthPixels;
        var windowHeight = displayMetrics.heightPixels;

        // Set a minimum height / width for the dialog view so that it is considered large with
        // respect to the window size.
        var customView = new View(sActivity);
        customView.setMinimumHeight(windowHeight - 20);
        customView.setMinimumWidth(windowWidth - 20);
        ThreadUtils.runOnUiThreadBlocking(
                () -> dialog.set(ModalDialogProperties.CUSTOM_VIEW, customView));

        // Apply window insets before dialog is shown.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var windowInsets =
                            new WindowInsetsCompat.Builder()
                                    .setInsets(
                                            systemBars(),
                                            Insets.of(leftInset, topInset, rightInset, bottomInset))
                                    .build();
                    sInsetObserver.onApplyWindowInsets(
                            sActivity.getWindow().getDecorView().getRootView(), windowInsets);
                });
        showDialogInRoot(sManager, dialog, ModalDialogType.APP);

        // Verify dialog edges don't draw into insets' regions.
        var view =
                ((AppModalPresenter) sManager.getCurrentPresenterForTest())
                        .getDialogViewForTesting();
        assertTrue(
                "View is wider than expected.",
                view.getWidth() <= (windowWidth - 2 * Math.max(rightInset, leftInset)));
        assertTrue(
                "View is taller than expected.",
                view.getHeight() <= (windowHeight - 2 * Math.max(topInset, bottomInset)));

        ModalDialogFeatureMap.setModalDialogLayoutWithSystemInsetsEnabledForTesting(false);
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
