// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;

import android.view.View;

import androidx.appcompat.app.AppCompatActivity;
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
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
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
import org.chromium.components.browser_ui.bottomsheet.TestBottomSheetContent;
import org.chromium.components.signin.test.util.TestAccounts;

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

    private BottomSheetController bottomSheetController() {
        BottomSheetController instance =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                BottomSheetControllerProvider.from(
                                        mActivityTestRule.getActivity().getWindowAndroid()));
        return instance;
    }

    private void verifyActivityShowingSignalsDisclaimer() {
        onView(withId(R.id.disclaimer_title)).check(matches(isDisplayed()));
        onView(withId(R.id.disclaimer_description)).check(matches(isDisplayed()));

        // On smaller devices the buttons might not fit on the screen.
        onView(withId(R.id.disclaimer_accept_button))
                .perform(scrollTo())
                .check(matches(isDisplayed()));
        onView(withId(R.id.disclaimer_cancel_button))
                .perform(scrollTo())
                .check(matches(isDisplayed()));
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

    @Test
    @LargeTest
    public void disclaimerShowsOnStartup() {
        verifyActivityShowingSignalsDisclaimer();
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void disclaimerIsQueuedIfOtherDialogIsShown() {
        final BottomSheetContent fakeContent = showTestBottomSheetDisclaimer();
        final BottomSheetController bottomSheetController = bottomSheetController();

        Assert.assertEquals(fakeContent, bottomSheetController.getCurrentSheetContent());

        final Profile profile =
                ThreadUtils.runOnUiThreadBlocking(ProfileManager::getLastUsedRegularProfile);
        final EnterpriseSignalsDisclaimerController controller =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                EnterpriseSignalsDisclaimerController.maybeCreateForProfile(
                                        profile, bottomSheetController, activity(), url -> {}));
        Assert.assertNotNull(controller);
        Assert.assertTrue(ThreadUtils.runOnUiThreadBlocking(controller::maybeShow));

        // The existing dialog should still be showing.
        Assert.assertEquals(fakeContent, bottomSheetController.getCurrentSheetContent());

        // Close the currently open dialog, this should cause our dialog to be shown.
        ThreadUtils.runOnUiThreadBlocking(
                () -> bottomSheetController.hideContent(fakeContent, /* animate= */ false));

        verifyActivityShowingSignalsDisclaimer();

        ThreadUtils.runOnUiThreadBlocking(controller::destroy);
    }

    @Test
    @LargeTest
    public void disclaimerCannotBeSuppressed() {
        verifyActivityShowingSignalsDisclaimer();

        final BottomSheetContent fakeContent = showTestBottomSheetDisclaimer();
        Assert.assertNotEquals(fakeContent, bottomSheetController().getCurrentSheetContent());
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void disclaimerCannotBeQueuedTwice() {
        final BottomSheetContent fakeContent = showTestBottomSheetDisclaimer();
        final BottomSheetController bottomSheetController = bottomSheetController();
        Assert.assertEquals(fakeContent, bottomSheetController.getCurrentSheetContent());

        final Profile profile =
                ThreadUtils.runOnUiThreadBlocking(ProfileManager::getLastUsedRegularProfile);
        final EnterpriseSignalsDisclaimerController controller =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                EnterpriseSignalsDisclaimerController.maybeCreateForProfile(
                                        profile, bottomSheetController, activity(), url -> {}));
        Assert.assertNotNull(controller);

        Assert.assertTrue(ThreadUtils.runOnUiThreadBlocking(controller::maybeShow));

        // Attempt to queue up again, should return false because the first is already in queue.
        Assert.assertFalse(ThreadUtils.runOnUiThreadBlocking(controller::maybeShow));

        ThreadUtils.runOnUiThreadBlocking(controller::destroy);
    }
}
