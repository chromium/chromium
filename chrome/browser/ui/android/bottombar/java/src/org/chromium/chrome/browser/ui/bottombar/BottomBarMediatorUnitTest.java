// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.util.DisplayMetrics;

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

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedServiceFactory;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager.Host;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.GlicIneligibilityReason;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link BottomBarMediator}. */
@NullMarked
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR})
public class BottomBarMediatorUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private BottomBarMediator.VisibilityDelegate mVisibilityDelegate;
    @Mock private Profile mProfile;
    @Mock private BottomBarButtonManager mButtonManager;
    @Mock private GlicEnabling.Natives mGlicEnablingJniMock;
    @Mock private GlicKeyedService mGlicKeyedService;
    @Mock private Context mContext;
    @Mock private Resources mResources;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private BottomBarEducationCoordinator mEducationCoordinator;
    @Mock private Tab mTab;
    private SettableNullableObservableSupplier<Tab> mTabSupplier;

    @Captor
    private ArgumentCaptor<GlicKeyedService.AllowedChangedObserver> mAllowedChangedObserverCaptor;

    private SettableNullableObservableSupplier<Profile> mProfileSupplier;

    private SettableNonNullObservableSupplier<Boolean> mHomepageEnabledSupplier;
    private SettableNonNullObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private SettableNonNullObservableSupplier<@ControlsPosition Integer> mToolbarPositionSupplier;
    private PropertyModel mModel;
    private @Nullable BottomBarMediator mMediator;

    @Before
    public void setUp() {
        mHomepageEnabledSupplier = ObservableSuppliers.createNonNull(false);
        mTabSupplier = ObservableSuppliers.createNullable();
        mTabSupplier.set(mTab);
        when(mTab.isOffTheRecord()).thenReturn(false);
        mOmniboxFocusStateSupplier = ObservableSuppliers.createNonNull(false);
        mToolbarPositionSupplier = ObservableSuppliers.createNonNull(ControlsPosition.TOP);
        mProfileSupplier = ObservableSuppliers.createNullable();
        mProfileSupplier.set(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mModel = new PropertyModel(BottomBarProperties.ALL_KEYS);
        when(mThemeColorProvider.getBrandedColorScheme())
                .thenReturn(BrandedColorScheme.APP_DEFAULT);
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingJniMock);
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);

        DisplayMetrics displayMetrics = new DisplayMetrics();
        displayMetrics.density = 1.0f;
        when(mContext.getResources()).thenReturn(mResources);
        when(mResources.getDisplayMetrics()).thenReturn(displayMetrics);
        when(mResources.getDimensionPixelSize(R.dimen.bottom_bar_new_tab_background_size))
                .thenReturn(40);
    }

    @After
    public void tearDown() {
        if (mMediator != null) {
            mMediator.destroy();
        }
    }

    @Test
    public void testInitialization_ObservesHomepage() {
        createMediator();

        mHomepageEnabledSupplier.set(true);
        verify(mButtonManager).setButtonVisibility(ActionId.HOME_BUTTON, true);
    }

    @Test
    public void testConstructor() {
        createMediator();

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testGlicAllowedChanged_WhenGlicCandidate_HidesGlic() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);

        createMediator();

        // Verify initial state: GLIC is visible.
        verify(mButtonManager).setButtonVisibility(ActionId.GLIC, true);

        verify(mGlicKeyedService)
                .addAllowedChangedObserver(mAllowedChangedObserverCaptor.capture());
        GlicKeyedService.AllowedChangedObserver observer = mAllowedChangedObserverCaptor.getValue();
        assertNotNull(observer);

        // Policy or profile eligibility disallows GLIC -> GLIC hidden.
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);
        observer.onAllowedStateChanged();
        verify(mButtonManager).setButtonVisibility(ActionId.GLIC, false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testProfileChanged_ReResolvesCandidateAndUpdatesObservers() {
        when(mGlicEnablingJniMock.isEnabledForProfile(mProfile)).thenReturn(true);

        createMediator();

        // Verify initial state: GLIC is visible.
        verify(mButtonManager).setButtonVisibility(ActionId.GLIC, true);
        verify(mGlicKeyedService).addAllowedChangedObserver(any());

        // Transition through null: observers removed and GLIC hidden.
        mProfileSupplier.set(null);
        verify(mButtonManager).setButtonVisibility(ActionId.GLIC, false);
        verify(mGlicKeyedService).removeAllowedChangedObserver(any());

        Profile newProfile = mock(Profile.class);
        when(newProfile.getOriginalProfile()).thenReturn(newProfile);
        when(mGlicEnablingJniMock.isEnabledForProfile(newProfile)).thenReturn(false);

        GlicKeyedService newGlicKeyedService = mock(GlicKeyedService.class);
        GlicKeyedServiceFactory.setForTesting(newGlicKeyedService);

        mProfileSupplier.set(newProfile);

        verify(mButtonManager, times(2)).setButtonVisibility(ActionId.GLIC, false);
        verify(newGlicKeyedService).addAllowedChangedObserver(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testGlicAllowedChanged_WhenGlicInitiallyDisabled_ShowsGlicWhenEnabled() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);

        createMediator();

        // Verify initial state: GLIC is hidden.
        verify(mButtonManager).setButtonVisibility(ActionId.GLIC, false);

        verify(mGlicKeyedService)
                .addAllowedChangedObserver(mAllowedChangedObserverCaptor.capture());
        GlicKeyedService.AllowedChangedObserver observer = mAllowedChangedObserverCaptor.getValue();
        assertNotNull(observer);

        // GLIC becomes enabled for profile -> GLIC shown and cached as candidate extra action.
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        observer.onAllowedStateChanged();
        verify(mButtonManager).setButtonVisibility(ActionId.GLIC, true);
        assertEquals(
                Integer.valueOf(ActionId.GLIC),
                BottomBarActionEligibility.getCachedCandidateExtraAction());

        // Disabling via SharedPreferences should hide the button (confirming listener was
        // registered on candidate promotion).
        BottomBarConfigUtils.setGlicButtonEnabled(false);
        verify(mButtonManager, times(2)).setButtonVisibility(ActionId.GLIC, false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testOnSharedPreferenceChanged_TogglesGlicVisibility() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);

        createMediator();

        assertNotNull(mMediator);
        verify(mButtonManager).setButtonVisibility(ActionId.GLIC, true);

        // Toggle GLIC button OFF via SharedPreferences.
        BottomBarConfigUtils.setGlicButtonEnabled(false);
        verify(mButtonManager).setButtonVisibility(ActionId.GLIC, false);

        // Toggle GLIC button ON via SharedPreferences.
        BottomBarConfigUtils.setGlicButtonEnabled(true);
        verify(mButtonManager, times(2)).setButtonVisibility(ActionId.GLIC, true);
    }

    @Test
    public void testColdStart_NullProfile_HidesExtraButton() {
        mProfileSupplier.set(null);
        createMediator();
        RobolectricUtil.runAllBackgroundAndUi();

        assertNotNull(mMediator);
        verify(mButtonManager).setButtonVisibility(ActionId.GLIC, false);
        verify(mGlicKeyedService, never()).addAllowedChangedObserver(any());
    }

    @Test
    public void testDeferredCandidateResolution_ProfileDeferred() {
        mProfileSupplier.set(null);
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);

        createMediator();
        RobolectricUtil.runAllBackgroundAndUi();

        // While profile is null, extra buttons should remain hidden.
        verify(mButtonManager).setButtonVisibility(ActionId.GLIC, false);
        verify(mGlicKeyedService, never()).addAllowedChangedObserver(any());

        // Profile arrives.
        mProfileSupplier.set(mProfile);

        // Candidate resolves to GLIC and becomes visible.
        verify(mButtonManager).setButtonVisibility(ActionId.GLIC, true);
        verify(mGlicKeyedService).addAllowedChangedObserver(any());
    }

    @Test
    public void testCandidateExtraActionResolved_RecordsMetric() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        var glicWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.BottomBar.ExtraAction.CandidateResolved",
                        BottomBarMetrics.CandidateAction.GLIC);
        createMediator();
        glicWatcher.assertExpected();
    }

    @Test
    public void testCandidateExtraActionResolved_ProfileChanged_ReRecordsMetric() {
        when(mGlicEnablingJniMock.isEnabledForProfile(mProfile)).thenReturn(true);
        var initialWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.BottomBar.ExtraAction.CandidateResolved",
                        BottomBarMetrics.CandidateAction.GLIC);
        createMediator();
        initialWatcher.assertExpected();

        Profile newProfile = mock(Profile.class);
        when(newProfile.getOriginalProfile()).thenReturn(newProfile);
        when(mGlicEnablingJniMock.isEnabledForProfile(newProfile)).thenReturn(false);

        var newProfileWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.BottomBar.ExtraAction.CandidateResolved",
                        BottomBarMetrics.CandidateAction.NONE);
        mProfileSupplier.set(newProfile);
        newProfileWatcher.assertExpected();
    }

    @Test
    public void testCandidateExtraActionResolved_ActionNone_RecordsMetricOnce() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomBar.ExtraAction.CandidateResolved",
                                BottomBarMetrics.CandidateAction.NONE)
                        .expectIntRecord(
                                "Android.BottomBar.Glic.IneligibilityReason",
                                GlicIneligibilityReason.PROFILE_INELIGIBLE)
                        .build();

        createMediator();
        watcher.assertExpected();

        // Subsequent updates for the same profile should not re-run candidate resolution or re-log
        // ineligibility reasons.
        var secondWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.BottomBar.ExtraAction.CandidateResolved")
                        .expectNoRecords("Android.BottomBar.Glic.IneligibilityReason")
                        .build();
        mProfileSupplier.set(mProfile);
        secondWatcher.assertExpected();
    }

    @Test
    public void testUpdateObservers_ProfileChanged_CleansUpObservers() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        createMediator();

        verify(mGlicKeyedService).addAllowedChangedObserver(any());

        // When profile becomes null, observers should be cleaned up.
        mProfileSupplier.set(null);
        verify(mGlicKeyedService).removeAllowedChangedObserver(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_glic_setting_toggle/true")
    public void testUpdateGlicVisibility_DisabledInSettings_RecordsIneligibilityReason() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        when(mGlicEnablingJniMock.isPolicyEnforced(any())).thenReturn(false);
        BottomBarConfigUtils.setGlicButtonEnabled(false);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.BottomBar.Glic.IneligibilityReason",
                        GlicIneligibilityReason.USER_DISABLED_IN_SETTINGS);
        createMediator();
        watcher.assertExpected();
    }

    @Test
    public void testOnTintChanged_WhenHostTabbed_UpdatesColorSchemeAndNotifiesDelegate() {
        createMediator();
        assertNotNull(mMediator);
        assertEquals(Host.TABBED, mMediator.getHostForTesting());

        mMediator.onTintChanged(null, null, BrandedColorScheme.INCOGNITO);

        assertEquals(BrandedColorScheme.INCOGNITO, mModel.get(BottomBarProperties.COLOR_SCHEME));
        verify(mVisibilityDelegate).onBackgroundColorChanged();
    }

    @Test
    public void testOnTintChanged_WhenHostHub_SuppressesColorSchemeUpdate() {
        createMediator();
        assertNotNull(mMediator);

        // Set initial color scheme to INCOGNITO
        mMediator.onTintChanged(null, null, BrandedColorScheme.INCOGNITO);
        assertEquals(BrandedColorScheme.INCOGNITO, mModel.get(BottomBarProperties.COLOR_SCHEME));
        verify(mVisibilityDelegate, times(1)).onBackgroundColorChanged();

        // Switch to Hub host
        mMediator.setParent(Host.HUB);
        assertEquals(Host.HUB, mMediator.getHostForTesting());

        // When in Hub, onTintChanged (e.g. fired when closing last incognito tab) should be ignored
        mMediator.onTintChanged(null, null, BrandedColorScheme.APP_DEFAULT);

        // Color scheme in model should NOT have snapped to APP_DEFAULT
        assertEquals(BrandedColorScheme.INCOGNITO, mModel.get(BottomBarProperties.COLOR_SCHEME));
        // Visibility delegate should NOT have received another notification
        verify(mVisibilityDelegate, times(1)).onBackgroundColorChanged();
    }

    @Test
    public void testSetParent_TabbedToHubAndBack_UpdatesColorSchemeOnReturn() {
        createMediator();
        assertNotNull(mMediator);

        mMediator.onTintChanged(null, null, BrandedColorScheme.INCOGNITO);
        assertEquals(BrandedColorScheme.INCOGNITO, mModel.get(BottomBarProperties.COLOR_SCHEME));

        mMediator.setParent(Host.HUB);
        assertEquals(Host.HUB, mMediator.getHostForTesting());

        // Simulate ThemeColorProvider changing to APP_DEFAULT while in Hub
        when(mThemeColorProvider.getBrandedColorScheme())
                .thenReturn(BrandedColorScheme.APP_DEFAULT);

        // When returning to TABBED host, model and delegate should be updated
        mMediator.setParent(Host.TABBED);
        assertEquals(Host.TABBED, mMediator.getHostForTesting());
        assertEquals(BrandedColorScheme.APP_DEFAULT, mModel.get(BottomBarProperties.COLOR_SCHEME));
        verify(mVisibilityDelegate, times(2)).onBackgroundColorChanged();
    }

    @Test
    public void testOnBottomBarStateChanged_AfterDestroy_IsNoOp() {
        createMediator();
        assertNotNull(mMediator);

        mMediator.destroy();
        clearInvocations(mVisibilityDelegate);

        mMediator.onBottomBarStateChanged(/* visibilityChanged= */ true);
        verify(mVisibilityDelegate, never()).onModelTokenChange();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testToolbarPositionChanged_RemoveTopPaddingForBottomOmniboxDefaultEnabled() {
        createMediator();
        assertNotNull(mMediator);

        // Initially TOP toolbar: top padding not removed, height is 56px.
        assertFalse(mModel.get(BottomBarProperties.IS_TOP_PADDING_REMOVED));
        verify(mVisibilityDelegate).onHeightChanged(56);

        // Switch to BOTTOM toolbar: top padding removed, height is 48px.
        mToolbarPositionSupplier.set(ControlsPosition.BOTTOM);
        assertTrue(mModel.get(BottomBarProperties.IS_TOP_PADDING_REMOVED));
        verify(mVisibilityDelegate).onHeightChanged(48);

        // Switch to HUB host: top padding restored on view while in Hub. The tabbed host container
        // height must stay at 48px since the Hub hosts the view in its own container.
        clearInvocations(mVisibilityDelegate);
        mMediator.setParent(Host.HUB);
        assertFalse(mModel.get(BottomBarProperties.IS_TOP_PADDING_REMOVED));
        verify(mVisibilityDelegate, never()).onHeightChanged(56);

        // Return to TABBED host: top padding removed again, tabbed height unchanged.
        mMediator.setParent(Host.TABBED);
        assertTrue(mModel.get(BottomBarProperties.IS_TOP_PADDING_REMOVED));
        verify(mVisibilityDelegate, never()).onHeightChanged(56);

        // Switch back to TOP toolbar: top padding restored, height is 56px.
        clearInvocations(mVisibilityDelegate);
        mToolbarPositionSupplier.set(ControlsPosition.TOP);
        assertFalse(mModel.get(BottomBarProperties.IS_TOP_PADDING_REMOVED));
        verify(mVisibilityDelegate).onHeightChanged(56);
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ANDROID_BOTTOM_BAR + ":remove_top_padding_for_bottom_omnibox/false")
    public void testToolbarPositionChanged_RemoveTopPaddingForBottomOmniboxDisabled() {
        createMediator();
        assertNotNull(mMediator);

        assertFalse(mModel.get(BottomBarProperties.IS_TOP_PADDING_REMOVED));
        verify(mVisibilityDelegate).onHeightChanged(56);

        clearInvocations(mVisibilityDelegate);
        mToolbarPositionSupplier.set(ControlsPosition.BOTTOM);
        assertFalse(mModel.get(BottomBarProperties.IS_TOP_PADDING_REMOVED));
        verify(mVisibilityDelegate).onHeightChanged(56);
    }

    @Test
    public void testEducation_MaybeShowCalled() {
        createMediator(true, false);
        assertNotNull(mMediator);
        when(mEducationCoordinator.wouldShow(mProfile)).thenReturn(true);
        mOmniboxFocusStateSupplier.set(false); // visible

        assertTrue(mMediator.maybeClaimPromoSlotForEducation(mProfile));
        mMediator.onStartupPromoFlowFinished(true);

        verify(mEducationCoordinator).maybeShow();
    }

    @Test
    public void testEducation_WouldShowTooltips_MaybeShowCalledWithoutClaimingSlot() {
        createMediator(true, false);
        assertNotNull(mMediator);
        when(mEducationCoordinator.wouldShowTooltips(mProfile)).thenReturn(true);
        mOmniboxFocusStateSupplier.set(false); // visible

        // Even without claiming the promo slot, maybeShow should be called
        // because there are pending tooltips that should bypass the startup queue.
        mMediator.onStartupPromoFlowFinished(false);
        verify(mEducationCoordinator).maybeShow();
    }

    @Test
    public void testEducation_WouldShowTooltips_Incognito_NotCalled() {
        when(mTab.isOffTheRecord()).thenReturn(true);
        createMediator(true, false);
        assertNotNull(mMediator);
        when(mEducationCoordinator.wouldShowTooltips(mProfile)).thenReturn(true);
        mOmniboxFocusStateSupplier.set(false); // visible

        mMediator.onStartupPromoFlowFinished(false);
        verify(mEducationCoordinator, never()).maybeShow();
    }

    @Test
    public void testEducation_NotClaimed_NotCalled() {
        createMediator(true, false);
        assertNotNull(mMediator);
        mOmniboxFocusStateSupplier.set(false); // visible

        mMediator.onStartupPromoFlowFinished(false);
        verify(mEducationCoordinator, never()).maybeShow();
    }

    @Test
    public void testEducation_Incognito_NotCalled() {
        when(mTab.isOffTheRecord()).thenReturn(true);
        createMediator(true, false);
        assertNotNull(mMediator);
        when(mEducationCoordinator.wouldShow(mProfile)).thenReturn(true);
        mOmniboxFocusStateSupplier.set(false); // visible

        assertTrue(mMediator.maybeClaimPromoSlotForEducation(mProfile));
        mMediator.onStartupPromoFlowFinished(true);

        verify(mEducationCoordinator, never()).maybeShow();
    }

    @Test
    public void testEducation_NotifiedWhenHidden() {
        createMediator(true, false);
        mOmniboxFocusStateSupplier.set(false); // visible
        clearInvocations(mEducationCoordinator);

        mOmniboxFocusStateSupplier.set(true); // invisible
        verify(mEducationCoordinator).onBottomBarHidden();
        verify(mEducationCoordinator, never()).dismiss();
    }

    @Test
    public void testEducation_NullTab_NotCalled() {
        mTabSupplier.set(null);
        createMediator(true, false);
        assertNotNull(mMediator);
        when(mEducationCoordinator.wouldShow(mProfile)).thenReturn(true);
        mOmniboxFocusStateSupplier.set(false); // visible

        assertTrue(mMediator.maybeClaimPromoSlotForEducation(mProfile));
        mMediator.onStartupPromoFlowFinished(true);

        verify(mEducationCoordinator, never()).maybeShow();
    }

    @Test
    public void testEducation_DestroyDoesNotDestroyCoordinator() {
        createMediator(true, false);
        assertNotNull(mMediator);
        mMediator.destroy();
        verify(mEducationCoordinator, never()).destroy();
    }

    private void createMediator() {
        createMediator(false, true);
    }

    private void createMediator(boolean withEducation, boolean finishStartupFlow) {
        mMediator =
                new BottomBarMediator(
                        mContext,
                        mModel,
                        mButtonManager,
                        mThemeColorProvider,
                        mHomepageEnabledSupplier,
                        mVisibilityDelegate,
                        mProfileSupplier,
                        mOmniboxFocusStateSupplier,
                        mLayoutStateProvider,
                        mToolbarPositionSupplier,
                        withEducation ? mEducationCoordinator : null,
                        mTabSupplier);
        if (finishStartupFlow) {
            mMediator.onStartupPromoFlowFinished(false);
        }
    }
}
