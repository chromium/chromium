// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;

import android.app.Activity;

import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;
import androidx.test.espresso.ViewInteraction;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.settings.test.R;

/** Tests of {@link ChromeImageViewPreference}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ChromeImageViewPreferenceTest {
    private static final String TITLE = "Preference Title";
    private static final String SUMMARY = "This is a summary.";
    private static final int DRAWABLE_RES = R.drawable.ic_folder_blue_24dp;
    private static final int CONTENT_DESCRIPTION_RES = R.string.ok;

    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    private Activity mActivity;
    private PreferenceFragmentCompat mPreferenceFragment;
    private PreferenceScreen mPreferenceScreen;

    @Before
    public void setUp() {
        mSettingsRule.launchPreference(PlaceholderSettingsForTest.class);
        mActivity = mSettingsRule.getActivity();
        mPreferenceFragment = mSettingsRule.getPreferenceFragment();
        mPreferenceScreen = mSettingsRule.getPreferenceScreen();
    }

    @Test
    @SmallTest
    public void testChromeImageViewPreference() {
        ChromeImageViewPreference preference = new ChromeImageViewPreference(mActivity);
        preference.setTitle(TITLE);
        preference.setSummary(SUMMARY);
        preference.setImageView(DRAWABLE_RES, CONTENT_DESCRIPTION_RES, null);
        mPreferenceScreen.addPreference(preference);

        Assert.assertTrue(preference.isEnabled());

        getTitleView().check(matches(allOf(withText(TITLE), isDisplayed())));
        getSummaryView().check(matches(allOf(withText(SUMMARY), isDisplayed())));
        getImageViewWidget().check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testChromeImageViewPreferenceManaged() {
        ChromeImageViewPreference preference = new ChromeImageViewPreference(mActivity);
        preference.setTitle(TITLE);
        preference.setImageView(DRAWABLE_RES, CONTENT_DESCRIPTION_RES, null);
        preference.setManagedPreferenceDelegate(ManagedPreferenceTestDelegates.POLICY_DELEGATE);
        mPreferenceScreen.addPreference(preference);

        Assert.assertFalse(preference.isEnabled());

        getTitleView().check(matches(allOf(withText(TITLE), isDisplayed())));
        getSummaryView()
                .check(
                        matches(
                                allOf(
                                        withText(R.string.managed_by_your_organization),
                                        isDisplayed())));
        getImageViewWidget().check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testChromeImageViewPreferenceResetImage() {
        ChromeImageViewPreference preference = new ChromeImageViewPreference(mActivity);
        preference.setTitle(TITLE);
        preference.setImageView(DRAWABLE_RES, CONTENT_DESCRIPTION_RES, null);
        mPreferenceScreen.addPreference(preference);

        getTitleView().check(matches(allOf(withText(TITLE), isDisplayed())));
        getImageViewWidget().check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    preference.setImageView(0, 0, null);
                });
        Assert.assertNull(preference.getButton().getDrawable());
    }

    private ViewInteraction getTitleView() {
        return onView(withId(android.R.id.title));
    }

    private ViewInteraction getSummaryView() {
        return onView(withId(android.R.id.summary));
    }

    private ViewInteraction getImageViewWidget() {
        return onView(withId(R.id.image_view_widget));
    }
}
