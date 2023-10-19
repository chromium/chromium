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

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.user_education.IPHCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.test.util.browser.Features;

import java.util.HashSet;
import java.util.Set;

/** Unit tests for ToggleTabStackButtonCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class ToggleTabStackButtonCoordinatorTest {
    @Rule public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Mock private Context mContext;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private ToggleTabStackButton mToggleTabStackButton;
    @Mock private android.content.res.Resources mResources;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private Callback<Boolean> mSetNewTabButtonHighlightCallback;

    @Captor private ArgumentCaptor<IPHCommand> mIPHCommandCaptor;

    private boolean mIsIncognito;
    private boolean mOverviewOpen;
    private final OneshotSupplierImpl<Boolean> mPromoShownOneshotSupplier =
            new OneshotSupplierImpl<>();
    private Set<LayoutStateProvider.LayoutStateObserver> mLayoutStateObserverSet;

    private OneshotSupplierImpl<LayoutStateProvider> mLayoutSateProviderOneshotSupplier;

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

        // Defaults most test cases expect, can be overridden by each test though.
        when(mToggleTabStackButton.isShown()).thenReturn(true);
        mIsIncognito = false;
    }

    private ToggleTabStackButtonCoordinator newToggleTabStackButtonCoordinator(
            ToggleTabStackButton toggleTabStackButton) {
        return new ToggleTabStackButtonCoordinator(
                mContext,
                toggleTabStackButton,
                mUserEducationHelper,
                () -> mIsIncognito,
                mPromoShownOneshotSupplier,
                mLayoutSateProviderOneshotSupplier,
                mSetNewTabButtonHighlightCallback,
                new ObservableSupplierImpl<>());
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

    private void verifyNtpButtonHighlightChanged(boolean expectedHighlight) {
        verify(mSetNewTabButtonHighlightCallback).onResult(expectedHighlight);
        reset(mSetNewTabButtonHighlightCallback);
    }

    private void verifyNtpButtonHighlightNotChanged() {
        verify(mSetNewTabButtonHighlightCallback, never()).onResult(any());
        reset(mSetNewTabButtonHighlightCallback);
    }

    @Test
    public void testOverviewBehaviorAvailableDuringConstruction() {
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(
                        /* toggleTabStackButton= */ mToggleTabStackButton);
        Assert.assertEquals("Should have 1 overview observer", 1, mLayoutStateObserverSet.size());

        toggleTabStackButtonCoordinator.destroy();
        Assert.assertTrue("Should have no overview observers", mLayoutStateObserverSet.isEmpty());
    }

    @Test
    public void testOverviewBehaviorAvailableAfterDestroy() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(
                        /* toggleTabStackButton= */ mToggleTabStackButton);
        toggleTabStackButtonCoordinator.destroy();

        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        Assert.assertTrue("Should have no overview observers", mLayoutStateObserverSet.isEmpty());
    }

    @Test
    public void testDestroyDuringIph() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(
                        /* toggleTabStackButton= */ mToggleTabStackButton);
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        mPromoShownOneshotSupplier.set(false);

        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        IPHCommand iphCommand = verifyIphShown();

        iphCommand.onShowCallback.run();
        Assert.assertEquals("Should have 1 overview observer", 1, mLayoutStateObserverSet.size());

        toggleTabStackButtonCoordinator.destroy();
        Assert.assertTrue("Should have no overview observers", mLayoutStateObserverSet.isEmpty());
    }

    @Test
    public void testIphAndOverviewHighlight() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(
                        /* toggleTabStackButton= */ mToggleTabStackButton);
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        mPromoShownOneshotSupplier.set(false);

        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        IPHCommand iphCommand = verifyIphShown();

        iphCommand.onShowCallback.run();
        assertEquals(true, toggleTabStackButtonCoordinator.mIphBeingShown);
        verifyNtpButtonHighlightNotChanged();

        showOverviewMode();
        assertEquals(true, toggleTabStackButtonCoordinator.mIphBeingShown);
        verifyNtpButtonHighlightChanged(true);

        iphCommand.onDismissCallback.run();
        assertEquals(false, toggleTabStackButtonCoordinator.mIphBeingShown);
        verifyNtpButtonHighlightNotChanged();
        hideOverviewMode();
        assertEquals(false, toggleTabStackButtonCoordinator.mIphBeingShown);
        verifyNtpButtonHighlightChanged(false);
    }

    @Test
    public void testDismissIphBeforeOverview() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(
                        /* toggleTabStackButton= */ mToggleTabStackButton);
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        mPromoShownOneshotSupplier.set(false);

        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        IPHCommand iphCommand = verifyIphShown();

        iphCommand.onShowCallback.run();
        assertEquals(true, toggleTabStackButtonCoordinator.mIphBeingShown);
        verifyNtpButtonHighlightNotChanged();

        iphCommand.onDismissCallback.run();
        assertEquals(false, toggleTabStackButtonCoordinator.mIphBeingShown);
        verifyNtpButtonHighlightNotChanged();

        showOverviewMode();
        assertEquals(false, toggleTabStackButtonCoordinator.mIphBeingShown);
        verifyNtpButtonHighlightNotChanged();

        hideOverviewMode();
        assertEquals(false, toggleTabStackButtonCoordinator.mIphBeingShown);
        verifyNtpButtonHighlightNotChanged();
    }

    @Test
    public void testOverviewModeEventsWithoutIph() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(
                        /* toggleTabStackButton= */ mToggleTabStackButton);
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        mPromoShownOneshotSupplier.set(false);

        showOverviewMode();
        assertEquals(false, toggleTabStackButtonCoordinator.mIphBeingShown);
        verifyNtpButtonHighlightNotChanged();

        hideOverviewMode();
        assertEquals(false, toggleTabStackButtonCoordinator.mIphBeingShown);
        verifyNtpButtonHighlightNotChanged();
    }

    @Test
    public void testIphWithNoPageLoad() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(
                        /* toggleTabStackButton= */ mToggleTabStackButton);
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        mPromoShownOneshotSupplier.set(false);

        verifyIphNotShown();
    }

    @Test
    public void testIphWithNoViewButton() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(/* toggleTabStackButton= */ null);
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        mPromoShownOneshotSupplier.set(false);

        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphNotShown();
    }

    @Test
    public void testIphWithNoOverviewModeBehavior() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(
                        /* toggleTabStackButton= */ mToggleTabStackButton);
        mPromoShownOneshotSupplier.set(false);

        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        IPHCommand iphCommand = verifyIphShown();

        iphCommand.onShowCallback.run();
        assertEquals(true, toggleTabStackButtonCoordinator.mIphBeingShown);
        verifyNtpButtonHighlightNotChanged();

        showOverviewMode();
        assertEquals(true, toggleTabStackButtonCoordinator.mIphBeingShown);
        verifyNtpButtonHighlightNotChanged();

        iphCommand.onDismissCallback.run();
        assertEquals(false, toggleTabStackButtonCoordinator.mIphBeingShown);
        verifyNtpButtonHighlightNotChanged();

        hideOverviewMode();
        assertEquals(false, toggleTabStackButtonCoordinator.mIphBeingShown);
        verifyNtpButtonHighlightNotChanged();
    }

    @Test
    public void testIphIncognito() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(
                        /* toggleTabStackButton= */ mToggleTabStackButton);
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        mPromoShownOneshotSupplier.set(false);

        mIsIncognito = true;
        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphNotShown();

        mIsIncognito = false;
        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphShown();
    }

    @Test
    public void testIphIsShown() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(
                        /* toggleTabStackButton= */ mToggleTabStackButton);
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        mPromoShownOneshotSupplier.set(false);

        when(mToggleTabStackButton.isShown()).thenReturn(false);
        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphNotShown();

        when(mToggleTabStackButton.isShown()).thenReturn(true);
        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphShown();
    }

    @Test
    public void testIphShowedPromo() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(
                        /* toggleTabStackButton= */ mToggleTabStackButton);
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);
        mPromoShownOneshotSupplier.set(true);

        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphNotShown();
    }

    @Test
    public void testIphDelayedPromoShown() {
        ToggleTabStackButtonCoordinator toggleTabStackButtonCoordinator =
                newToggleTabStackButtonCoordinator(
                        /* toggleTabStackButton= */ mToggleTabStackButton);
        mLayoutSateProviderOneshotSupplier.set(mLayoutStateProvider);

        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphNotShown();

        mPromoShownOneshotSupplier.set(false);
        toggleTabStackButtonCoordinator.handlePageLoadFinished();
        verifyIphShown();
    }
}
