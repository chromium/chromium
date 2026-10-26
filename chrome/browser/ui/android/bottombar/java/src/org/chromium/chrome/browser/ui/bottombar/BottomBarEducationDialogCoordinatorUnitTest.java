// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Configuration;
import android.view.View;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.EducationOnboardingEvent;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({
    ChromeFeatureList.ANDROID_BOTTOM_BAR + ":education_enabled/true/default_user_enabled/false"
})
public class BottomBarEducationDialogCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;
    @Mock private Tab mTab;

    @Captor private ArgumentCaptor<PropertyModel> mModelCaptor;

    private Activity mActivity;
    private BottomBarEducationDialogCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        TrackerFactory.setTrackerForTests(mTracker);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION))
                .thenReturn(true);

        mCoordinator =
                new BottomBarEducationDialogCoordinator(mActivity, mModalDialogManager, () -> mTab);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR})
    public void testNotShownWhenEducationDisabled() {
        assertFalse(mCoordinator.maybeShow(mProfile));
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyBoolean());
    }

    @Test
    public void testNotShownInIncognito() {
        when(mProfile.isOffTheRecord()).thenReturn(true);
        assertFalse(mCoordinator.maybeShow(mProfile));
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyBoolean());

        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mTab.isOffTheRecord()).thenReturn(true);
        assertFalse(mCoordinator.maybeShow(mProfile));
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyBoolean());
    }

    @Test
    public void testNotShownWhenUserEnabled() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.BOTTOM_BAR_ENABLED, /* value= */ true);
        assertFalse(mCoordinator.maybeShow(mProfile));
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyBoolean());
    }

    @Test
    public void testNotShownWhenHasUserSetBottomBarEnabled() {
        // Preference contains the key, even if false
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.BOTTOM_BAR_ENABLED, /* value= */ false);
        assertFalse(mCoordinator.maybeShow(mProfile));
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyBoolean());
    }

    @Test
    public void testNotShownWhenShouldTriggerHelpUiFalse() {
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION))
                .thenReturn(false);
        assertFalse(mCoordinator.maybeShow(mProfile));
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyBoolean());
    }

    @Test
    public void testShownAndTitleText() {
        assertTrue(mCoordinator.maybeShow(mProfile));
        assertTrue(BottomBarEducationCoordinator.hasEducationShownThisSession());
        verify(mModalDialogManager)
                .showDialog(
                        mModelCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));

        PropertyModel model = mModelCaptor.getValue();
        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        TextView title = customView.findViewById(R.id.education_title);
        assertEquals(
                mActivity.getString(R.string.bottom_bar_education_title_opt_in),
                title.getText().toString());
        assertEquals(View.GONE, customView.findViewById(R.id.button_bar).getVisibility());
        assertEquals(
                mActivity.getString(R.string.bottom_bar_education_try_it_now),
                model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals(
                mActivity.getString(R.string.bottom_bar_education_not_now),
                model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
    }

    @Test
    public void testPositiveButton() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomBar.Education.Onboarding.OptInDialog.Event",
                                EducationOnboardingEvent.SHOWN)
                        .expectIntRecord(
                                "Android.BottomBar.Education.Onboarding.OptInDialog.Event",
                                EducationOnboardingEvent.ACCEPTED)
                        .build();
        assertTrue(mCoordinator.maybeShow(mProfile));
        verify(mModalDialogManager)
                .showDialog(
                        mModelCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));

        PropertyModel model = mModelCaptor.getValue();

        doAnswer(
                        invocation -> {
                            assertFalse(BottomBarConfigUtils.isBottomBarUserEnabled());
                            return null;
                        })
                .when(mModalDialogManager)
                .dismissDialog(
                        any(PropertyModel.class), eq(DialogDismissalCause.POSITIVE_BUTTON_CLICKED));

        mCoordinator.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);

        InOrder inOrder = inOrder(mTracker, mModalDialogManager);
        inOrder.verify(mTracker, times(1))
                .notifyEvent(EventConstants.ANDROID_BOTTOM_BAR_EDUCATION_USED);
        inOrder.verify(mTracker, times(1))
                .notifyEvent(EventConstants.ANDROID_BOTTOM_BAR_EDUCATION_OPT_IN_ACCEPTED);
        inOrder.verify(mTracker, times(1)).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);
        inOrder.verify(mModalDialogManager)
                .dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        assertTrue(BottomBarConfigUtils.isBottomBarUserEnabled());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testNegativeButton() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomBar.Education.Onboarding.OptInDialog.Event",
                                EducationOnboardingEvent.SHOWN)
                        .expectIntRecord(
                                "Android.BottomBar.Education.Onboarding.OptInDialog.Event",
                                EducationOnboardingEvent.DECLINED)
                        .build();
        assertTrue(mCoordinator.maybeShow(mProfile));
        verify(mModalDialogManager)
                .showDialog(
                        mModelCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));

        PropertyModel model = mModelCaptor.getValue();
        mCoordinator.onClick(model, ModalDialogProperties.ButtonType.NEGATIVE);

        verify(mTracker, never()).notifyEvent(EventConstants.ANDROID_BOTTOM_BAR_EDUCATION_USED);
        verify(mTracker, never())
                .notifyEvent(EventConstants.ANDROID_BOTTOM_BAR_EDUCATION_OPT_IN_ACCEPTED);
        verify(mTracker, times(1)).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .contains(ChromePreferenceKeys.BOTTOM_BAR_ENABLED));
        verify(mModalDialogManager)
                .dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnDismissAfterPositiveButton() {
        assertTrue(mCoordinator.maybeShow(mProfile));
        verify(mModalDialogManager)
                .showDialog(
                        mModelCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));

        PropertyModel model = mModelCaptor.getValue();
        mCoordinator.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);

        mCoordinator.onDismiss(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        verify(mTracker, times(1)).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);
    }

    @Test
    public void testDismissal_NavigateBackOrTouchOutside_RecordsBackPressed() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomBar.Education.Onboarding.OptInDialog.Event",
                                EducationOnboardingEvent.SHOWN)
                        .expectIntRecord(
                                "Android.BottomBar.Education.Onboarding.OptInDialog.Event",
                                EducationOnboardingEvent.BACK_PRESSED)
                        .build();
        assertTrue(mCoordinator.maybeShow(mProfile));
        verify(mModalDialogManager)
                .showDialog(
                        mModelCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));

        PropertyModel model = mModelCaptor.getValue();
        mCoordinator.onDismiss(model, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

        verify(mTracker, times(1)).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testDestroyWhileShowing() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomBar.Education.Onboarding.OptInDialog.Event",
                                EducationOnboardingEvent.SHOWN)
                        .expectIntRecord(
                                "Android.BottomBar.Education.Onboarding.OptInDialog.Event",
                                EducationOnboardingEvent.DISMISSED_OTHER)
                        .build();
        assertTrue(mCoordinator.maybeShow(mProfile));
        verify(mModalDialogManager)
                .showDialog(
                        mModelCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));

        PropertyModel model = mModelCaptor.getValue();

        mCoordinator.destroy();

        verify(mModalDialogManager).dismissDialog(model, DialogDismissalCause.ACTIVITY_DESTROYED);
        verify(mTracker, times(1)).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_EDUCATION);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnConfigurationChanged_LandscapeHidesIllustration() {
        assertTrue(mCoordinator.maybeShow(mProfile));
        verify(mModalDialogManager)
                .showDialog(
                        mModelCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));

        PropertyModel model = mModelCaptor.getValue();
        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        View illustration = customView.findViewById(R.id.education_illustration);

        assertEquals(View.VISIBLE, illustration.getVisibility());

        Configuration config = new Configuration();
        config.orientation = Configuration.ORIENTATION_LANDSCAPE;
        mCoordinator.onConfigurationChanged(config);

        assertEquals(View.GONE, illustration.getVisibility());

        config.orientation = Configuration.ORIENTATION_PORTRAIT;
        mCoordinator.onConfigurationChanged(config);

        assertEquals(View.VISIBLE, illustration.getVisibility());
    }
}
