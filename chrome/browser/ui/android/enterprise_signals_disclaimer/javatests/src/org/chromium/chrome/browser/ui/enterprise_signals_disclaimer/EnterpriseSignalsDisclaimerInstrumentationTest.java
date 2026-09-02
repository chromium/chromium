// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.action.ViewActions.swipeDown;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;

import static org.chromium.ui.test.util.ViewUtils.VIEW_GONE;
import static org.chromium.ui.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.ui.test.util.ViewUtils.waitForVisibleView;
import static org.chromium.ui.test.util.ViewUtils.withEventualExpectedViewState;

import android.view.View;

import androidx.appcompat.app.AppCompatActivity;
import androidx.test.espresso.Espresso;
import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtilsJni;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.bottomsheet.TestBottomSheetContent;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Instrumentation tests for {@link EnterpriseSignalsDisclaimerController}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Testing browser startup prevents batching")
@EnableFeatures(ChromeFeatureList.ANDROID_DEVICE_SIGNALS_DISCLAIMER)
public class EnterpriseSignalsDisclaimerInstrumentationTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Mock private ManagedBrowserUtils.Natives mManagedBrowserUtilsMock;

    @Before
    public void setUp() {
        mManagedBrowserUtilsMock = Mockito.mock(ManagedBrowserUtils.Natives.class);
        ManagedBrowserUtilsJni.setInstanceForTesting(mManagedBrowserUtilsMock);
        doReturn(true).when(mManagedBrowserUtilsMock).isProfileManaged(any());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
                    FirstRunStatus.setFirstRunFlowComplete(true);
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    IdentityServicesProvider.get()
                            .getSigninManager(profile)
                            .setUserAcceptedAccountManagement(true);
                });
        mSigninTestRule.addAccountThenSignin(TestAccounts.MANAGED_ACCOUNT);

        mActivityTestRule.startOnBlankPage();
    }

    @After
    public void tearDown() {
        mManagedBrowserUtilsMock = null;
        ManagedBrowserUtilsJni.setInstanceForTesting(null);
        mSigninTestRule.forceSignOut();
        mSigninTestRule.removeAccount(TestAccounts.MANAGED_ACCOUNT.getId());
    }

    private AppCompatActivity activity() {
        return mActivityTestRule.getActivity();
    }

    private ModalDialogManager modalDialogManager() {
        return mActivityTestRule.getActivity().getModalDialogManager();
    }

    private BottomSheetController bottomSheetController() {
        BottomSheetController instance =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                BottomSheetControllerProvider.from(
                                        mActivityTestRule.getActivity().getWindowAndroid()));
        return instance;
    }

    private void waitForDisclaimerVisible() {
        waitForVisibleView(withId(R.id.disclaimer_scroll_view));

        onView(withId(R.id.disclaimer_title)).perform(scrollTo()).check(matches(isDisplayed()));
        onView(withId(R.id.disclaimer_description))
                .perform(scrollTo())
                .check(matches(isDisplayed()));
        onView(withId(R.id.disclaimer_accept_button))
                .perform(scrollTo())
                .check(matches(isDisplayed()));
        onView(withId(R.id.disclaimer_cancel_button))
                .perform(scrollTo())
                .check(matches(isDisplayed()));
    }

    private void waitForDisclaimerNotShowing() {
        onView(isRoot())
                .check(
                        withEventualExpectedViewState(
                                withId(R.id.disclaimer_scroll_view), VIEW_GONE | VIEW_NULL));
    }

    private EnterpriseSignalsDisclaimerController createController() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final var profile = ProfileManager.getLastUsedRegularProfile();
                    return new EnterpriseSignalsDisclaimerController(
                            IdentityServicesProvider.get().getSigninManager(profile),
                            bottomSheetController(),
                            modalDialogManager(),
                            activity(),
                            profile,
                            url -> {},
                            EnterpriseSignalsDisclaimerCoordinator::new);
                });
    }

    private EnterpriseSignalsDisclaimerController createControllerAndShowDisclaimer() {
        final EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);
        Assert.assertTrue(ThreadUtils.runOnUiThreadBlocking(controller::maybeShow));
        waitForDisclaimerVisible();
        return controller;
    }

    /** Abstraction combining fake bottom sheet and modal dialogs. */
    private interface FakeDialog {
        boolean isShowing();

        void close();
    }

    private BottomSheetContent showTestBottomSheetDisclaimer() {
        TestBottomSheetContent fakeContent =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            View view = new View(activity());
                            view.setMinimumHeight(200);
                            TestBottomSheetContent content =
                                    new TestBottomSheetContent(
                                            activity(),
                                            BottomSheetContent.ContentPriority.HIGH,
                                            false,
                                            view);
                            content.setCanBeSuppressed(false);
                            return content;
                        });

        final BottomSheetController bottomSheetController = bottomSheetController();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    bottomSheetController.requestShowContent(fakeContent, /* animate= */ false);
                });
        return fakeContent;
    }

    private PropertyModel showTestModalDialog() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View view = new View(activity());
                    view.setMinimumHeight(200);
                    PropertyModel model =
                            new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                                    .with(ModalDialogProperties.CUSTOM_VIEW, view)
                                    .with(
                                            ModalDialogProperties.CONTROLLER,
                                            new ModalDialogProperties.Controller() {
                                                @Override
                                                public void onClick(
                                                        PropertyModel model, int buttonType) {}

                                                @Override
                                                public void onDismiss(
                                                        PropertyModel model, int dismissalCause) {}
                                            })
                                    .build();
                    modalDialogManager()
                            .showDialog(
                                    model,
                                    ModalDialogManager.ModalDialogType.APP,
                                    ModalDialogManager.ModalDialogPriority.HIGH);
                    return model;
                });
    }

    private FakeDialog showFakeDialog() {
        final boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity());
        if (isTablet) {
            final PropertyModel model = showTestModalDialog();
            return new FakeDialog() {
                @Override
                public boolean isShowing() {
                    return modalDialogManager().isShowing()
                            && modalDialogManager().getCurrentPresenterForTest() != null
                            && modalDialogManager().getCurrentPresenterForTest().getDialogModel()
                                    == model;
                }

                @Override
                public void close() {
                    modalDialogManager()
                            .dismissDialog(model, DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
                }
            };
        } else {
            final BottomSheetContent content = showTestBottomSheetDisclaimer();
            return new FakeDialog() {
                @Override
                public boolean isShowing() {
                    return content == bottomSheetController().getCurrentSheetContent();
                }

                @Override
                public void close() {
                    bottomSheetController().hideContent(content, /* animate= */ false);
                }
            };
        }
    }

    private void waitForSignout() {
        CriteriaHelper.pollUiThread(() -> Assert.assertNull(mSigninTestRule.getPrimaryAccount()));
    }

    @Test
    @LargeTest
    public void disclaimerShowsOnStartup() {
        waitForDisclaimerVisible();
        final boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity());
        // Verify that on phones the bottom sheet is used, while on large form factor the modal
        // dialog is used.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(!isTablet, bottomSheetController().isSheetOpen());
                    Assert.assertEquals(isTablet, modalDialogManager().isShowing());
                });
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void disclaimerIsQueuedIfOtherDialogIsShown() {
        final FakeDialog fakeDialog = showFakeDialog();

        Assert.assertTrue(ThreadUtils.runOnUiThreadBlocking(fakeDialog::isShowing));

        final EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);
        Assert.assertTrue(ThreadUtils.runOnUiThreadBlocking(controller::maybeShow));

        // The existing dialog should still be showing.
        Assert.assertTrue(ThreadUtils.runOnUiThreadBlocking(fakeDialog::isShowing));

        // Close the currently open dialog, this should cause our disclaimer dialog to be shown.
        ThreadUtils.runOnUiThreadBlocking(fakeDialog::close);

        waitForDisclaimerVisible();

        ThreadUtils.runOnUiThreadBlocking(controller::destroy);
    }

    @Test
    @LargeTest
    public void disclaimerCannotBeSuppressed() {
        waitForDisclaimerVisible();

        final FakeDialog fakeDialog = showFakeDialog();
        Assert.assertFalse(ThreadUtils.runOnUiThreadBlocking(fakeDialog::isShowing));

        waitForDisclaimerVisible();
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void disclaimerCannotBeQueuedTwice() {
        final FakeDialog fakeDialog = showFakeDialog();
        Assert.assertTrue(ThreadUtils.runOnUiThreadBlocking(fakeDialog::isShowing));

        final EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);

        Assert.assertTrue(ThreadUtils.runOnUiThreadBlocking(controller::maybeShow));

        // Attempt to queue up again, should return false because the first is already in queue.
        Assert.assertFalse(ThreadUtils.runOnUiThreadBlocking(controller::maybeShow));

        ThreadUtils.runOnUiThreadBlocking(controller::destroy);
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void destroyingControllerHidesDialog() {
        final EnterpriseSignalsDisclaimerController controller =
                createControllerAndShowDisclaimer();

        // Destroy the controller and verify that the dialog is not being shown anymore.
        ThreadUtils.runOnUiThreadBlocking(controller::destroy);
        waitForDisclaimerNotShowing();

        Assert.assertNotNull(mSigninTestRule.getPrimaryAccount());
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void clickingAcceptHidesDialog() {
        final EnterpriseSignalsDisclaimerController controller =
                createControllerAndShowDisclaimer();

        onView(withId(R.id.disclaimer_accept_button)).perform(scrollTo(), click());

        waitForDisclaimerNotShowing();
        Assert.assertNotNull(mSigninTestRule.getPrimaryAccount());
        ThreadUtils.runOnUiThreadBlocking(controller::destroy);
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void clickingSignOutSignsOutAndHidesDialog() {
        final EnterpriseSignalsDisclaimerController controller =
                createControllerAndShowDisclaimer();

        onView(withId(R.id.disclaimer_cancel_button)).perform(scrollTo(), click());

        waitForDisclaimerNotShowing();
        waitForSignout();
        ThreadUtils.runOnUiThreadBlocking(controller::destroy);
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.PHONE)
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void swipingBottomSheetSignsOutAndHidesDialog() {
        final EnterpriseSignalsDisclaimerController controller =
                createControllerAndShowDisclaimer();

        // Scroll to the top of the disclaimer so the swipe does not scroll instead of closing the
        // dialog.
        ThreadUtils.runOnUiThreadBlocking(
                () -> activity().findViewById(R.id.disclaimer_scroll_view).scrollTo(0, 0));
        onView(withId(R.id.disclaimer_scroll_view)).perform(swipeDown());

        waitForDisclaimerNotShowing();
        waitForSignout();
        ThreadUtils.runOnUiThreadBlocking(controller::destroy);
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.PHONE)
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void clickingOutsideSheetSignsOutAndHidesDialog() {
        final EnterpriseSignalsDisclaimerController controller =
                createControllerAndShowDisclaimer();

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        new BottomSheetTestSupport(bottomSheetController())
                                .forceClickOutsideTheSheet());

        waitForDisclaimerNotShowing();
        waitForSignout();
        ThreadUtils.runOnUiThreadBlocking(controller::destroy);
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void clickingOutsideModalDialogSignsOutAndHidesDialog() {
        final EnterpriseSignalsDisclaimerController controller =
                createControllerAndShowDisclaimer();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel model =
                            modalDialogManager().getCurrentPresenterForTest().getDialogModel();
                    modalDialogManager()
                            .dismissDialog(
                                    model, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
                });

        waitForDisclaimerNotShowing();
        waitForSignout();
        ThreadUtils.runOnUiThreadBlocking(controller::destroy);
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void backPressSignsOutAndHidesDialog() {
        final EnterpriseSignalsDisclaimerController controller =
                createControllerAndShowDisclaimer();

        Espresso.pressBack();

        waitForDisclaimerNotShowing();
        waitForSignout();
        ThreadUtils.runOnUiThreadBlocking(controller::destroy);
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_STARTUP_PROMOS)
    public void disclaimerShownOnSignin() {
        mSigninTestRule.forceSignOut();
        waitForSignout();

        mSigninTestRule.addAccountThenSignin(TestAccounts.MANAGED_ACCOUNT);

        waitForDisclaimerVisible();
    }

    @Test
    @LargeTest
    public void signoutHidesDisclaimer() {
        waitForDisclaimerVisible();

        mSigninTestRule.signOut();
        waitForSignout();

        waitForDisclaimerNotShowing();
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_STARTUP_PROMOS)
    public void anotherDialogShownOnSignin() {
        mSigninTestRule.forceSignOut();
        waitForSignout();

        final FakeDialog fakeDialog = showFakeDialog();
        Assert.assertTrue(ThreadUtils.runOnUiThreadBlocking(fakeDialog::isShowing));
        mSigninTestRule.addAccountThenSignin(TestAccounts.MANAGED_ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(fakeDialog::close);

        waitForDisclaimerVisible();
    }
}
