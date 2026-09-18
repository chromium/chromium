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

import android.view.FocusFinder;
import android.view.View;

import androidx.appcompat.app.AppCompatActivity;
import androidx.test.espresso.Espresso;
import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.HistogramBucket;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
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
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerHost.DismissalCause;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.MetricsHelper.ShownOn;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.bottomsheet.TestBottomSheetContent;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.test.util.AccountCapabilitiesBuilder;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Instrumentation tests for {@link EnterpriseSignalsDisclaimerController}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Testing browser startup prevents batching")
@EnableFeatures(ChromeFeatureList.ANDROID_DEVICE_SIGNALS_DISCLAIMER)
public class EnterpriseSignalsDisclaimerInstrumentationTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock private ManagedBrowserUtils.Natives mManagedBrowserUtilsMock;

    private static final AccountInfo MANAGED_ACCOUNT_2 =
            new AccountInfo.Builder(
                            "test2@example.com",
                            FakeAccountManagerFacade.toGaiaId("test2@example.com"))
                    .fullName("Managed2 Full")
                    .givenName("Managed2 Given")
                    .hostedDomain("example.com")
                    .accountCapabilities(
                            new AccountCapabilitiesBuilder()
                                    .setIsSubjectToEnterpriseFeatures(true)
                                    .build())
                    .build();

    private final HistogramWatcher mStartupHistogramWatcher =
            HistogramWatcher.newBuilder()
                    .expectIntRecord(MetricsHelper.HISTOGRAM_SHOWN_REQUESTED, ShownOn.STARTUP)
                    .expectIntRecord(MetricsHelper.HISTOGRAM_SHOWN, ShownOn.STARTUP)
                    .build();

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
        waitForSignout();
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

    private boolean maybeShow(
            EnterpriseSignalsDisclaimerController controller, @ShownOn int shownOn) {
        return ThreadUtils.runOnUiThreadBlocking(() -> controller.maybeShow(shownOn));
    }

    private EnterpriseSignalsDisclaimerController createControllerAndShowDisclaimer() {
        final EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);
        Assert.assertTrue(maybeShow(controller, ShownOn.STARTUP));
        waitForDisclaimerVisible();
        return controller;
    }

    private void dismissByTappingOutside() {
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity())) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        PropertyModel model =
                                modalDialogManager().getCurrentPresenterForTest().getDialogModel();
                        modalDialogManager()
                                .dismissDialog(
                                        model, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
                    });
        } else {
            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            new BottomSheetTestSupport(bottomSheetController())
                                    .forceClickOutsideTheSheet());
        }
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

    private View getDialogView() {
        final boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity());
        if (isTablet) {
            AppModalPresenter presenter =
                    (AppModalPresenter) modalDialogManager().getCurrentPresenterForTest();
            assert presenter != null;
            View dialogView = presenter.getDialogViewForTesting();
            assert dialogView != null;
            return dialogView;
        } else {
            return activity().getWindow().getDecorView();
        }
    }

    private boolean hasAccountAcknowledgedSignalsDisclaimer(CoreAccountInfo account) {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        EnterpriseSignalsDisclaimerBridge.hasAccountAcknowledgedSignalsDisclaimer(
                                account.getGaiaId()));
    }

    private void acceptDisclaimerForAccount(CoreAccountInfo account) {
        waitForDisclaimerVisible();
        onView(withId(R.id.disclaimer_accept_button)).perform(scrollTo(), click());
        waitForDisclaimerNotShowing();
        Assert.assertTrue(hasAccountAcknowledgedSignalsDisclaimer(account));
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

        mStartupHistogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void disclaimerIsQueuedIfOtherDialogIsShown() {
        final FakeDialog fakeDialog = showFakeDialog();

        Assert.assertTrue(ThreadUtils.runOnUiThreadBlocking(fakeDialog::isShowing));

        final EnterpriseSignalsDisclaimerController controller = createController();
        Assert.assertNotNull(controller);
        Assert.assertTrue(maybeShow(controller, ShownOn.STARTUP));

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

        Assert.assertTrue(maybeShow(controller, ShownOn.STARTUP));

        // Attempt to queue up again, should return false because the first is already in queue.
        Assert.assertFalse(maybeShow(controller, ShownOn.STARTUP));

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
        Assert.assertFalse(hasAccountAcknowledgedSignalsDisclaimer(TestAccounts.MANAGED_ACCOUNT));
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void clickingAcceptHidesDialog() {
        final EnterpriseSignalsDisclaimerController controller =
                createControllerAndShowDisclaimer();

        Assert.assertFalse(hasAccountAcknowledgedSignalsDisclaimer(TestAccounts.MANAGED_ACCOUNT));

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_RESULT, DismissalCause.TAPPED_ACCEPT)
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_IMPLICIT_DISMISSALS_BEFORE_ACCEPTANCE, 0)
                        .expectAnyRecord(MetricsHelper.HISTOGRAM_TIME_TO_USER_ACTION)
                        .build();

        onView(withId(R.id.disclaimer_accept_button)).perform(scrollTo(), click());

        waitForDisclaimerNotShowing();
        Assert.assertNotNull(mSigninTestRule.getPrimaryAccount());
        Assert.assertTrue(hasAccountAcknowledgedSignalsDisclaimer(TestAccounts.MANAGED_ACCOUNT));
        ThreadUtils.runOnUiThreadBlocking(controller::destroy);
        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void clickingSignOutSignsOutAndHidesDialog() {
        final EnterpriseSignalsDisclaimerController controller =
                createControllerAndShowDisclaimer();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_RESULT, DismissalCause.TAPPED_SIGN_OUT)
                        .expectAnyRecord(MetricsHelper.HISTOGRAM_TIME_TO_USER_ACTION)
                        .build();

        onView(withId(R.id.disclaimer_cancel_button)).perform(scrollTo(), click());

        waitForDisclaimerNotShowing();
        waitForSignout();
        Assert.assertFalse(hasAccountAcknowledgedSignalsDisclaimer(TestAccounts.MANAGED_ACCOUNT));
        ThreadUtils.runOnUiThreadBlocking(controller::destroy);
        histogramWatcher.assertExpected();
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

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_RESULT,
                                DismissalCause.DISMISSED_BY_SWIPE_DOWN)
                        .expectAnyRecord(MetricsHelper.HISTOGRAM_TIME_TO_USER_ACTION)
                        .build();

        onView(withId(R.id.disclaimer_scroll_view)).perform(swipeDown());

        waitForDisclaimerNotShowing();
        waitForSignout();
        Assert.assertFalse(hasAccountAcknowledgedSignalsDisclaimer(TestAccounts.MANAGED_ACCOUNT));
        ThreadUtils.runOnUiThreadBlocking(controller::destroy);
        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.PHONE)
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void clickingOutsideSheetSignsOutAndHidesDialog() {
        final EnterpriseSignalsDisclaimerController controller =
                createControllerAndShowDisclaimer();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_RESULT,
                                DismissalCause.DISMISSED_BY_TAP_OUTSIDE)
                        .expectAnyRecord(MetricsHelper.HISTOGRAM_TIME_TO_USER_ACTION)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        new BottomSheetTestSupport(bottomSheetController())
                                .forceClickOutsideTheSheet());

        waitForDisclaimerNotShowing();
        waitForSignout();
        Assert.assertFalse(hasAccountAcknowledgedSignalsDisclaimer(TestAccounts.MANAGED_ACCOUNT));
        ThreadUtils.runOnUiThreadBlocking(controller::destroy);
        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void clickingOutsideModalDialogSignsOutAndHidesDialog() {
        final EnterpriseSignalsDisclaimerController controller =
                createControllerAndShowDisclaimer();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_RESULT,
                                DismissalCause.DISMISSED_BY_TAP_OUTSIDE)
                        .expectAnyRecord(MetricsHelper.HISTOGRAM_TIME_TO_USER_ACTION)
                        .build();

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
        Assert.assertFalse(hasAccountAcknowledgedSignalsDisclaimer(TestAccounts.MANAGED_ACCOUNT));
        ThreadUtils.runOnUiThreadBlocking(controller::destroy);
        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void backPressSignsOutAndHidesDialog() {
        final EnterpriseSignalsDisclaimerController controller =
                createControllerAndShowDisclaimer();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_RESULT,
                                DismissalCause.DISMISSED_BY_BACK_PRESS)
                        .expectAnyRecord(MetricsHelper.HISTOGRAM_TIME_TO_USER_ACTION)
                        .build();

        Espresso.pressBack();

        waitForDisclaimerNotShowing();
        waitForSignout();
        Assert.assertFalse(hasAccountAcknowledgedSignalsDisclaimer(TestAccounts.MANAGED_ACCOUNT));
        ThreadUtils.runOnUiThreadBlocking(controller::destroy);
        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void implicitDismissalsRecordedBeforeAcceptance() {
        final EnterpriseSignalsDisclaimerController controller =
                createControllerAndShowDisclaimer();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_RESULT,
                                DismissalCause.DISMISSED_BY_BACK_PRESS)
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_RESULT,
                                DismissalCause.DISMISSED_BY_TAP_OUTSIDE)
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_RESULT, DismissalCause.TAPPED_ACCEPT)
                        .expectIntRecord(
                                MetricsHelper.HISTOGRAM_IMPLICIT_DISMISSALS_BEFORE_ACCEPTANCE, 2)
                        .expectAnyRecordTimes(MetricsHelper.HISTOGRAM_TIME_TO_USER_ACTION, 3)
                        .build();

        // 1. Implicitly dismiss via back press.
        Espresso.pressBack();
        waitForDisclaimerNotShowing();
        waitForSignout();
        Assert.assertFalse(hasAccountAcknowledgedSignalsDisclaimer(TestAccounts.MANAGED_ACCOUNT));

        // Re-sign in and wait for the disclaimer to show again.
        mSigninTestRule.forceSignOut();
        mSigninTestRule.addAccountThenSignin(TestAccounts.MANAGED_ACCOUNT);
        waitForDisclaimerVisible();

        // 2. Implicitly dismiss via tap outside.
        dismissByTappingOutside();
        waitForDisclaimerNotShowing();
        waitForSignout();
        Assert.assertFalse(hasAccountAcknowledgedSignalsDisclaimer(TestAccounts.MANAGED_ACCOUNT));

        // Re-sign in and wait for the disclaimer to show again.
        mSigninTestRule.forceSignOut();
        mSigninTestRule.addAccountThenSignin(TestAccounts.MANAGED_ACCOUNT);
        waitForDisclaimerVisible();

        // 3. Finally accept the disclaimer.
        onView(withId(R.id.disclaimer_accept_button)).perform(scrollTo(), click());
        waitForDisclaimerNotShowing();

        Assert.assertNotNull(mSigninTestRule.getPrimaryAccount());
        Assert.assertTrue(hasAccountAcknowledgedSignalsDisclaimer(TestAccounts.MANAGED_ACCOUNT));
        ThreadUtils.runOnUiThreadBlocking(controller::destroy);
        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_STARTUP_PROMOS)
    public void disclaimerShownOnSignin() {
        mSigninTestRule.forceSignOut();
        waitForSignout();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(MetricsHelper.HISTOGRAM_SHOWN_REQUESTED, ShownOn.SIGN_IN)
                        .expectIntRecord(MetricsHelper.HISTOGRAM_SHOWN, ShownOn.SIGN_IN)
                        .build();

        mSigninTestRule.addAccountThenSignin(TestAccounts.MANAGED_ACCOUNT);

        waitForDisclaimerVisible();
        histogramWatcher.assertExpected();
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

    @Test
    @LargeTest
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_STARTUP_PROMOS)
    public void timeToUserActionNotRecordedWhileQueued() {
        mSigninTestRule.forceSignOut();
        waitForSignout();

        final FakeDialog fakeDialog = showFakeDialog();
        Assert.assertTrue(ThreadUtils.runOnUiThreadBlocking(fakeDialog::isShowing));

        var shownWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(MetricsHelper.HISTOGRAM_TIME_TO_USER_ACTION)
                        .build();

        mSigninTestRule.addAccountThenSignin(TestAccounts.MANAGED_ACCOUNT);

        // Advance time while the disclaimer is queued behind the fake dialog.
        mFakeTimeTestRule.advanceMillis(5000);

        // Close the blocking dialog so the disclaimer can be dequeued and shown.
        ThreadUtils.runOnUiThreadBlocking(fakeDialog::close);

        waitForDisclaimerVisible();

        // Advance 1000ms while the disclaimer is showing and then accept the disclaimer.
        mFakeTimeTestRule.advanceMillis(1000);
        onView(withId(R.id.disclaimer_accept_button)).perform(scrollTo(), click());
        waitForDisclaimerNotShowing();
        shownWatcher.assertExpected();

        // Verify that the recorded sample does not exceed the 5000ms queued time.
        List<HistogramBucket> buckets =
                RecordHistogram.getHistogramSamplesForTesting(
                        MetricsHelper.HISTOGRAM_TIME_TO_USER_ACTION);
        Assert.assertFalse("Expected histogram records", buckets.isEmpty());
        for (HistogramBucket bucket : buckets) {
            Assert.assertTrue(bucket.mMax <= 5000);
        }
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void focusSearchConfinesFocusToDisclaimer() {
        InstrumentationRegistry.getInstrumentation().setInTouchMode(false);
        final EnterpriseSignalsDisclaimerController controller =
                createControllerAndShowDisclaimer();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View acceptButton = getDialogView().findViewById(R.id.disclaimer_accept_button);
                    Assert.assertNotNull(acceptButton);

                    View scrollView = getDialogView().findViewById(R.id.disclaimer_scroll_view);
                    Assert.assertNotNull(scrollView);
                    EnterpriseSignalsDisclaimerView disclaimerView =
                            (EnterpriseSignalsDisclaimerView) scrollView.getParent();

                    View firstFocusable =
                            FocusFinder.getInstance()
                                    .findNextFocus(disclaimerView, null, View.FOCUS_FORWARD);
                    View lastFocusable =
                            FocusFinder.getInstance()
                                    .findNextFocus(disclaimerView, null, View.FOCUS_BACKWARD);
                    Assert.assertNotNull(firstFocusable);
                    Assert.assertNotNull(lastFocusable);

                    // Forward focus from the last focusable element should wrap to the first
                    // focusable element.
                    View nextAfterLast =
                            disclaimerView.focusSearch(lastFocusable, View.FOCUS_FORWARD);
                    Assert.assertEquals(firstFocusable, nextAfterLast);

                    // Backward focus from the first focusable element should wrap to the last
                    // focusable element.
                    View prevBeforeFirst =
                            disclaimerView.focusSearch(firstFocusable, View.FOCUS_BACKWARD);
                    Assert.assertEquals(lastFocusable, prevBeforeFirst);
                });

        waitForDisclaimerVisible();
        ThreadUtils.runOnUiThreadBlocking(controller::destroy);
    }

    @Test
    @LargeTest
    public void acknowledgmentPersistsAcrossSignouts() {
        acceptDisclaimerForAccount(TestAccounts.MANAGED_ACCOUNT);

        mSigninTestRule.signOut();
        waitForSignout();
        mSigninTestRule.addAccountThenSignin(TestAccounts.MANAGED_ACCOUNT);

        Assert.assertTrue(hasAccountAcknowledgedSignalsDisclaimer(TestAccounts.MANAGED_ACCOUNT));
        waitForDisclaimerNotShowing();
    }

    @Test
    @LargeTest
    public void acknowledgmentIsPerAccount() {
        acceptDisclaimerForAccount(TestAccounts.MANAGED_ACCOUNT);

        mSigninTestRule.signOut();
        waitForSignout();

        mSigninTestRule.addAccountThenSignin(MANAGED_ACCOUNT_2);

        waitForDisclaimerVisible();
        Assert.assertFalse(hasAccountAcknowledgedSignalsDisclaimer(MANAGED_ACCOUNT_2));
        Assert.assertTrue(hasAccountAcknowledgedSignalsDisclaimer(TestAccounts.MANAGED_ACCOUNT));

        mSigninTestRule.forceSignOut();
        waitForSignout();
        mSigninTestRule.removeAccount(MANAGED_ACCOUNT_2.getId());
    }

    @Test
    @LargeTest
    public void syncAcknowledgmentSetAfterRestart() {
        // Accept the disclaimer for the first account.
        acceptDisclaimerForAccount(TestAccounts.MANAGED_ACCOUNT);

        // Switch to the second account.
        mSigninTestRule.signOut();
        waitForDisclaimerNotShowing();
        mSigninTestRule.addAccountThenSignin(MANAGED_ACCOUNT_2);

        // Accept the disclaimer for the second account.
        acceptDisclaimerForAccount(MANAGED_ACCOUNT_2);

        // Simulate a restart, the account is removed before the restart.
        ThreadUtils.runOnUiThreadBlocking(EnterpriseSignalsDisclaimerAckSyncer::resetForTesting);
        ApplicationTestUtils.finishActivity(activity());

        mSigninTestRule.removeAccount(MANAGED_ACCOUNT_2.getId());

        mActivityTestRule.startOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        EnterpriseSignalsDisclaimerAckSyncer.initialize(
                                ProfileManager.getLastUsedRegularProfile()));

        // Verify that the second account is not acknowledged.
        Assert.assertTrue(hasAccountAcknowledgedSignalsDisclaimer(TestAccounts.MANAGED_ACCOUNT));
        Assert.assertFalse(hasAccountAcknowledgedSignalsDisclaimer(MANAGED_ACCOUNT_2));
    }

    @Test
    @LargeTest
    public void syncsAcksWhenAccountIsRemoved() {
        // Accept the disclaimer shown on startup.
        acceptDisclaimerForAccount(TestAccounts.MANAGED_ACCOUNT);

        // Remove the account. The acknowledgment should be removed in result.
        mSigninTestRule.removeAccount(TestAccounts.MANAGED_ACCOUNT.getId());

        CriteriaHelper.pollUiThread(
                () ->
                        Assert.assertFalse(
                                hasAccountAcknowledgedSignalsDisclaimer(
                                        TestAccounts.MANAGED_ACCOUNT)));

        // Add the account again and verify that the disclaimer is shown.
        // forceSignOut still needs to be called, otherwise the next signin will fail.
        mSigninTestRule.forceSignOut();
        mSigninTestRule.addAccountThenSignin(TestAccounts.MANAGED_ACCOUNT);
        waitForDisclaimerVisible();
    }
}
