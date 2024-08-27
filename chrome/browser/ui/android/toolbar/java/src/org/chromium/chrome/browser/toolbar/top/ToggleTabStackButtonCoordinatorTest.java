// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Canvas;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.user_education.IPHCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;

import java.util.HashSet;
import java.util.Set;

/** Unit tests for ToggleTabStackButtonCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class ToggleTabStackButtonCoordinatorTest {

    @Mock private Context mContext;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private ToggleTabStackButton mToggleTabStackButton;
    @Mock private android.content.res.Resources mResources;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private OnClickListener mOnClickListener;
    @Mock private OnLongClickListener mOnLongClickListener;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;

    @Captor private ArgumentCaptor<IPHCommand> mIPHCommandCaptor;

    private boolean mIsIncognito;
    private boolean mOverviewOpen;
    private final OneshotSupplierImpl<Boolean> mPromoShownOneshotSupplier =
            new OneshotSupplierImpl<>();
    private Set<LayoutStateProvider.LayoutStateObserver> mLayoutStateObserverSet;
    private OneshotSupplierImpl<LayoutStateProvider> mLayoutSateProviderOneshotSupplier;
    private ObservableSupplier<Integer> mTabCountSupplier;
    private ObservableSupplierImpl<Integer> mArchivedTabCountSupplier;

    private ToggleTabStackButtonCoordinator mCoordinator;
    private ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mContext.getResources()).thenReturn(mResources);
        doAnswer(invocation -> mOverviewOpen)
                .when(mLayoutStateProvider)
                .isLayoutVisible(LayoutType.TAB_SWITCHER);
        doAnswer(
                        invocation -> {
                            mLayoutStateObserverSet.add(invocation.getArgument(0));
                            return null;
                        })
                .when(mLayoutStateProvider)
                .addObserver(any(LayoutStateProvider.LayoutStateObserver.class));
        doAnswer(
                        invocation -> {
                            mLayoutStateObserverSet.remove(invocation.getArgument(0));
                            return null;
                        })
                .when(mLayoutStateProvider)
                .removeObserver(any(LayoutStateProvider.LayoutStateObserver.class));

        mLayoutStateObserverSet = new HashSet<>();
        mLayoutSateProviderOneshotSupplier = new OneshotSupplierImpl<>();
        mTabModelSelectorSupplier = new ObservableSupplierImpl<>();
        mTabModelSelectorSupplier.set(mTabModelSelector);

        // Defaults most test cases expect, can be overridden by each test though.
        when(mToggleTabStackButton.isShown()).thenReturn(true);
        mIsIncognito = false;
        mCoordinator = newToggleTabStackButtonCoordinator(mToggleTabStackButton);
    }

    private ToggleTabStackButtonCoordinator newToggleTabStackButtonCoordinator(
            ToggleTabStackButton toggleTabStackButton) {
        ToggleTabStackButtonCoordinator coordinator =
                new ToggleTabStackButtonCoordinator(
                        mContext,
                        toggleTabStackButton,
                        mUserEducationHelper,
                        () -> mIsIncognito,
                        mPromoShownOneshotSupplier,
                        mLayoutSateProviderOneshotSupplier,
                        new ObservableSupplierImpl<>(),
                        mTabModelSelectorSupplier);
        coordinator.initializeWithNative(
                mOnClickListener,
                mOnLongClickListener,
                mTabCountSupplier,
                mArchivedTabCountSupplier,
                () -> {},
                () -> {});
        return coordinator;
    }

    private void showOverviewMode() {
        mOverviewOpen = true;
        for (LayoutStateProvider.LayoutStateObserver observer : mLayoutStateObserverSet) {
            observer.onStartedShowing(/* showToolbar= */ LayoutType.TAB_SWITCHER);
        }
        for (LayoutStateProvider.LayoutStateObserver observer : mLayoutStateObserverSet) {
            observer.onFinishedShowing(LayoutType.TAB_SWITCHER);
        }
    }

    private void hideOverviewMode() {
        mOverviewOpen = false;
        for (LayoutStateProvider.LayoutStateObserver observer : mLayoutStateObserverSet) {
            observer.onStartedHiding(LayoutType.TAB_SWITCHER);
        }
        for (LayoutStateProvider.LayoutStateObserver observer : mLayoutStateObserverSet) {
            observer.onFinishedHiding(LayoutType.TAB_SWITCHER);
        }
    }

    private IPHCommand verifyIphShown() {
        verify(mUserEducationHelper).requestShowIPH(mIPHCommandCaptor.capture());
        reset(mUserEducationHelper);
        return mIPHCommandCaptor.getValue();
    }

    private void verifyIphNotShown() {
        verify(mUserEducationHelper, never()).requestShowIPH(any());
        reset(mUserEducationHelper);
    }

    @Test
    public void testOverviewBehaviorAvailableDuringConstruction() {
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        Assert.assertEquals("Should have 1 overview observer", 1, mLayoutStateObserverSet.size());

        mCoordinator.destroy();
        Assert.assertTrue("Should have no overview observers", mLayoutStateObserverSet.isEmpty());
    }

    @Test
    public void testOverviewBehaviorAvailableAfterDestroy() {
        mCoordinator.destroy();

        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        Assert.assertTrue("Should have no overview observers", mLayoutStateObserverSet.isEmpty());
    }

    @Test
    public void testDestroyDuringIph() {
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        mPromoShownOneshotSupplier.set(false);

        mCoordinator.handlePageLoadFinished();
        IPHCommand iphCommand = verifyIphShown();

        iphCommand.onShowCallback.run();
        Assert.assertEquals("Should have 1 overview observer", 1, mLayoutStateObserverSet.size());

        mCoordinator.destroy();
        Assert.assertTrue("Should have no overview observers", mLayoutStateObserverSet.isEmpty());
    }

    @Test
    public void testIphAndOverviewHighlight() {
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        mPromoShownOneshotSupplier.set(false);

        mCoordinator.handlePageLoadFinished();
        IPHCommand iphCommand = verifyIphShown();

        iphCommand.onShowCallback.run();
        assertEquals(true, mCoordinator.mIphBeingShown);

        showOverviewMode();
        assertEquals(true, mCoordinator.mIphBeingShown);

        iphCommand.onDismissCallback.run();
        assertEquals(false, mCoordinator.mIphBeingShown);
        hideOverviewMode();
        assertEquals(false, mCoordinator.mIphBeingShown);
    }

    @Test
    public void testDismissIphBeforeOverview() {
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        mPromoShownOneshotSupplier.set(false);

        mCoordinator.handlePageLoadFinished();
        IPHCommand iphCommand = verifyIphShown();

        iphCommand.onShowCallback.run();
        assertEquals(true, mCoordinator.mIphBeingShown);

        iphCommand.onDismissCallback.run();
        assertEquals(false, mCoordinator.mIphBeingShown);

        showOverviewMode();
        assertEquals(false, mCoordinator.mIphBeingShown);

        hideOverviewMode();
        assertEquals(false, mCoordinator.mIphBeingShown);
    }

    @Test
    public void testOverviewModeEventsWithoutIph() {
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        mPromoShownOneshotSupplier.set(false);

        showOverviewMode();
        assertEquals(false, mCoordinator.mIphBeingShown);

        hideOverviewMode();
        assertEquals(false, mCoordinator.mIphBeingShown);
    }

    @Test
    public void testIphWithNoPageLoad() {
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        mPromoShownOneshotSupplier.set(false);

        verifyIphNotShown();
    }

    @Test
    public void testIphWithNoOverviewModeBehavior() {
        mPromoShownOneshotSupplier.set(false);

        mCoordinator.handlePageLoadFinished();
        IPHCommand iphCommand = verifyIphShown();

        iphCommand.onShowCallback.run();
        assertEquals(true, mCoordinator.mIphBeingShown);

        showOverviewMode();
        assertEquals(true, mCoordinator.mIphBeingShown);

        iphCommand.onDismissCallback.run();
        assertEquals(false, mCoordinator.mIphBeingShown);

        hideOverviewMode();
        assertEquals(false, mCoordinator.mIphBeingShown);
    }

    @Test
    public void testIphIncognito() {
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        mPromoShownOneshotSupplier.set(false);

        mIsIncognito = true;
        mCoordinator.handlePageLoadFinished();
        verifyIphNotShown();

        mIsIncognito = false;
        mCoordinator.handlePageLoadFinished();
        IPHCommand iphCommand = verifyIphShown();
        assertEquals(
                "IPH feature is not as expected.",
                FeatureConstants.TAB_SWITCHER_BUTTON_FEATURE,
                iphCommand.featureName);
        assertEquals(
                "IPH string is not as expected.",
                R.string.iph_tab_switcher_text,
                iphCommand.stringId);
        assertEquals(
                "IPH string is not as expected.",
                R.string.iph_tab_switcher_accessibility_text,
                iphCommand.accessibilityStringId);
    }

    @EnableFeatures(ChromeFeatureList.TAB_STRIP_INCOGNITO_MIGRATION)
    @Test
    public void testSwitchToIncognitoIphIsShown() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(
                        /* toggleTabStackButton= */ mToggleTabStackButton);
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        mPromoShownOneshotSupplier.set(false);

        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mTabModel);
        when(mTabModel.getCount()).thenReturn(1);

        // Standard model with incognito tabs - show switch into incognito IPH.
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        IPHCommand iphCommand = verifyIphShown();
        assertEquals(
                "IPH feature is not as expected.",
                FeatureConstants.TAB_SWITCHER_BUTTON_SWITCH_INCOGNITO,
                iphCommand.featureName);
        assertEquals(
                "IPH string is not as expected.",
                R.string.iph_tab_switcher_switch_into_incognito_text,
                iphCommand.stringId);
        assertEquals(
                "IPH string is not as expected.",
                R.string.iph_tab_switcher_switch_into_incognito_accessibility_text,
                iphCommand.accessibilityStringId);

        // Incognito model - show switch out of incognito IPH.
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        iphCommand = verifyIphShown();
        assertEquals(
                "IPH feature is not as expected.",
                FeatureConstants.TAB_SWITCHER_BUTTON_SWITCH_INCOGNITO,
                iphCommand.featureName);
        assertEquals(
                "IPH string is not as expected.",
                R.string.iph_tab_switcher_switch_out_of_incognito_text,
                iphCommand.stringId);
        assertEquals(
                "IPH string is not as expected.",
                R.string.iph_tab_switcher_switch_out_of_incognito_accessibility_text,
                iphCommand.accessibilityStringId);
    }

    @Test
    public void testIphIsShown() {
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        mPromoShownOneshotSupplier.set(false);

        when(mToggleTabStackButton.isShown()).thenReturn(false);
        mCoordinator.handlePageLoadFinished();
        verifyIphNotShown();

        when(mToggleTabStackButton.isShown()).thenReturn(true);
        mCoordinator.handlePageLoadFinished();
        verifyIphShown();
    }

    @Test
    public void testIphShowedPromo() {
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        mPromoShownOneshotSupplier.set(true);

        mCoordinator.handlePageLoadFinished();
        verifyIphNotShown();
    }

    @Test
    public void testIphDelayedPromoShown() {
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);

        mCoordinator.handlePageLoadFinished();
        verifyIphNotShown();

        mPromoShownOneshotSupplier.set(false);
        mCoordinator.handlePageLoadFinished();
        verifyIphShown();
    }

    @Test
    public void testSetBrandedColorScheme() {
        mCoordinator.setBrandedColorScheme(BrandedColorScheme.DARK_BRANDED_THEME);
        verify(mToggleTabStackButton).setBrandedColorScheme(BrandedColorScheme.DARK_BRANDED_THEME);
    }

    @Test
    public void testDrawTabSwitcherAnimationOverlay() {
        Canvas canvas = new Canvas();
        mCoordinator.drawTabSwitcherAnimationOverlay(mToggleTabStackButton, canvas, 255);
        verify(mToggleTabStackButton).drawTabSwitcherAnimationOverlay(canvas, 255);
    }
}
