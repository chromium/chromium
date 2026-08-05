// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

/** Unit tests for {@link PageZoomIndicatorCoordinator}. */
@SmallTest
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "sw600dp")
public class PageZoomIndicatorCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PageZoomManager mManager;
    @Mock private WebContents mWebContents;
    @Captor private ArgumentCaptor<ZoomEventsObserver> mObserverCaptor;

    private PageZoomIndicatorCoordinator mCoordinator;
    private View mAnchorView;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_MaterialComponents_DayNight);

        mAnchorView = new View(mContext);
        Supplier<View> anchorViewSupplier = () -> mAnchorView;

        when(mManager.getWebContents()).thenReturn(mWebContents);
        when(mManager.getZoomLevel()).thenReturn(0.0);
        when(mManager.getDefaultZoomLevel()).thenReturn(0.0);
        when(mManager.canShowPopupWindow()).thenReturn(true);
        DeviceFormFactor.setIsTabletForTesting(true);

        mCoordinator = new PageZoomIndicatorCoordinator(anchorViewSupplier, mManager);
        mCoordinator.onNativeInitialized();

        verify(mManager, atLeastOnce()).addZoomEventsObserver(mObserverCaptor.capture());
    }

    @After
    public void tearDown() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
        }
        ShadowLooper.idleMainLooper();
    }

    @Test
    public void testShow_FromOmniboxIconClick_DoesNotStartDismissalTimer() {
        mCoordinator.show();
        assertTrue(mCoordinator.isPopupWindowShowing());

        // Advance looper past the 2-second timeout
        ShadowLooper.idleMainLooper(3000, TimeUnit.MILLISECONDS);

        // Verify popup is still showing (no auto-dismiss timer for icon clicks)
        assertTrue(mCoordinator.isPopupWindowShowing());
    }

    @Test
    public void testOnZoomLevelChanged_ShortcutTriggered_ShowsPopupWithDismissalTimer() {
        assertFalse(mCoordinator.isPopupWindowShowing());

        // Trigger shortcut native zoom event
        mObserverCaptor.getValue().onZoomLevelChanged("example.com", 0.52);
        assertTrue(mCoordinator.isPopupWindowShowing());

        // Advance looper past the 2-second timeout
        ShadowLooper.idleMainLooper(2500, TimeUnit.MILLISECONDS);

        // Verify popup auto-dismisses
        assertFalse(mCoordinator.isPopupWindowShowing());
    }

    @Test
    public void testOnZoomLevelChanged_ShortcutTriggered_ResetsDismissalTimer() {
        // Trigger shortcut zoom event
        mObserverCaptor.getValue().onZoomLevelChanged("example.com", 0.52);
        assertTrue(mCoordinator.isPopupWindowShowing());

        // Advance looper by 1.5 seconds (popup still open)
        ShadowLooper.idleMainLooper(1500, TimeUnit.MILLISECONDS);
        assertTrue(mCoordinator.isPopupWindowShowing());

        // Trigger second shortcut zoom event (resets timer)
        mObserverCaptor.getValue().onZoomLevelChanged("example.com", 0.65);

        // Advance another 1.5 seconds (3 seconds total from start, but 1.5s from reset)
        ShadowLooper.idleMainLooper(1500, TimeUnit.MILLISECONDS);
        assertTrue(mCoordinator.isPopupWindowShowing());

        // Advance another 1 second past remaining timer
        ShadowLooper.idleMainLooper(1000, TimeUnit.MILLISECONDS);
        assertFalse(mCoordinator.isPopupWindowShowing());
    }

    @Test
    public void testOnZoomLevelChanged_IconClickTriggered_DoesNotDismissOnTimer() {
        // Show via icon click
        mCoordinator.show();
        assertTrue(mCoordinator.isPopupWindowShowing());

        // Trigger zoom event while open
        mObserverCaptor.getValue().onZoomLevelChanged("example.com", 0.52);

        // Advance looper past 3 seconds
        ShadowLooper.idleMainLooper(3000, TimeUnit.MILLISECONDS);
        assertTrue(mCoordinator.isPopupWindowShowing());
    }

    @Test
    public void testHoverInteraction_PausesAndResetsDismissalTimer() {
        // Shortcut triggers popup
        mObserverCaptor.getValue().onZoomLevelChanged("example.com", 0.52);
        assertTrue(mCoordinator.isPopupWindowShowing());

        // Hover enter
        mCoordinator.onHoverForTesting(MotionEvent.ACTION_HOVER_ENTER);

        // Advance looper by 3 seconds while hovered
        ShadowLooper.idleMainLooper(3000, TimeUnit.MILLISECONDS);
        assertTrue(mCoordinator.isPopupWindowShowing());

        // Hover exit
        mCoordinator.onHoverForTesting(MotionEvent.ACTION_HOVER_EXIT);

        // Advance looper past 2-second timeout
        ShadowLooper.idleMainLooper(2500, TimeUnit.MILLISECONDS);
        assertFalse(mCoordinator.isPopupWindowShowing());
    }

    @Test
    public void testDestroy_RemovesObserverAndHidesPopup() {
        mCoordinator.show();
        assertTrue(mCoordinator.isPopupWindowShowing());

        mCoordinator.destroy();

        assertFalse(mCoordinator.isPopupWindowShowing());
        verify(mManager).removeZoomEventsObserver(mObserverCaptor.getValue());
    }

    @Test
    public void testOnZoomLevelChanged_CannotShowPopupWindow_DoesNotShowPopup() {
        when(mManager.canShowPopupWindow()).thenReturn(false);
        assertFalse(mCoordinator.isPopupWindowShowing());

        mObserverCaptor.getValue().onZoomLevelChanged("example.com", 0.52);
        assertFalse(mCoordinator.isPopupWindowShowing());
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testOnZoomLevelChanged_PhoneFormFactor_DoesNotShowPopup() {
        assertFalse(mCoordinator.isPopupWindowShowing());

        mObserverCaptor.getValue().onZoomLevelChanged("example.com", 0.52);
        assertFalse(mCoordinator.isPopupWindowShowing());
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testShow_PhoneFormFactor_DoesNotShowPopup() {
        mCoordinator.show();
        assertFalse(mCoordinator.isPopupWindowShowing());
    }
}

