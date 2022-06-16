// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;

import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;
import androidx.test.espresso.ViewInteraction;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/**
 * Tests of {@link ChromeImageViewPreference}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ChromeImageViewPreferenceTest extends BlankUiTestActivityTestCase {
    private static final String TITLE = "Preference Title";
    private static final String SUMMARY = "This is a summary.";
    private static final int DRAWABLE_RES = R.drawable.ic_folder_blue_24dp;
    private static final int CONTENT_DESCRIPTION_RES = R.string.ok;

    private PreferenceFragmentCompat mPreferenceFragment;
    private PreferenceScreen mPreferenceScreen;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPreferenceFragment = new PlaceholderSettingsForTest();
            getActivity()
                    .getSupportFragmentManager()
                    .beginTransaction()
                    .replace(android.R.id.content, mPreferenceFragment)
                    .commit();
        });
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mPreferenceFragment.getPreferenceManager(), Matchers.notNullValue());
            Criteria.checkThat(mPreferenceFragment.getPreferenceScreen(), Matchers.notNullValue());
        });
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mPreferenceScreen = mPreferenceFragment.getPreferenceScreen(); });
    }

    @Test
    @SmallTest
    public void testChromeImageViewPreference() {
        ChromeImageViewPreference preference = new ChromeImageViewPreference(getActivity());
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
        ChromeImageViewPreference preference = new ChromeImageViewPreference(getActivity());
        preference.setTitle(TITLE);
        preference.setImageView(DRAWABLE_RES, CONTENT_DESCRIPTION_RES, null);
        preference.setManagedPreferenceDelegate(ManagedPreferencesUtilsTest.POLICY_DELEGATE);
        mPreferenceScreen.addPreference(preference);

        Assert.assertFalse(preference.isEnabled());

        getTitleView().check(matches(allOf(withText(TITLE), isDisplayed())));
        getSummaryView().check(
                matches(allOf(withText(R.string.managed_by_your_organization), isDisplayed())));
        getImageViewWidget().check(matches(isDisplayed()));
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
