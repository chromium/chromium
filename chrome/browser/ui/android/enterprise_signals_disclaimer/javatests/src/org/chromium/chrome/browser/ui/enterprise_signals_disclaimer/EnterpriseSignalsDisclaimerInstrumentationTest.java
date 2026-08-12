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

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtilsJni;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.test.util.TestAccounts;

/** Instrumentation tests for {@link EnterpriseSignalsDisclaimerController}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
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
        ManagedBrowserUtilsJni.setInstanceForTesting(null);
    }

    @Test
    @LargeTest
    public void disclaimerShowsOnStartup() {
        onView(withId(R.id.disclaimer_title)).check(matches(isDisplayed()));
        onView(withId(R.id.disclaimer_description)).check(matches(isDisplayed()));

        // On smaller devices the buttons might not fit on the screen.
        onView(withId(R.id.disclaimer_accept_button))
                .perform(scrollTo())
                .check(matches(isDisplayed()));
        onView(withId(R.id.disclaimer_cancel_button))
                .perform(scrollTo())
                .check(matches(isDisplayed()));

        // Closes the activity and waits for destruction. This is called to cover
        // TabbedRootUiCoordinator destroying EnterpriseSignalsDisclaimerController.
        ApplicationTestUtils.finishActivity(mActivityTestRule.getActivity());
    }
}
