// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.desktop_windowing;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.tabstrip.StripVisibilityState;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link TopControlsLockCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, qualifiers = "sw320dp")
public class TopControlsLockCoordinatorTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenario =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private TopControlsStacker mTopControlsStacker;
    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock private AppHeaderState mAppHeaderState;

    @Captor
    private ArgumentCaptor<DesktopWindowStateManager.AppHeaderObserver> mAppHeaderObserverCaptor;

    private Context mContext;
    private SettableNullableObservableSupplier<@StripVisibilityState Integer>
            mTabStripVisibilitySupplier;
    private boolean mCurrentScrollDisabled;

    @Before
    public void setUp() {
        mActivityScenario.getScenario().onActivity(activity -> mContext = activity);
        mTabStripVisibilitySupplier = ObservableSuppliers.createNullable();
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(mAppHeaderState);

        doAnswer(
                        (invocationOnMock) -> {
                            boolean newState = invocationOnMock.getArgument(0);
                            if (newState == mCurrentScrollDisabled) return false;

                            mCurrentScrollDisabled = newState;
                            return true;
                        })
                .when(mTopControlsStacker)
                .setScrollingDisabled(anyBoolean());
    }

    private TopControlsLockCoordinator createCoordinator() {
        return new TopControlsLockCoordinator(
                mContext,
                mTopControlsStacker,
                mTabStripVisibilitySupplier,
                mDesktopWindowStateManager);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testLockWhenInDesktopWindowing() {
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(true);
        mTabStripVisibilitySupplier.set(StripVisibilityState.VISIBLE);
        createCoordinator();

        verify(mTopControlsStacker).setScrollingDisabled(true);
        verify(mTopControlsStacker).requestLayerUpdatePost(false);
    }

    @Test
    @Config(qualifiers = "sw720dp")
    public void testLockWhenOnLargeTabletAndStripVisible() {
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(false);
        mTabStripVisibilitySupplier.set(StripVisibilityState.VISIBLE);
        createCoordinator();

        verify(mTopControlsStacker).setScrollingDisabled(true);
        verify(mTopControlsStacker).requestLayerUpdatePost(false);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testUnlockWhenOnSmallTablet() {
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(false);
        mTabStripVisibilitySupplier.set(StripVisibilityState.VISIBLE);
        createCoordinator();

        verify(mTopControlsStacker, atLeastOnce()).setScrollingDisabled(false);
        verify(mTopControlsStacker, times(0)).requestLayerUpdatePost(false);
    }

    @Test
    @Config(qualifiers = "sw720dp")
    public void testUnlockWhenStripHidden() {
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(false);
        createCoordinator();
        clearInvocations(mTopControlsStacker);

        mTabStripVisibilitySupplier.set(StripVisibilityState.HIDDEN_BY_HEIGHT_TRANSITION);

        verify(mTopControlsStacker).setScrollingDisabled(false);
        verify(mTopControlsStacker).requestLayerUpdatePost(false);
    }

    @Test
    @Config(qualifiers = "sw480dp")
    public void testUnlockWhenOnPhone() {
        createCoordinator();

        verify(mTopControlsStacker).setScrollingDisabled(false);
        verify(mTopControlsStacker, times(0)).requestLayerUpdatePost(false);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testDesktopWindowingStateChange() {
        // Start in non-desktop mode on a small tablet; scrolling should be enabled.
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(false);
        createCoordinator();
        verify(mDesktopWindowStateManager).addObserver(mAppHeaderObserverCaptor.capture());
        mTabStripVisibilitySupplier.set(StripVisibilityState.VISIBLE);
        verify(mTopControlsStacker, atLeastOnce()).setScrollingDisabled(false);

        // Transition into desktop windowing.
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(true);
        mAppHeaderObserverCaptor.getValue().onDesktopWindowingModeChanged(true);
        verify(mTopControlsStacker, atLeastOnce()).setScrollingDisabled(true);
        verify(mTopControlsStacker).requestLayerUpdatePost(false);
    }

    @Test
    @Config(qualifiers = "sw720dp")
    public void testDeferredLocking() {
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(false);
        TopControlsLockCoordinator coordinator = createCoordinator();
        // The initial visibility is null, so updateLock() will call setScrollingDisabled(true).
        verify(mTopControlsStacker).setScrollingDisabled(true);
        verify(mTopControlsStacker).requestLayerUpdatePost(false);

        // Acquire a token.
        clearInvocations(mTopControlsStacker);
        int token = coordinator.getDeferredLockingTokenJar().acquireToken();

        // Attempt to update the lock state. This should not trigger a call to
        // setScrollingDisabled because the token is held.
        mTabStripVisibilitySupplier.set(StripVisibilityState.HIDDEN_BY_HEIGHT_TRANSITION);

        // Verify no new change occurred.
        verify(mTopControlsStacker, times(0)).setScrollingDisabled(true);

        // Release the token. The lock state should now be updated.
        coordinator.getDeferredLockingTokenJar().releaseToken(token);

        // Verify the lock is now inactive.
        verify(mTopControlsStacker).setScrollingDisabled(false);
        verify(mTopControlsStacker).requestLayerUpdatePost(false);
    }

    @Test
    @Config(qualifiers = "sw720dp")
    public void testNullDesktopWindowStateManager() {
        TopControlsLockCoordinator coordinator =
                new TopControlsLockCoordinator(
                        mContext, mTopControlsStacker, mTabStripVisibilitySupplier, null);

        mTabStripVisibilitySupplier.set(StripVisibilityState.VISIBLE);
        verify(mTopControlsStacker, atLeastOnce()).setScrollingDisabled(true);
        verify(mTopControlsStacker).requestLayerUpdatePost(false);
        clearInvocations(mTopControlsStacker);

        mTabStripVisibilitySupplier.set(StripVisibilityState.HIDDEN_BY_HEIGHT_TRANSITION);
        verify(mTopControlsStacker).setScrollingDisabled(false);
        verify(mTopControlsStacker).requestLayerUpdatePost(false);
    }

    @Test
    @Config(qualifiers = "sw720dp")
    public void testDestroy() {
        TopControlsLockCoordinator coordinator = createCoordinator();
        verify(mDesktopWindowStateManager).addObserver(mAppHeaderObserverCaptor.capture());
        // Clear invocations from the initial updateLock() call.
        Mockito.clearInvocations(mTopControlsStacker);

        coordinator.destroy();

        verify(mDesktopWindowStateManager).removeObserver(mAppHeaderObserverCaptor.getValue());
        // Verify that updates no longer trigger changes.
        mTabStripVisibilitySupplier.set(StripVisibilityState.VISIBLE);
        verify(mTopControlsStacker, never()).setScrollingDisabled(anyBoolean());
    }
}
