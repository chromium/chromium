// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

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
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link BottomBarPromoDialogCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BottomBarPromoDialogCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Profile mProfile;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private Tracker mTracker;
    @Mock private BottomBarPromoDialogCoordinator.BottomBarPromoDialogListener mListener;
    @Mock private GlicEnabling.Natives mGlicEnablingJniMock;

    @Captor private ArgumentCaptor<PropertyModel> mModelCaptor;

    private Context mContext;
    private Activity mActivity;
    private SettableNonNullObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    private OneshotSupplierImpl<String> mCountrySupplier;
    private BottomBarPromoDialogCoordinator mCoordinator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mModalDialogManagerSupplier = ObservableSuppliers.createNonNull(mModalDialogManager);
        mCountrySupplier = new OneshotSupplierImpl<>();
        mCountrySupplier.set("us");
        TrackerFactory.setTrackerForTests(mTracker);
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingJniMock);
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);

        mCoordinator =
                new BottomBarPromoDialogCoordinator(
                        mActivity, mModalDialogManagerSupplier, mCountrySupplier);
        mCoordinator.setListener(mListener);
    }

    @After
    public void tearDown() {
        TrackerFactory.setTrackerForTests(null);
    }

    @Test
    public void testMaybeShowPromoDialog_SuccessfulShow() {
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_PROMO_DIALOG))
                .thenReturn(true);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomBar.Promo.Event", BottomBarMetrics.PromoEvent.SHOWN)
                        .build();

        mCoordinator.maybeShowPromoDialog(mProfile);

        watcher.assertExpected();

        verify(mModalDialogManager)
                .showDialog(
                        mModelCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));

        PropertyModel model = mModelCaptor.getValue();
        assertNotNull(model);
        assertEquals(mCoordinator, model.get(ModalDialogProperties.CONTROLLER));
        assertNotNull(model.get(ModalDialogProperties.CUSTOM_VIEW));
    }

    @Test
    public void testMaybeShowPromoDialog_FETRejected() {
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_PROMO_DIALOG))
                .thenReturn(false);

        mCoordinator.maybeShowPromoDialog(mProfile);

        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyBoolean());
    }

    @Test
    public void testMaybeShowPromoDialog_NullProfile() {
        mCoordinator.maybeShowPromoDialog(null);

        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyBoolean());
    }

    @Test
    public void testMaybeShowPromoDialog_DuplicateCallPrevented() {
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_PROMO_DIALOG))
                .thenReturn(true);

        mCoordinator.maybeShowPromoDialog(mProfile);
        verify(mModalDialogManager, times(1)).showDialog(any(), anyInt(), anyBoolean());

        // Second call should be a no-op because dialog is already showing
        mCoordinator.maybeShowPromoDialog(mProfile);
        verify(mModalDialogManager, times(1)).showDialog(any(), anyInt(), anyBoolean());
    }

    @Test
    public void testPositiveButtonClickDismissesAndNotifiesAccepted() {
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_PROMO_DIALOG))
                .thenReturn(true);

        mCoordinator.maybeShowPromoDialog(mProfile);
        verify(mModalDialogManager).showDialog(mModelCaptor.capture(), anyInt(), anyBoolean());
        PropertyModel model = mModelCaptor.getValue();

        // Perform click
        mCoordinator.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);
        verify(mModalDialogManager)
                .dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomBar.Promo.Event",
                                BottomBarMetrics.PromoEvent.ACCEPTED)
                        .build();

        // Perform dismiss
        mCoordinator.onDismiss(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        watcher.assertExpected();

        verify(mTracker).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_PROMO_DIALOG);

        // Check listener callback is invoked synchronously
        verify(mListener).onPromoDialogAccepted();
    }

    @Test
    public void testNegativeButtonClickDismisses() {
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_PROMO_DIALOG))
                .thenReturn(true);

        mCoordinator.maybeShowPromoDialog(mProfile);
        verify(mModalDialogManager).showDialog(mModelCaptor.capture(), anyInt(), anyBoolean());
        PropertyModel model = mModelCaptor.getValue();

        // Perform click
        mCoordinator.onClick(model, ModalDialogProperties.ButtonType.NEGATIVE);
        verify(mModalDialogManager)
                .dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomBar.Promo.Event",
                                BottomBarMetrics.PromoEvent.DISMISSED)
                        .build();

        // Perform dismiss
        mCoordinator.onDismiss(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);

        watcher.assertExpected();

        verify(mTracker).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_PROMO_DIALOG);

        verify(mListener, never()).onPromoDialogAccepted();
    }

    @Test
    public void testDestroyDismissesWithoutCallback() {
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_PROMO_DIALOG))
                .thenReturn(true);

        mCoordinator.maybeShowPromoDialog(mProfile);
        verify(mModalDialogManager).showDialog(mModelCaptor.capture(), anyInt(), anyBoolean());
        PropertyModel model = mModelCaptor.getValue();

        mCoordinator.destroy();
        verify(mModalDialogManager).dismissDialog(model, DialogDismissalCause.ACTIVITY_DESTROYED);

        mCoordinator.onDismiss(model, DialogDismissalCause.ACTIVITY_DESTROYED);
        verify(mTracker).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_PROMO_DIALOG);

        verify(mListener, never()).onPromoDialogAccepted();
    }

    @Test
    public void testMaybeShowPromoDialog_NoActionEligible() {
        mCountrySupplier = new OneshotSupplierImpl<>();
        mCountrySupplier.set("fr");
        mCoordinator =
                new BottomBarPromoDialogCoordinator(
                        mActivity, mModalDialogManagerSupplier, mCountrySupplier);
        mCoordinator.setListener(mListener);

        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_PROMO_DIALOG))
                .thenReturn(true);
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_AIM_PROMO_DIALOG))
                .thenReturn(true);

        mCoordinator.maybeShowPromoDialog(mProfile);

        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyBoolean());
    }

    @Test
    public void testMaybeShowPromoDialog_CountryNull_ReturnsFalse() {
        mCountrySupplier = new OneshotSupplierImpl<>();
        mCoordinator =
                new BottomBarPromoDialogCoordinator(
                        mActivity, mModalDialogManagerSupplier, mCountrySupplier);
        mCoordinator.setListener(mListener);

        when(mTracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_PROMO_DIALOG))
                .thenReturn(true);

        assertFalse(mCoordinator.maybeShowPromoDialog(mProfile));
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyBoolean());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR_AIM,
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":bypass_aim_geofencing/true"
    })
    public void testMaybeShowPromoDialog_AimSuccessfulShow() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_AIM_PROMO_DIALOG))
                .thenReturn(true);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomBar.Promo.Event", BottomBarMetrics.PromoEvent.SHOWN)
                        .build();

        mCoordinator.maybeShowPromoDialog(mProfile);

        watcher.assertExpected();

        verify(mModalDialogManager)
                .showDialog(
                        mModelCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));

        PropertyModel model = mModelCaptor.getValue();
        assertNotNull(model);
        assertEquals(mCoordinator, model.get(ModalDialogProperties.CONTROLLER));
        assertNotNull(model.get(ModalDialogProperties.CUSTOM_VIEW));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR_AIM,
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":bypass_aim_geofencing/true"
    })
    public void testAimPositiveButtonClickDismissesAndNotifiesAccepted() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_AIM_PROMO_DIALOG))
                .thenReturn(true);

        mCoordinator.maybeShowPromoDialog(mProfile);
        verify(mModalDialogManager).showDialog(mModelCaptor.capture(), anyInt(), anyBoolean());
        PropertyModel model = mModelCaptor.getValue();

        // Perform click
        mCoordinator.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);
        verify(mModalDialogManager)
                .dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomBar.Promo.Event",
                                BottomBarMetrics.PromoEvent.ACCEPTED)
                        .build();

        // Perform dismiss
        mCoordinator.onDismiss(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        watcher.assertExpected();

        verify(mTracker).dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_AIM_PROMO_DIALOG);

        // Check listener callback is invoked synchronously
        verify(mListener).onPromoDialogAccepted();
    }

    @Test
    public void testMaybeShowPromoDialog_IncognitoProfile_UsesOriginalProfile() {
        Profile incognitoProfile = org.mockito.Mockito.mock(Profile.class);
        when(incognitoProfile.isOffTheRecord()).thenReturn(true);
        when(incognitoProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mGlicEnablingJniMock.isEnabledForProfile(eq(mProfile))).thenReturn(true);
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_PROMO_DIALOG))
                .thenReturn(true);

        assertTrue(mCoordinator.maybeShowPromoDialog(incognitoProfile));
        verify(mModalDialogManager).showDialog(mModelCaptor.capture(), anyInt(), anyBoolean());
    }
}
