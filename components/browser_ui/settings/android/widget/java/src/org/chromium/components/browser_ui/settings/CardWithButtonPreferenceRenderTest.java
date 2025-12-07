// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertNotNull;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.ApplicationTestUtils.finishActivity;

import android.app.Activity;
import android.view.View;

import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.settings.test.R;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/** Render tests for {@link CardWithButtonPreference}. */
@RunWith(ParameterizedRunner.class)
@Batch(Batch.PER_CLASS)
public class CardWithButtonPreferenceRenderTest {
    private static final String TITLE = "Preference Title";
    private static final String SUMMARY = "This is a summary.";
    private static final String SUMMARY_WITH_LONG_TEXT =
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Phasellus cursus risus eu"
                + " turpis gravida, et venenatis elit tempor. Etiam tempus interdum elit, vel"
                + " placerat odio ullamcorper sit amet. Phasellus sapien mauris, mattis suscipit"
                + " libero ac, hendrerit consequat nunc. Suspendisse a nulla ut enim fringilla"
                + " feugiat vel ut elit. Donec at ultricies arcu. Nullam consectetur quam ut ipsum"
                + " cursus, vel venenatis tellus tincidunt. Aenean eu imperdiet massa. Vestibulum"
                + " hendrerit lobortis nulla, sed finibus lorem consectetur ac. Fusce efficitur"
                + " mauris non tortor aliquet tempus. Fusce ac erat et nisi suscipit malesuada a a"
                + " nulla. Quisque in bibendum justo. Integer vitae feugiat ipsum, eget consequat"
                + " tortor. Phasellus tempus velit quis lacus laoreet, non egestas massa lacinia."
                + " Suspendisse quis tortor hendrerit, tincidunt quam nec, rutrum odio.";
    private static final String BUTTON_TEXT = "Button text";

    // TODO(crbug.com/440316886): Add a parameter for RTL text direction.
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false).name("Default"),
                    new ParameterSet().value(true).name("NightMode"));

    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_SETTINGS)
                    .setDescription("Initial screenshots for CardWithButtonPreference")
                    .build();

    private Activity mActivity;
    private PreferenceFragmentCompat mPreferenceFragment;
    private PreferenceScreen mPreferenceScreen;

    public CardWithButtonPreferenceRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        mSettingsRule.launchPreference(PlaceholderSettingsForTest.class);
        mActivity = mSettingsRule.getActivity();
        mPreferenceFragment = mSettingsRule.getPreferenceFragment();
        mPreferenceScreen = mSettingsRule.getPreferenceScreen();
    }

    @After
    public void tearDown() throws Exception {
        runOnUiThreadBlocking(NightModeTestUtils::tearDownNightModeForBlankUiTestActivity);
        try {
            finishActivity(mSettingsRule.getActivity());
        } catch (Exception e) {
            // Activity was already closed (e.g. due to last test tearing down the suite).
        }
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testCardWithButtonPreference() throws IOException {
        CardWithButtonPreference preference = new CardWithButtonPreference(mActivity, null);
        preference.setIconResource(R.drawable.ic_globe_24dp);
        preference.setTitle(TITLE);
        preference.setSummary(SUMMARY);
        preference.setButtonText(BUTTON_TEXT);
        mPreferenceScreen.addPreference(preference);

        ViewUtils.waitForVisibleView(withId(R.id.card_layout));
        View view = mActivity.findViewById(R.id.card_layout);
        assertNotNull(view);

        mRenderTestRule.render(view, "card_with_button_preference");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testCardWithButtonPreference_noIcon() throws IOException {
        CardWithButtonPreference preference = new CardWithButtonPreference(mActivity, null);
        preference.setTitle(TITLE);
        preference.setSummary(SUMMARY);
        preference.setButtonText(BUTTON_TEXT);
        mPreferenceScreen.addPreference(preference);

        ViewUtils.waitForVisibleView(withId(R.id.card_layout));
        View view = mActivity.findViewById(R.id.card_layout);
        assertNotNull(view);

        mRenderTestRule.render(view, "card_with_button_preference_no_icon");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testCardWithButtonPreferenceWithLongSummary() throws IOException {
        CardWithButtonPreference preference = new CardWithButtonPreference(mActivity, null);
        preference.setIconResource(R.drawable.ic_globe_24dp);
        preference.setTitle(TITLE);
        preference.setSummary(SUMMARY_WITH_LONG_TEXT);
        preference.setButtonText(BUTTON_TEXT);
        mPreferenceScreen.addPreference(preference);

        ViewUtils.waitForVisibleView(withId(R.id.card_layout));
        View view = mActivity.findViewById(R.id.card_layout);
        assertNotNull(view);

        mRenderTestRule.render(view, "card_with_button_preference_long_summary");
    }
}
