// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_BACK_CLICK_HANDLER;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_MODEL_LIST;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_SETTINGS_CLICK_HANDLER;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.PROFILE_MODEL_LIST;

import android.view.LayoutInflater;
import android.view.View;

import androidx.appcompat.widget.Toolbar;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DetailItemType;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.chrome.browser.ui.fast_checkout.detail_screen.AutofillProfileItemProperties;
import org.chromium.chrome.browser.ui.fast_checkout.detail_screen.DetailScreenCoordinator;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Simple unit tests for the detail screen view.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, ChromeSwitches.DISABLE_NATIVE_INITIALIZATION})
public class FastCheckoutDetailScreenViewTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public TestRule mCommandLineFlagsRule = CommandLineFlags.getTestRule();
    @Rule
    public ActivityScenarioRule<ChromeTabbedActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(ChromeTabbedActivity.class);

    @Mock
    private Runnable mBackClickHandler;
    @Mock
    private Runnable mSettingsClickHandler;
    @Mock
    private Runnable mCallback1;
    @Mock
    private Runnable mCallback2;

    private PropertyModel mModel;
    private View mView;
    // Test support.
    private ShadowLooper mShadowLooper;

    @Before
    public void setUp() {
        mShadowLooper = ShadowLooper.shadowMainLooper();
        mActivityScenarioRule.getScenario().onActivity(activity -> {
            mModel = FastCheckoutProperties.createDefaultModel();
            mModel.set(DETAIL_SCREEN_SETTINGS_CLICK_HANDLER,
                    FastCheckoutMediator.createSettingsOnClickListener(mSettingsClickHandler));
            mModel.set(DETAIL_SCREEN_BACK_CLICK_HANDLER, mBackClickHandler);

            // Create the view.
            mView = LayoutInflater.from(activity).inflate(
                    R.layout.fast_checkout_detail_screen_sheet, null);

            // Let the coordinator connect model and view.
            new DetailScreenCoordinator(activity, mView, mModel);
            activity.setContentView(mView);
        });
    }

    @Test
    @SmallTest
    public void testBackArrowClickCallsHandler() {
        assertNotNull(mView);
        Toolbar toolbar = mView.findViewById(R.id.action_bar);
        assertNotNull(toolbar);

        // Find the navigation button. Toolbar does not expose a method to get
        // the navigation button and Espresso does not work in this setup.
        // TODO(crbug.com/1355310): Move to integration test once that exists.
        View backButton = null;
        for (int index = 0; index < toolbar.getChildCount(); ++index) {
            View candidateView = toolbar.getChildAt(index);
            if (candidateView.getContentDescription() != null
                    && candidateView.getContentDescription().equals(
                            toolbar.getNavigationContentDescription())) {
                backButton = candidateView;
            }
        }
        assertNotNull(backButton);
        backButton.performClick();
        mShadowLooper.idle();
    }

    @Test
    @SmallTest
    public void testOpenSettingsClickCallsHandler() {
        assertNotNull(mView);

        // Click on the settings element.
        View settingsMenuElement = mView.findViewById(R.id.settings_menu_id);
        assertNotNull(settingsMenuElement);
        settingsMenuElement.performClick();
        mShadowLooper.idle();
    }

    @Test
    @SmallTest
    public void testRecyclerViewPopulatesItemEntriesAndReactsToClicks() {
        assertNotNull(mView);

        FastCheckoutAutofillProfile profile1 =
                FastCheckoutTestUtils.createDummyProfile("John Moe", "john.moe@gmail.com");
        FastCheckoutAutofillProfile profile2 =
                FastCheckoutTestUtils.createDummyProfile("Jane Doe", "doe.jane@gmail.com");

        ListModel<ListItem> models = mModel.get(PROFILE_MODEL_LIST);
        models.add(new ListItem(DetailItemType.PROFILE,
                AutofillProfileItemProperties.create(profile1, /*isSelected=*/false,
                        /*onClickListener=*/mCallback1)));
        models.add(new ListItem(DetailItemType.PROFILE,
                AutofillProfileItemProperties.create(profile2, /*isSelected=*/true,
                        /*onClickListener=*/mCallback2)));
        mModel.set(DETAIL_SCREEN_MODEL_LIST, models);

        // Check that the sheet is populated properly.
        mShadowLooper.idle();
        assertThat(getProfileItems().getChildCount(), is(2));

        // Check that clicks are handled properly.
        getProfileItemAt(0).performClick();
        mShadowLooper.idle();
        verify(mCallback1, times(1)).run();
        verify(mCallback2, never()).run();
    }

    private RecyclerView getProfileItems() {
        return mView.findViewById(R.id.fast_checkout_detail_screen_recycler_view);
    }

    private View getProfileItemAt(int index) {
        return getProfileItems().getChildAt(index);
    }
}
