// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Configuration;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/** Unit tests for {@link BottomBarEducationUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomBarEducationUtilsUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private GlicEnabling.Natives mGlicEnablingJniMock;
    @Mock private Runnable mOnSettingsClicked;
    @Mock private Runnable mOnPositiveClicked;
    @Mock private Runnable mOnNegativeClicked;

    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingJniMock);
    }

    @After
    public void tearDown() {
        BottomBarActionEligibility.setCachedCandidateExtraActionForTesting(null);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":education_gemini_focused/true"})
    public void testShouldUseGlicVariant_Enabled() {
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        when(mGlicEnablingJniMock.isPolicyEnforced(any())).thenReturn(true);
        assertTrue(BottomBarEducationUtils.shouldUseGlicVariant(mProfile));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":education_gemini_focused/true"})
    public void testShouldUseGlicVariant_GlicDisabled() {
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);
        assertFalse(BottomBarEducationUtils.shouldUseGlicVariant(mProfile));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":education_gemini_focused/false"})
    public void testShouldUseGlicVariant_FeatureParamDisabled() {
        assertFalse(BottomBarEducationUtils.shouldUseGlicVariant(mProfile));
    }

    @Test
    public void testInflateBubbleContent_GeminiFocused() {
        View view = inflateBubbleContent(/* glicFocused= */ true);
        assertTextAndLink(
                view,
                R.string.bottom_bar_education_title_gemini,
                R.string.bottom_bar_education_body);
        assertBubbleButtons(view);
        ImageView illustration = view.findViewById(R.id.education_illustration);
        assertEquals(View.VISIBLE, illustration.getVisibility());
        assertEquals(
                R.drawable.bottom_bar_education_illustration_gemini,
                Shadows.shadowOf(illustration.getDrawable()).getCreatedFromResId());
    }

    @Test
    public void testInflateBubbleContent_Ergonomics() {
        View view = inflateBubbleContent(/* glicFocused= */ false);
        assertTextAndLink(
                view,
                R.string.bottom_bar_education_title_ergonomics,
                R.string.bottom_bar_education_body);
        assertBubbleButtons(view);
        ImageView illustration = view.findViewById(R.id.education_illustration);
        assertEquals(View.VISIBLE, illustration.getVisibility());
        assertEquals(
                R.drawable.bottom_bar_education_illustration_ergonomics,
                Shadows.shadowOf(illustration.getDrawable()).getCreatedFromResId());
    }

    @Test
    public void testInflateDialogContent() {
        View view = BottomBarEducationUtils.inflateDialogContent(mContext, mOnSettingsClicked);
        assertTextAndLink(
                view,
                R.string.bottom_bar_education_title_opt_in,
                R.string.bottom_bar_education_body_opt_in);
        assertEquals(View.GONE, view.findViewById(R.id.button_bar).getVisibility());
        ImageView illustration = view.findViewById(R.id.education_illustration);
        assertEquals(View.VISIBLE, illustration.getVisibility());
        assertEquals(
                R.drawable.bottom_bar_education_illustration_ergonomics,
                Shadows.shadowOf(illustration.getDrawable()).getCreatedFromResId());
    }

    @Test
    @Config(qualifiers = "land")
    public void testInflateBubbleContent_LandscapeHidesIllustration() {
        View view = inflateBubbleContent(/* glicFocused= */ true);
        assertEquals(View.GONE, view.findViewById(R.id.education_illustration).getVisibility());
    }

    @Test
    public void testUpdateIllustrationVisibility() {
        View view = inflateBubbleContent(/* glicFocused= */ true);
        View illustration = view.findViewById(R.id.education_illustration);

        Configuration landscapeConfig = new Configuration();
        landscapeConfig.orientation = Configuration.ORIENTATION_LANDSCAPE;
        BottomBarEducationUtils.updateIllustrationVisibility(view, landscapeConfig);
        assertEquals(View.GONE, illustration.getVisibility());

        Configuration portraitConfig = new Configuration();
        portraitConfig.orientation = Configuration.ORIENTATION_PORTRAIT;
        BottomBarEducationUtils.updateIllustrationVisibility(view, portraitConfig);
        assertEquals(View.VISIBLE, illustration.getVisibility());
    }

    private View inflateBubbleContent(boolean glicFocused) {
        return BottomBarEducationUtils.inflateBubbleContent(
                mContext, glicFocused, mOnSettingsClicked, mOnPositiveClicked, mOnNegativeClicked);
    }

    private void assertTextAndLink(View view, int expectedTitleRes, int expectedBodyRes) {
        TextView title = view.findViewById(R.id.education_title);
        assertEquals(mContext.getString(expectedTitleRes), title.getText().toString());

        TextView body = view.findViewById(R.id.education_body);
        assertTrue(body.getMovementMethod() instanceof LinkMovementMethod);
        CharSequence bodyText = body.getText();
        assertEquals(
                SpanApplier.applySpans(
                                mContext.getString(expectedBodyRes),
                                new SpanInfo("<link>", "</link>"))
                        .toString(),
                bodyText.toString());
        assertTrue(bodyText instanceof Spanned);
        ClickableSpan[] spans =
                ((Spanned) bodyText).getSpans(0, bodyText.length(), ClickableSpan.class);
        assertEquals(1, spans.length);
        assertTrue(spans[0] instanceof ChromeClickableSpan);
        spans[0].onClick(body);
        verify(mOnSettingsClicked).run();
    }

    private void assertBubbleButtons(View view) {
        assertEquals(View.VISIBLE, view.findViewById(R.id.button_bar).getVisibility());

        TextView positive = view.findViewById(R.id.positive_button);
        assertEquals(
                mContext.getString(R.string.bottom_bar_education_show_me),
                positive.getText().toString());
        positive.performClick();
        verify(mOnPositiveClicked).run();

        TextView negative = view.findViewById(R.id.negative_button);
        assertEquals(
                mContext.getString(R.string.bottom_bar_education_switch_back),
                negative.getText().toString());
        negative.performClick();
        verify(mOnNegativeClicked).run();
    }
}
