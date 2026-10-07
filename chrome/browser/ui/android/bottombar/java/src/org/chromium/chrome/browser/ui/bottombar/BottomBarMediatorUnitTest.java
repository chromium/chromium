// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
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
import android.view.View;

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

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedServiceFactory;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.android.bars_common.IphIntent;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager.Host;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics.GlicIneligibilityReason;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
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
    @Mock private BottomBarPromoDialogCoordinator mPromoDialogCoordinator;
    @Mock private Tracker mTracker;
    @Mock private ActionRegistry mActionRegistry;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private Context mContext;
    @Mock private Resources mResources;
    @Mock private LayoutStateProvider mLayoutStateProvider;

    @Captor private ArgumentCaptor<BottomBarButtonManager.Listener> mButtonManagerListenerCaptor;

    @Captor
    private ArgumentCaptor<GlicKeyedService.AllowedChangedObserver> mAllowedChangedObserverCaptor;

    private SettableNullableObservableSupplier<Profile> mProfileSupplier;
    private OneshotSupplierImpl<String> mCountrySupplier;

    private SettableNonNullObservableSupplier<Boolean> mHomepageEnabledSupplier;
    private SettableNonNullObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private SettableNullableObservableSupplier<PropertyModel> mGlicActionSupplier;
    private SettableNullableObservableSupplier<PropertyModel> mNewTabActionSupplier;
    private PropertyModel mModel;
    private final View mView = new View(ContextUtils.getApplicationContext());
    private @Nullable BottomBarMediator mMediator;

    @Before
    public void setUp() {
        mHomepageEnabledSupplier = ObservableSuppliers.createNonNull(false);
        mOmniboxFocusStateSupplier = ObservableSuppliers.createNonNull(false);
        mProfileSupplier = ObservableSuppliers.createNullable();
        mProfileSupplier.set(mProfile);
        mCountrySupplier = new OneshotSupplierImpl<>();
        mCountrySupplier.set("us");
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        TrackerFactory.setTrackerForTests(mTracker);
        mModel = new PropertyModel(BottomBarProperties.ALL_KEYS);
        when(mThemeColorProvider.getBrandedColorScheme())
                .thenReturn(BrandedColorScheme.APP_DEFAULT);
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingJniMock);
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);

        when(mPromoDialogCoordinator.maybeShowPromoDialog(any())).thenReturn(true);

        mGlicActionSupplier = ObservableSuppliers.createNullable();
        mNewTabActionSupplier = ObservableSuppliers.createNullable();
        when(mActionRegistry.get(ActionId.GLIC)).thenReturn(mGlicActionSupplier);
        when(mActionRegistry.get(ActionId.NEW_TAB)).thenReturn(mNewTabActionSupplier);

        when(mContext.getResources()).thenReturn(mResources);
        when(mResources.getDimensionPixelSize(R.dimen.bottom_bar_new_tab_background_radius))
                .thenReturn(12);
        when(mResources.getDimensionPixelSize(R.dimen.bottom_bar_new_tab_background_size))
                .thenReturn(40);
        when(mResources.getDimensionPixelSize(R.dimen.bottom_bar_button_highlight_radius))
                .thenReturn(20);
    }

    @After
    public void tearDown() {
        if (mMediator != null) {
            mMediator.destroy();
        }
        TrackerFactory.setTrackerForTests(null);
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
    public void testIphOrchestrationFlow_PromoAccepted_ChainsGlicToNewTabIph() {
        PropertyModel glicModel = new PropertyModel.Builder(ActionProperties.ALL_KEYS).build();
        PropertyModel newTabModel = new PropertyModel.Builder(ActionProperties.ALL_KEYS).build();
        mGlicActionSupplier.set(glicModel);
        mNewTabActionSupplier.set(newTabModel);

        mModel.set(BottomBarProperties.IS_EXTRA_BUTTON_VISIBLE, true);

        createMediator();
        assertNotNull(mMediator);

        mMediator.onPromoDialogAccepted();

        IphIntent glicIph = glicModel.get(ActionProperties.IPH_INTENT);
        assertNotNull(glicIph);
        assertEquals(FeatureConstants.ANDROID_BOTTOM_BAR_GLIC, glicIph.getFeatureNameForTesting());
        assertFalse(Boolean.TRUE.equals(glicModel.get(ActionProperties.IS_SELECTED)));

        // Verify New Tab IPH is not set before Glic IPH is dismissed.
        assertNull(newTabModel.get(ActionProperties.IPH_INTENT));

        glicIph.tryShow(mView, mUserEducationHelper);

        ArgumentCaptor<IphCommand> commandCaptor = ArgumentCaptor.forClass(IphCommand.class);
        verify(mUserEducationHelper, times(1)).requestShowIph(commandCaptor.capture());

        IphCommand command = commandCaptor.getValue();
        assertNotNull(command);
        assertEquals(FeatureConstants.ANDROID_BOTTOM_BAR_GLIC, command.featureName);
        assertNotNull(command.onShowCallback);
        assertNotNull(command.onDismissCallback);
        assertNotNull(command.highlightParams);
        assertEquals(HighlightShape.RECTANGLE, command.highlightParams.getShape());
        assertTrue(command.highlightParams.getBoundsRespectPadding());
        assertEquals(20, command.highlightParams.getCornerRadius());

        // Verify GLIC IPH Shown metric
        HistogramWatcher glicShownWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomBar.IPH.Glic.Event", BottomBarMetrics.IphEvent.SHOWN)
                        .build();
        command.onShowCallback.run();
        glicShownWatcher.assertExpected();

        // Verify GLIC IPH Dismissed metric
        HistogramWatcher glicDismissedWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomBar.IPH.Glic.Event",
                                BottomBarMetrics.IphEvent.DISMISSED)
                        .build();
        command.onDismissCallback.run();
        glicDismissedWatcher.assertExpected();

        IphIntent newTabIph = newTabModel.get(ActionProperties.IPH_INTENT);
        assertNotNull(newTabIph);
        assertEquals(
                FeatureConstants.ANDROID_BOTTOM_BAR_NEW_TAB, newTabIph.getFeatureNameForTesting());

        newTabIph.tryShow(mView, mUserEducationHelper);
        ArgumentCaptor<IphCommand> newTabCommandCaptor = ArgumentCaptor.forClass(IphCommand.class);
        verify(mUserEducationHelper, times(2)).requestShowIph(newTabCommandCaptor.capture());
        IphCommand newTabCommand = newTabCommandCaptor.getAllValues().get(1);
        assertNotNull(newTabCommand);
        assertEquals(FeatureConstants.ANDROID_BOTTOM_BAR_NEW_TAB, newTabCommand.featureName);
        assertNotNull(newTabCommand.onShowCallback);
        assertNotNull(newTabCommand.onDismissCallback);
        assertNotNull(newTabCommand.highlightParams);
        assertEquals(HighlightShape.RECTANGLE, newTabCommand.highlightParams.getShape());
        assertTrue(newTabCommand.highlightParams.getBoundsRespectPadding());
        assertEquals(20, newTabCommand.highlightParams.getCornerRadius());

        // Verify New Tab IPH Shown metric
        HistogramWatcher newTabShownWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomBar.IPH.NewTab.Event",
                                BottomBarMetrics.IphEvent.SHOWN)
                        .build();
        newTabCommand.onShowCallback.run();
        newTabShownWatcher.assertExpected();

        // Verify New Tab IPH Dismissed metric
        HistogramWatcher newTabDismissedWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomBar.IPH.NewTab.Event",
                                BottomBarMetrics.IphEvent.DISMISSED)
                        .build();
        newTabCommand.onDismissCallback.run();
        newTabDismissedWatcher.assertExpected();
    }

    @Test
    public void testNewTabIphHighlight_WithCenteredButton() {
        when(mButtonManager.hasCenteredButton()).thenReturn(true);
        PropertyModel glicModel = new PropertyModel.Builder(ActionProperties.ALL_KEYS).build();
        PropertyModel newTabModel = new PropertyModel.Builder(ActionProperties.ALL_KEYS).build();
        mGlicActionSupplier.set(glicModel);
        mNewTabActionSupplier.set(newTabModel);

        mModel.set(BottomBarProperties.IS_EXTRA_BUTTON_VISIBLE, true);

        createMediator();
        assertNotNull(mMediator);

        mMediator.onPromoDialogAccepted();

        IphIntent glicIph = glicModel.get(ActionProperties.IPH_INTENT);
        assertNotNull(glicIph);
        glicIph.tryShow(mView, mUserEducationHelper);

        ArgumentCaptor<IphCommand> commandCaptor = ArgumentCaptor.forClass(IphCommand.class);
        verify(mUserEducationHelper, times(1)).requestShowIph(commandCaptor.capture());
        IphCommand command = commandCaptor.getValue();
        assertNotNull(command);

        // Dismiss Glic IPH to chain to New Tab IPH.
        command.onDismissCallback.run();

        IphIntent newTabIph = newTabModel.get(ActionProperties.IPH_INTENT);
        assertNotNull(newTabIph);

        newTabIph.tryShow(mView, mUserEducationHelper);
        ArgumentCaptor<IphCommand> newTabCommandCaptor = ArgumentCaptor.forClass(IphCommand.class);
        verify(mUserEducationHelper, times(2)).requestShowIph(newTabCommandCaptor.capture());
        IphCommand newTabCommand = newTabCommandCaptor.getAllValues().get(1);
        assertNotNull(newTabCommand);
        assertEquals(FeatureConstants.ANDROID_BOTTOM_BAR_NEW_TAB, newTabCommand.featureName);
        assertNotNull(newTabCommand.highlightParams);
        assertEquals(HighlightShape.RECTANGLE, newTabCommand.highlightParams.getShape());
        assertTrue(newTabCommand.highlightParams.getBoundsRespectPadding());

        // Corner radius should be 12 (from bottom_bar_new_tab_background_radius) because
        // hasCenteredButton is true.
        assertEquals(12, newTabCommand.highlightParams.getCornerRadius());
    }

    @Test
    public void testLifetimeTeardown_NullsOutIphIntentsInRegistry() {
        PropertyModel glicModel = new PropertyModel.Builder(ActionProperties.ALL_KEYS).build();
        PropertyModel newTabModel = new PropertyModel.Builder(ActionProperties.ALL_KEYS).build();
        mGlicActionSupplier.set(glicModel);
        mNewTabActionSupplier.set(newTabModel);

        createMediator();
        assertNotNull(mMediator);

        // Simulate accepting the promo dialog to populate IPH intents in the action models.
        mMediator.onPromoDialogAccepted();
        assertNotNull(glicModel.get(ActionProperties.IPH_INTENT));

        // Verify that destroying the mediator cleans up (nulls out) the IPH intents in the action
        // registry.
        mMediator.destroy();
        mMediator = null;

        assertNull(glicModel.get(ActionProperties.IPH_INTENT));
        assertNull(newTabModel.get(ActionProperties.IPH_INTENT));
    }

    @Test
    public void testNewTabIphCaching_GlicNotVisible() {
        PropertyModel newTabModel = new PropertyModel.Builder(ActionProperties.ALL_KEYS).build();
        mNewTabActionSupplier.set(newTabModel);

        // Create mediator with GLIC not visible, which triggers New Tab IPH.
        mModel.set(BottomBarProperties.IS_EXTRA_BUTTON_VISIBLE, false);
        createMediator();
        assertNotNull(mMediator);

        IphIntent firstIntent = newTabModel.get(ActionProperties.IPH_INTENT);
        assertNotNull(firstIntent);
        assertEquals(
                FeatureConstants.ANDROID_BOTTOM_BAR_NEW_TAB,
                firstIntent.getFeatureNameForTesting());

        IphIntent secondIntent = newTabModel.get(ActionProperties.IPH_INTENT);
        assertNotNull(secondIntent);
        assertEquals(firstIntent, secondIntent);
    }

    @Test
    public void testIphOrchestration_NewTabFirst_ThenGlicPromo() {
        PropertyModel glicModel = new PropertyModel.Builder(ActionProperties.ALL_KEYS).build();
        PropertyModel newTabModel = new PropertyModel.Builder(ActionProperties.ALL_KEYS).build();
        mGlicActionSupplier.set(glicModel);
        mNewTabActionSupplier.set(newTabModel);

        // Create mediator with GLIC not visible, which triggers New Tab IPH.
        mModel.set(BottomBarProperties.IS_EXTRA_BUTTON_VISIBLE, false);
        createMediator();
        assertNotNull(mMediator);

        IphIntent initialNewTabIph = newTabModel.get(ActionProperties.IPH_INTENT);
        assertNotNull(initialNewTabIph);
        assertEquals(
                FeatureConstants.ANDROID_BOTTOM_BAR_NEW_TAB,
                initialNewTabIph.getFeatureNameForTesting());

        // Simulate showing the initial New Tab IPH.
        assertTrue(initialNewTabIph.tryShow(mView, mUserEducationHelper));
        verify(mUserEducationHelper, times(1)).requestShowIph(any());

        // GLIC button becomes visible.
        verify(mButtonManager).setListener(mButtonManagerListenerCaptor.capture());
        BottomBarButtonManager.Listener listener = mButtonManagerListenerCaptor.getValue();
        mModel.set(BottomBarProperties.IS_EXTRA_BUTTON_VISIBLE, true);
        listener.onButtonVisibilityChanged(ActionId.GLIC, true);

        // maybeShowPromoDialog is now centralized.
        mMediator.onPromoDialogAccepted();
        IphIntent glicIph = glicModel.get(ActionProperties.IPH_INTENT);
        assertNotNull(glicIph);
        assertEquals(FeatureConstants.ANDROID_BOTTOM_BAR_GLIC, glicIph.getFeatureNameForTesting());

        // Showing the GLIC IPH.
        assertTrue(glicIph.tryShow(mView, mUserEducationHelper));
        ArgumentCaptor<IphCommand> commandCaptor = ArgumentCaptor.forClass(IphCommand.class);
        verify(mUserEducationHelper, times(2)).requestShowIph(commandCaptor.capture());
        IphCommand glicCommand = commandCaptor.getAllValues().get(1);
        assertEquals(FeatureConstants.ANDROID_BOTTOM_BAR_GLIC, glicCommand.featureName);

        glicCommand.onDismissCallback.run();

        IphIntent chainedNewTabIph = newTabModel.get(ActionProperties.IPH_INTENT);
        assertNotNull(chainedNewTabIph);

        // Because the New Tab IPH intent is cached, it should be the exact same instance.
        assertEquals(initialNewTabIph, chainedNewTabIph);

        // Verify that trying to show it a second time is blocked.
        assertFalse(chainedNewTabIph.tryShow(mView, mUserEducationHelper));
        verify(mUserEducationHelper, times(2)).requestShowIph(any());
    }

    @Test
    public void testStartupPromoFlowFinished_PromoShown_DefersIph() {
        PropertyModel newTabModel = new PropertyModel.Builder(ActionProperties.ALL_KEYS).build();
        mNewTabActionSupplier.set(newTabModel);
        mModel.set(BottomBarProperties.IS_EXTRA_BUTTON_VISIBLE, false);

        // Create mediator without calling onStartupPromoFlowFinished immediately.
        mMediator =
                new BottomBarMediator(
                        mContext,
                        mModel,
                        mButtonManager,
                        mThemeColorProvider,
                        mHomepageEnabledSupplier,
                        mVisibilityDelegate,
                        mProfileSupplier,
                        mCountrySupplier,
                        mOmniboxFocusStateSupplier,
                        mPromoDialogCoordinator,
                        mActionRegistry,
                        mLayoutStateProvider);

        // Bottom bar is visible, but startup promo flow hasn't finished. IPH should NOT be shown.
        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        assertNull(newTabModel.get(ActionProperties.IPH_INTENT));

        // Simulate startup promo flow finished with promoShown = true.
        mMediator.onStartupPromoFlowFinished(/* promoShown= */ true);

        // IPH should still NOT be shown immediately because a promo was shown.
        assertNull(newTabModel.get(ActionProperties.IPH_INTENT));

        // Simulate a visibility change (e.g. bottom bar becomes invisible and then visible again).
        mOmniboxFocusStateSupplier.set(true); // invisible
        assertFalse(mModel.get(BottomBarProperties.IS_VISIBLE));

        mOmniboxFocusStateSupplier.set(false); // visible again
        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));

        // Now the IPH SHOULD be shown because the bottom bar became visible after the startup promo
        // flow finished.
        IphIntent newTabIph = newTabModel.get(ActionProperties.IPH_INTENT);
        assertNotNull(newTabIph);
        assertEquals(
                FeatureConstants.ANDROID_BOTTOM_BAR_NEW_TAB, newTabIph.getFeatureNameForTesting());
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
    public void testGlicInitiallyDisabled_CountryResolvesToDisallowed_UnregistersObserver() {
        mCountrySupplier = new OneshotSupplierImpl<>();
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);

        createMediator();

        verify(mGlicKeyedService).addAllowedChangedObserver(any());

        mCountrySupplier.set("fr");
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mGlicKeyedService).removeAllowedChangedObserver(any());
        verify(mButtonManager, never()).setButtonVisibility(ActionId.GLIC, true);
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
    public void testActionNone_NoObserversRegistered() {
        mCountrySupplier = new OneshotSupplierImpl<>();
        mCountrySupplier.set("fr");
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);

        createMediator();

        assertNotNull(mMediator);
        verify(mButtonManager).setButtonVisibility(ActionId.GLIC, false);
        verify(mGlicKeyedService, never()).addAllowedChangedObserver(any());
    }

    @Test
    public void testDeferredCandidateResolution_ProfileFirst_CountrySecond() {
        mProfileSupplier.set(mProfile);
        mCountrySupplier = new OneshotSupplierImpl<>();
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);

        createMediator();

        // While country is null, extra buttons should remain hidden and no dynamic observers
        // attached.
        verify(mButtonManager).setButtonVisibility(ActionId.GLIC, false);
        verify(mGlicKeyedService, never()).addAllowedChangedObserver(any());

        // Country arrives.
        mCountrySupplier.set("us");
        RobolectricUtil.runAllBackgroundAndUi();

        // Candidate resolves to GLIC and becomes visible.
        verify(mButtonManager).setButtonVisibility(ActionId.GLIC, true);
        verify(mGlicKeyedService).addAllowedChangedObserver(any());
    }

    @Test
    public void testDeferredCandidateResolution_CountryFirst_ProfileSecond() {
        mProfileSupplier.set(null);
        mCountrySupplier = new OneshotSupplierImpl<>();
        mCountrySupplier.set("us");
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
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":bypass_glic_geofencing/true")
    public void testDeferredCandidateResolution_BypassGeofencing_NullCountry() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        mCountrySupplier = new OneshotSupplierImpl<>();

        createMediator();

        // Geofencing is bypassed -> candidate resolves to GLIC immediately despite null country.
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

    private void createMediator() {
        mMediator =
                new BottomBarMediator(
                        mContext,
                        mModel,
                        mButtonManager,
                        mThemeColorProvider,
                        mHomepageEnabledSupplier,
                        mVisibilityDelegate,
                        mProfileSupplier,
                        mCountrySupplier,
                        mOmniboxFocusStateSupplier,
                        mPromoDialogCoordinator,
                        mActionRegistry,
                        mLayoutStateProvider);
        mMediator.onStartupPromoFlowFinished(false);
    }
}
