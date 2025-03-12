// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.reload_button;

import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.verify;

import android.view.KeyEvent;
import android.view.MotionEvent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class ReloadButtonMediatorTest {

    @Rule public MockitoRule mockitoTestRule = MockitoJUnit.rule();

    @Spy public ReloadButtonCoordinator.Delegate mDelegate;
    private PropertyModel mModel;
    private ReloadButtonMediator mMediator;

    @Before
    public void setup() {
        mModel = new PropertyModel.Builder(ReloadButtonProperties.ALL_KEYS).build();
        mMediator = new ReloadButtonMediator(mModel, mDelegate);
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
