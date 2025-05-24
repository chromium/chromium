// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Canvas;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab_ui.TabModelDotInfo;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.base.TestActivity;

import java.util.HashSet;
import java.util.Set;

/** Unit tests for {@link ToggleTabStackButtonCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class ToggleTabStackButtonCoordinatorTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private ToggleTabStackButton mToggleTabStackButton;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private OnClickListener mOnClickListener;
    @Mock private OnLongClickListener mOnLongClickListener;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mStandardTabModel;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private TopUiThemeColorProvider mTopUIThemeProvider;
    @Mock private IncognitoStateProvider mIncognitoStateProvider;

    @Captor private ArgumentCaptor<IphCommand> mIphCommandCaptor;

    private Activity mActivity;
    private final ObservableSupplierImpl<TabModelDotInfo> mNotificationDotSupplier =
            new ObservableSupplierImpl<>(TabModelDotInfo.HIDE);
    private final OneshotSupplierImpl<Boolean> mPromoShownOneshotSupplier =
            new OneshotSupplierImpl<>();
    private final ObservableSupplierImpl<Integer> mTabCountSupplier =
            new ObservableSupplierImpl<>(0);

    private boolean mOverviewOpen;
    private Set<LayoutStateProvider.LayoutStateObserver> mLayoutStateObserverSet;
    private OneshotSupplierImpl<LayoutStateProvider> mLayoutSateProviderOneshotSupplier;

    private ToggleTabStackButtonCoordinator mCoordinator;
    private ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

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
        when(mTabModelSelector.getCurrentModel()).thenReturn(mStandardTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        when(mStandardTabModel.isIncognitoBranded()).thenReturn(false);
        when(mIncognitoTabModel.isIncognitoBranded()).thenReturn(true);
        when(mIncognitoTabModel.getCount()).thenReturn(0);

        // Defaults most test cases expect, can be overridden by each test though.
        when(mToggleTabStackButton.isShown()).thenReturn(true);
        when(mIncognitoStateProvider.isIncognitoSelected()).thenReturn(false);
        mCoordinator = newToggleTabStackButtonCoordinator(mToggleTabStackButton);
    }

    private ToggleTabStackButtonCoordinator newToggleTabStackButtonCoordinator(
            ToggleTabStackButton toggleTabStackButton) {
        ToggleTabStackButtonCoordinator coordinator =
                new ToggleTabStackButtonCoordinator(
                        mActivity,
                        toggleTabStackButton,
                        mUserEducationHelper,
                        mPromoShownOneshotSupplier,
                        mLayoutSateProviderOneshotSupplier,
                        new ObservableSupplierImpl<>(),
                        mTabModelSelectorSupplier,
                        mTopUIThemeProvider,
                        mIncognitoStateProvider);

        coordinator.initializeWithNative(
                mOnClickListener,
                mOnLongClickListener,
                mTabCountSupplier,
                /* archivedTabCountSupplier= */ null,
                mNotificationDotSupplier,
                () -> {},
                () -> {});
        return coordinator;
    }

    private void showOverviewMode() {
        mOverviewOpen = true;
        for (LayoutStateProvider.LayoutStateObserver observer : mLayoutStateObserverSet) {
            observer.onStartedShowing(/* layoutType= */ LayoutType.TAB_SWITCHER);
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

    private IphCommand verifyIphShown() {
        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());
        reset(mUserEducationHelper);
        return mIphCommandCaptor.getValue();
    }

    private void verifyIphNotShown() {
        verify(mUserEducationHelper, never()).requestShowIph(any());
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
        IphCommand iphCommand = verifyIphShown();

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
        IphCommand iphCommand = verifyIphShown();

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
        IphCommand iphCommand = verifyIphShown();

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
        IphCommand iphCommand = verifyIphShown();

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

        when(mIncognitoStateProvider.isIncognitoSelected()).thenReturn(true);
        mCoordinator.handlePageLoadFinished();
        verifyIphNotShown();

        when(mIncognitoStateProvider.isIncognitoSelected()).thenReturn(false);
        mCoordinator.handlePageLoadFinished();
        IphCommand iphCommand = verifyIphShown();
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

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_INCOGNITO_MIGRATION)
    public void testSwitchToIncognitoIphIsShown() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(
                        /* toggleTabStackButton= */ mToggleTabStackButton);
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        mPromoShownOneshotSupplier.set(false);

        when(mIncognitoTabModel.getCount()).thenReturn(1);

        // Standard model with incognito tabs - show switch into incognito IPH.
        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        IphCommand iphCommand = verifyIphShown();
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
        when(mTabModelSelector.getCurrentModel()).thenReturn(mIncognitoTabModel);
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
    public void testDraw() {
        Canvas canvas = new Canvas();
        mCoordinator.draw(mToggleTabStackButton, canvas);
        verify(mToggleTabStackButton).drawTabSwitcherAnimationOverlay(canvas);
    }

    @Test
    public void testTabModelDotInfoIph() {
        String groupTitle = "Vacation";
        mNotificationDotSupplier.set(new TabModelDotInfo(true, groupTitle));

        IphCommand iphCommand = verifyIphShown();
        assertTrue(iphCommand.contentString.contains(groupTitle));
    }
}
