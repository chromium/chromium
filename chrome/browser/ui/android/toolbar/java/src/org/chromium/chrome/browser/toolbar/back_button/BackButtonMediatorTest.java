// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.back_button;

import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class BackButtonMediatorTest {

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();
    @Mock public Runnable mOnBackPressed;
    private PropertyModel mModel;
    private BackButtonMediator mMediator;

    @Before
    public void setup() {
        mModel =
                new PropertyModel.Builder(BackButtonProperties.ALL_KEYS)
                        .with(BackButtonProperties.CLICK_LISTENER, mOnBackPressed)
                        .build();
        mMediator = new BackButtonMediator(mModel, mOnBackPressed);
    }

    @Test
    public void testClick_shouldForwardCallToParent() {
        mModel.get(BackButtonProperties.CLICK_LISTENER).run();
        verify(mOnBackPressed).run();
    }
}
