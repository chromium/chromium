// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.content.Context;
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
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils.DefaultPinState;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/** Unit tests for {@link BottomBarEducationUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomBarEducationUtilsUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;
    @Mock private GlicEnabling.Natives mGlicEnablingJniMock;
    @Mock private Runnable mOnSettingsClicked;
    @Mock private Runnable mOnPositiveClicked;
    @Mock private Runnable mOnNegativeClicked;
    @Captor private ArgumentCaptor<Callback<Boolean>> mInitCallbackCaptor;

    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingJniMock);
        TrackerFactory.setTrackerForTests(mTracker);
    }

    @After
    public void tearDown() {
        BottomBarActionEligibility.setCachedCandidateExtraActionForTesting(null);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.BOTTOM_BAR_DEFAULT_PIN_STATE);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR})
    public void testMaybeBackfillEducationSeen() {
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        // No pending state: does not query the Tracker.
        BottomBarEducationUtils.maybeBackfillEducationSeen(mProfile);
        writePinState(DefaultPinState.KEPT_OFF);
        BottomBarEducationUtils.maybeBackfillEducationSeen(mProfile);
        writePinState(DefaultPinState.RELEASED);
        BottomBarEducationUtils.maybeBackfillEducationSeen(mProfile);
        verifyNoInteractions(mTracker);

        // Pending pin exists, but neither hasEverTriggered nor wouldTriggerHelpUi is true: stays
        // PENDING.
        writePinState(DefaultPinState.PENDING_OFF);
        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.hasEverTriggered(
                        FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION, /* fromWindow= */ true))
                .thenReturn(false);
        when(mTracker.wouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION))
                .thenReturn(false);
        BottomBarEducationUtils.maybeBackfillEducationSeen(mProfile);
        assertEquals(DefaultPinState.PENDING_OFF, readPinState());

        // Pending pin exists and wouldTriggerHelpUi is true, but education cannot show because
        // education_enabled is false or AndroidBottomBar is off (rollback, safe mode): stays
        // PENDING so the bar does not flip without education able to show.
        when(mTracker.wouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION))
                .thenReturn(true);
        ChromeFeatureList.sAndroidBottomBarEducationEnabled.setForTesting(false);
        BottomBarEducationUtils.maybeBackfillEducationSeen(mProfile);
        assertEquals(DefaultPinState.PENDING_OFF, readPinState());
        ChromeFeatureList.sAndroidBottomBarEducationEnabled.setForTesting(true);
        FeatureOverrides.disable(ChromeFeatureList.ANDROID_BOTTOM_BAR);
        BottomBarEducationUtils.maybeBackfillEducationSeen(mProfile);
        assertEquals(DefaultPinState.PENDING_OFF, readPinState());
        FeatureOverrides.enable(ChromeFeatureList.ANDROID_BOTTOM_BAR);

        // Seen when Tracker is already initialized: transitions to KEPT, even though both
        // isEducationEnabled() and wouldTriggerHelpUi are true.
        when(mTracker.hasEverTriggered(
                        FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION, /* fromWindow= */ true))
                .thenReturn(true);
        BottomBarEducationUtils.maybeBackfillEducationSeen(mProfile);
        assertEquals(DefaultPinState.KEPT_OFF, readPinState());

        // Never-seen via async Tracker initialization: transitions to RELEASED.
        writePinState(DefaultPinState.PENDING_OFF);
        when(mTracker.isInitialized()).thenReturn(false);
        when(mTracker.hasEverTriggered(
                        FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION, /* fromWindow= */ true))
                .thenReturn(false);
        BottomBarEducationUtils.maybeBackfillEducationSeen(mProfile);
        verify(mTracker).addOnInitializedCallback(mInitCallbackCaptor.capture());
        mInitCallbackCaptor.getValue().onResult(true);
        assertEquals(DefaultPinState.RELEASED, readPinState());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR})
    public void testMaybeBackfillEducationSeen_EducationShownBeforeTrackerAnswers() {
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        writePinState(DefaultPinState.PENDING_ON);
        when(mTracker.isInitialized()).thenReturn(false);
        BottomBarEducationUtils.maybeBackfillEducationSeen(mProfile);
        verify(mTracker).addOnInitializedCallback(mInitCallbackCaptor.capture());

        // Feature Engagement posts its init callbacks, so education can show before this callback
        // runs. Once KEPT_ON is recorded, the callback returns early without querying the Tracker.
        BottomBarConfigUtils.recordEducationShown();
        mInitCallbackCaptor.getValue().onResult(true);

        verify(mTracker, never()).hasEverTriggered(any(), anyBoolean());
        assertEquals(DefaultPinState.KEPT_ON, readPinState());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":education_gemini_focused/true"})
    public void testShouldUseGlicVariant_Enabled() {
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
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
    public void testCreateBubbleContent_GeminiFocused() {
        View view = createBubbleContentView(/* glicFocused= */ true);
        assertTextAndLink(
                view,
                R.string.bottom_bar_education_title_gemini,
                R.string.bottom_bar_education_body_gemini);
        assertBubbleButtons(view);
        ImageView illustration = view.findViewById(R.id.education_illustration);
        assertEquals(View.VISIBLE, illustration.getVisibility());
        assertEquals(
                R.drawable.bottom_bar_education_illustration_gemini,
                Shadows.shadowOf(illustration.getDrawable()).getCreatedFromResId());
    }

    @Test
    public void testCreateBubbleContent_Ergonomics() {
        View view = createBubbleContentView(/* glicFocused= */ false);
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
    public void testCreateDialogContent() {
        View view =
                BottomBarEducationUtils.createContentView(
                        mContext,
                        BottomBarEducationUtils.createDialogContentModel(
                                mContext, mOnSettingsClicked));
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
    public void testCreateBubbleContent_LandscapeHidesIllustration() {
        View view = createBubbleContentView(/* glicFocused= */ true);
        assertEquals(View.GONE, view.findViewById(R.id.education_illustration).getVisibility());
    }

    private View createBubbleContentView(boolean glicFocused) {
        return BottomBarEducationUtils.createContentView(
                mContext,
                BottomBarEducationUtils.createBubbleContentModel(
                        mContext,
                        glicFocused,
                        mOnSettingsClicked,
                        mOnPositiveClicked,
                        mOnNegativeClicked));
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
        assertEquals(mContext.getString(R.string.got_it), positive.getText().toString());
        positive.performClick();
        verify(mOnPositiveClicked).run();

        TextView negative = view.findViewById(R.id.negative_button);
        assertEquals(
                mContext.getString(R.string.bottom_bar_education_switch_back),
                negative.getText().toString());
        negative.performClick();
        verify(mOnNegativeClicked).run();
    }

    private static @DefaultPinState int readPinState() {
        return ChromeSharedPreferences.getInstance()
                .readInt(ChromePreferenceKeys.BOTTOM_BAR_DEFAULT_PIN_STATE, DefaultPinState.NONE);
    }

    private static void writePinState(@DefaultPinState int state) {
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.BOTTOM_BAR_DEFAULT_PIN_STATE, state);
    }
}
