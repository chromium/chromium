// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.reload_button;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.res.Resources;
import android.view.KeyEvent;
import android.view.MotionEvent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class ReloadButtonMediatorTest {
    private static final String STOP_LOADING_DESCRIPTION = "Stop loading";
    private static final String STOP_TOAST_MSG = "Stop";
    private static final String RELOAD_DESCRIPTION = "Reload";
    private static final String RELOAD_TOAST_MSG = "Reload";
    private static final int RELOAD_LEVEL = 0;
    private static final int STOP_LEVEL = 1;

    @Rule public MockitoRule mockitoTestRule = MockitoJUnit.rule();

    @Spy public ReloadButtonCoordinator.Delegate mDelegate;
    @Spy public Callback<String> mShowToastCallback;

    @Mock public Resources mResources;
    private PropertyModel mModel;
    private ReloadButtonMediator mMediator;

    @Before
    public void setup() {
        when(mResources.getString(R.string.accessibility_btn_stop_loading))
                .thenReturn(STOP_LOADING_DESCRIPTION);
        when(mResources.getString(R.string.accessibility_btn_refresh))
                .thenReturn(RELOAD_DESCRIPTION);
        when(mResources.getInteger(R.integer.reload_button_level_stop)).thenReturn(STOP_LEVEL);
        when(mResources.getInteger(R.integer.reload_button_level_reload)).thenReturn(RELOAD_LEVEL);
        when(mResources.getString(R.string.refresh)).thenReturn(RELOAD_TOAST_MSG);
        when(mResources.getString(R.string.menu_stop_refresh)).thenReturn(STOP_TOAST_MSG);

        mModel = new PropertyModel.Builder(ReloadButtonProperties.ALL_KEYS).build();
        mMediator = new ReloadButtonMediator(mModel, mDelegate, mShowToastCallback, mResources);
    }

    @Test
    public void testClicksWithoutShift_reloadTabWithCache() {
        final MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mModel.get(ReloadButtonProperties.TOUCH_LISTENER).onResult(event);

        mModel.get(ReloadButtonProperties.CLICK_LISTENER).run();
        verify(mDelegate).stopOrReloadCurrentTab(false);
    }

    @Test
    public void testClicksWithShift_reloadTabIgnoringCache() {
        final MotionEvent event =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, KeyEvent.META_SHIFT_ON);
        mModel.get(ReloadButtonProperties.TOUCH_LISTENER).onResult(event);

        mModel.get(ReloadButtonProperties.CLICK_LISTENER).run();
        verify(mDelegate).stopOrReloadCurrentTab(true);
    }

    @Test
    public void testReloadingActive_setButtonToStop() {
        mMediator.setReloading(true);

        assertEquals(
                "Reload icon should be stop reloading",
                STOP_LEVEL,
                mModel.get(ReloadButtonProperties.DRAWABLE_LEVEL));
        assertEquals(
                "Content description should be stop reloading",
                STOP_LOADING_DESCRIPTION,
                mModel.get(ReloadButtonProperties.CONTENT_DESCRIPTION));
    }

    @Test
    public void testStopReloading_setButtonToReload() {
        mMediator.setReloading(false);

        assertEquals(
                "Reload icon should be reload",
                RELOAD_LEVEL,
                mModel.get(ReloadButtonProperties.DRAWABLE_LEVEL));
        assertEquals(
                "Content description should be reload",
                RELOAD_DESCRIPTION,
                mModel.get(ReloadButtonProperties.CONTENT_DESCRIPTION));
    }

    @Test
    public void testLongClickReloading_showStopToast() {
        mMediator.setReloading(true);

        mModel.get(ReloadButtonProperties.LONG_CLICK_LISTENER).run();
        verify(mShowToastCallback).onResult(STOP_TOAST_MSG);
    }

    @Test
    public void testLongClickIdle_showReloadToast() {
        mMediator.setReloading(false);

        mModel.get(ReloadButtonProperties.LONG_CLICK_LISTENER).run();
        verify(mShowToastCallback).onResult(RELOAD_TOAST_MSG);
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();
        assertNull(
                "Touch listener should be set to null",
                mModel.get(ReloadButtonProperties.TOUCH_LISTENER));
        assertNull(
                "Click listener should be set to null",
                mModel.get(ReloadButtonProperties.CLICK_LISTENER));
    }
}
