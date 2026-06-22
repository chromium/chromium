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

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.android.bars_common.IphIntent;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link BottomBarMediator}. */
@NullMarked
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR})
public class BottomBarMediatorUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private BottomBarMediator.VisibilityDelegate mVisibilityDelegate;
    @Mock private Tab mTab;
    @Mock private Profile mProfile;
    @Mock private BottomBarButtonManager mButtonManager;
    @Mock private GlicEnabling.Natives mGlicEnablingJniMock;
    @Mock private GlicKeyedService mGlicKeyedService;
    @Mock private BottomBarPromoDialogCoordinator mPromoDialogCoordinator;
    @Mock private Tracker mTracker;
    @Mock private ActionRegistry mActionRegistry;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private View mView;
    @Mock private Context mContext;
    @Mock private Resources mResources;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor private ArgumentCaptor<BottomBarButtonManager.Listener> mButtonManagerListenerCaptor;

    @Captor
    private ArgumentCaptor<GlicKeyedService.AllowedChangedObserver> mAllowedChangedObserverCaptor;

    private SettableNullableObservableSupplier<Profile> mProfileSupplier;

    private SettableNullableObservableSupplier<Tab> mTabSupplier;
    private SettableNonNullObservableSupplier<Boolean> mHomepageEnabledSupplier;
    private SettableNonNullObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private SettableNullableObservableSupplier<PropertyModel> mGlicActionSupplier;
    private SettableNullableObservableSupplier<PropertyModel> mNewTabActionSupplier;
    private PropertyModel mModel;
    private @Nullable BottomBarMediator mMediator;

    @Before
    public void setUp() {
        mTabSupplier = ObservableSuppliers.createNullable();
        mHomepageEnabledSupplier = ObservableSuppliers.createNonNull(false);
        mOmniboxFocusStateSupplier = ObservableSuppliers.createNonNull(false);
        mProfileSupplier = ObservableSuppliers.createNullable();
        mProfileSupplier.set(mProfile);
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

        when(mView.getContext()).thenReturn(mContext);
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
    public void testInitialization_WithoutHomeButton_DoesNotObserveHomepage() {
        createMediator(/* shouldIncludeHomeButton= */ false);

        mHomepageEnabledSupplier.set(true);
        verify(mButtonManager, never()).setButtonVisibility(ActionId.HOME_BUTTON, true);
    }

    private void setupTab(GURL url, boolean isIncognito) {
        when(mTab.getUrl()).thenReturn(url);
        when(mTab.isOffTheRecord()).thenReturn(isIncognito);
        mTabSupplier.set(mTab);
        TrackerFactory.setTrackerForTests(mTracker);
    }

    @Test
    public void testConstructor() {
        createMediator(/* shouldIncludeHomeButton= */ true);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);
    }

    @Test
    public void testTabObserverCleanup_OnTabRemoved() {
        setupTab(JUnitTestGURLs.NTP_URL, false);
        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mTab).addObserver(mTabObserverCaptor.capture());

        mTabSupplier.set(null);
        verify(mTab).removeObserver(mTabObserverCaptor.getValue());
        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
    }

    @Test
    public void testVisibilityChange_EmptyUrl() {
        setupTab(GURL.emptyGURL(), false);
        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        mTabObserverCaptor.getValue().onUrlUpdated(mTab);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
    }

    @Test
    public void testVisibilityChange_Ntp_Incognito() {
        setupTab(JUnitTestGURLs.NTP_URL, true);
        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);

        mTabObserverCaptor.getValue().onUrlUpdated(mTab);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);
    }

    @Test
    public void testVisibilityChange_NotNtp() {
        setupTab(JUnitTestGURLs.EXAMPLE_URL, false);
        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);

        mTabObserverCaptor.getValue().onUrlUpdated(mTab);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);
    }

    @Test
    public void testIphOrchestrationFlow_PromoAccepted_ChainsGlicToNewTabIph() {
        PropertyModel glicModel = new PropertyModel.Builder(ActionProperties.ALL_KEYS).build();
        PropertyModel newTabModel = new PropertyModel.Builder(ActionProperties.ALL_KEYS).build();
        mGlicActionSupplier.set(glicModel);
        mNewTabActionSupplier.set(newTabModel);

        mModel.set(BottomBarProperties.IS_GLIC_BUTTON_VISIBLE, true);

        createMediator(/* shouldIncludeHomeButton= */ true);
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

        mModel.set(BottomBarProperties.IS_GLIC_BUTTON_VISIBLE, true);

        createMediator(/* shouldIncludeHomeButton= */ true);
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

        createMediator(/* shouldIncludeHomeButton= */ true);
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
        mModel.set(BottomBarProperties.IS_GLIC_BUTTON_VISIBLE, false);
        createMediator(/* shouldIncludeHomeButton= */ true);
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
        mModel.set(BottomBarProperties.IS_GLIC_BUTTON_VISIBLE, false);
        createMediator(/* shouldIncludeHomeButton= */ true);
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
        mModel.set(BottomBarProperties.IS_GLIC_BUTTON_VISIBLE, true);
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
        mModel.set(BottomBarProperties.IS_GLIC_BUTTON_VISIBLE, false);

        // Create mediator without calling onStartupPromoFlowFinished immediately.
        mMediator =
                new BottomBarMediator(
                        mContext,
                        mModel,
                        mButtonManager,
                        mThemeColorProvider,
                        mTabSupplier,
                        mHomepageEnabledSupplier,
                        mVisibilityDelegate,
                        /* shouldIncludeHomeButton= */ true,
                        mProfileSupplier,
                        mOmniboxFocusStateSupplier,
                        mPromoDialogCoordinator,
                        mActionRegistry);

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

    private void createMediator(boolean shouldIncludeHomeButton) {
        mMediator =
                new BottomBarMediator(
                        mContext,
                        mModel,
                        mButtonManager,
                        mThemeColorProvider,
                        mTabSupplier,
                        mHomepageEnabledSupplier,
                        mVisibilityDelegate,
                        shouldIncludeHomeButton,
                        mProfileSupplier,
                        mOmniboxFocusStateSupplier,
                        mPromoDialogCoordinator,
                        mActionRegistry);
        mMediator.onStartupPromoFlowFinished(false);
    }
}
