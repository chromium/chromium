// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressBack;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.os.IBinder;
import android.view.WindowManager;

import androidx.test.espresso.Root;
import androidx.test.filters.MediumTest;

import org.hamcrest.Description;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.function.Predicate;

/** Instrumentation tests for {@link ConfirmImportSyncDataDialogCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ConfirmImportSyncDataDialogTest {

    private static class ToastMatcher extends TypeSafeMatcher<Root> {
        @Override
        public void describeTo(Description description) {
            description.appendText("For Managed Domain");
        }

        @Override
        public boolean matchesSafely(Root root) {
            int type = root.getWindowLayoutParams().get().type;
            if ((type == WindowManager.LayoutParams.TYPE_TOAST)) {
                IBinder windowToken = root.getDecorView().getWindowToken();
                IBinder appToken = root.getDecorView().getApplicationWindowToken();
                if (windowToken == appToken) {
                    return true;
                }
            }
            return false;
        }
    }

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @ClassRule
    public static final BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock private ConfirmImportSyncDataDialogCoordinator.Listener mListenerMock;

    private ModalDialogManager mDialogManager;
    private ConfirmImportSyncDataDialogCoordinator mDialogCoordinator;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialogManager =
                            new ModalDialogManager(
                                    new AppModalPresenter(sActivityTestRule.getActivity()),
                                    ModalDialogType.APP);
                });
    }

    @Test
    @MediumTest
    public void testPositiveButtonWhenAccountIsManaged() {
        showConfirmImportSyncDataDialog(true);
        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        verify(mListenerMock).onConfirm(true);
        verify(mListenerMock, never()).onCancel();
    }

    @Test
    @MediumTest
    public void testPositiveButtonWhenAccountIsNotManaged() {
        showConfirmImportSyncDataDialog(false);
        onView(withId(R.id.sync_confirm_import_choice)).inRoot(isDialog()).perform(click());
        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        verify(mListenerMock).onConfirm(false);
        verify(mListenerMock, never()).onCancel();
    }

    @Test
    @MediumTest
    public void testNegativeButton() {
        showConfirmImportSyncDataDialog(false);
        onView(withText(R.string.cancel)).inRoot(isDialog()).perform(click());
        verify(mListenerMock, never()).onConfirm(anyBoolean());
        verify(mListenerMock).onCancel();
    }

    @Test
    @MediumTest
    public void testListenerOnCancelNotCalledWhenDialogDismissedInternally() {
        showConfirmImportSyncDataDialog(false);
        ThreadUtils.runOnUiThreadBlocking(mDialogCoordinator::dismissDialog);
        verify(mListenerMock, never()).onCancel();
    }

    @Test
    @MediumTest
    public void testListenerOnCancelCalledWhenDialogDismissedByUser() {
        showConfirmImportSyncDataDialog(false);
        onView(isRoot()).inRoot(isDialog()).perform(pressBack());
        verify(mListenerMock).onCancel();
    }

    @Test
    @MediumTest
    public void testToastOfConfirmImportOptionForManagedAccount() {
        showConfirmImportSyncDataDialog(true);
        onView(withId(R.id.sync_confirm_import_choice)).inRoot(isDialog()).perform(click());
        onView(withText(R.string.managed_by_your_organization))
                .inRoot(new ToastMatcher())
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testForNonDisplayableAccountEmail_noSplitStoresAndUPMForLocal() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialogCoordinator =
                            new ConfirmImportSyncDataDialogCoordinator(
                                    sActivityTestRule.getActivity(),
                                    mDialogManager,
                                    mListenerMock,
                                    "old.testaccount@gmail.com",
                                    "new.testaccount@gmail.com",
                                    /* checkIfDisplayableEmailAddress= */ email -> false,
                                    /* isCurrentAccountManaged= */ false,
                                    /* usesSplitStoresAndUPMForLocal= */ false);
                });

        final Activity activity = sActivityTestRule.getActivity();
        final String defaultAccountName =
                activity.getString(R.string.default_google_account_username);
        final String expectedString =
                activity.getString(R.string.sync_import_data_prompt, defaultAccountName);
        final String unexpectedString =
                activity.getString(R.string.sync_import_data_prompt, "old.testaccount@gmail.com");
        onView(withText(expectedString)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withText(unexpectedString)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testForNonDisplayableAccountEmail_usesSplitStoresAndUPMForLocal() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialogCoordinator =
                            new ConfirmImportSyncDataDialogCoordinator(
                                    sActivityTestRule.getActivity(),
                                    mDialogManager,
                                    mListenerMock,
                                    "old.testaccount@gmail.com",
                                    "new.testaccount@gmail.com",
                                    /* checkIfDisplayableEmailAddress= */ email -> false,
                                    /* isCurrentAccountManaged= */ false,
                                    /* usesSplitStoresAndUPMForLocal= */ true);
                });

        final Activity activity = sActivityTestRule.getActivity();
        final String defaultAccountName =
                activity.getString(R.string.default_google_account_username);
        final String expectedString =
                activity.getString(
                        R.string.sync_import_data_prompt_without_passwords, defaultAccountName);
        final String unexpectedString =
                activity.getString(
                        R.string.sync_import_data_prompt_without_passwords,
                        "old.testaccount@gmail.com");
        onView(withText(expectedString)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withText(unexpectedString)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testTextForNonManagedAccount_noSplitStoresAndUPMForLocal() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialogCoordinator =
                            new ConfirmImportSyncDataDialogCoordinator(
                                    sActivityTestRule.getActivity(),
                                    mDialogManager,
                                    mListenerMock,
                                    "old.testaccount@gmail.com",
                                    "new.testaccount@gmail.com",
                                    /* isCurrentAccountManaged= */ false,
                                    /* usesSplitStoresAndUPMForLocal= */ false);
                });

        String expectedText =
                sActivityTestRule
                        .getActivity()
                        .getString(R.string.sync_import_data_prompt, "old.testaccount@gmail.com");
        onView(withText(expectedText)).inRoot(isDialog()).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testTextForNonManagedAccount_withSplitStoresAndUPMForLocal() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialogCoordinator =
                            new ConfirmImportSyncDataDialogCoordinator(
                                    sActivityTestRule.getActivity(),
                                    mDialogManager,
                                    mListenerMock,
                                    "old.testaccount@gmail.com",
                                    "new.testaccount@gmail.com",
                                    /* isCurrentAccountManaged= */ false,
                                    /* usesSplitStoresAndUPMForLocal= */ true);
                });

        String expectedText =
                sActivityTestRule
                        .getActivity()
                        .getString(
                                R.string.sync_import_data_prompt_without_passwords,
                                "old.testaccount@gmail.com");
        onView(withText(expectedText)).inRoot(isDialog()).check(matches(isDisplayed()));
    }

    private void showConfirmImportSyncDataDialog(boolean isCurrentAccountManaged) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialogCoordinator =
                            new ConfirmImportSyncDataDialogCoordinator(
                                    sActivityTestRule.getActivity(),
                                    mDialogManager,
                                    mListenerMock,
                                    "old.testaccount@gmail.com",
                                    "new.testaccount@gmail.com",
                                    isCurrentAccountManaged,
                                    /* usesSplitStoresAndUPMForLocal= */ false);
                });
    }

    private void showConfirmImportSyncDataDialog(Predicate<String> checkIfDisplayableEmailAddress) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialogCoordinator =
                            new ConfirmImportSyncDataDialogCoordinator(
                                    sActivityTestRule.getActivity(),
                                    mDialogManager,
                                    mListenerMock,
                                    "old.testaccount@gmail.com",
                                    "new.testaccount@gmail.com",
                                    checkIfDisplayableEmailAddress,
                                    /* isCurrentAccountManaged= */ false,
                                    /* usesSplitStoresAndUPMForLocal= */ false);
                });
    }
}
