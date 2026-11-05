// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link AdaptiveUiController#from} and {@link AdaptiveUiControllerProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AdaptiveUiControllerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final UnownedUserDataHost mHost = new UnownedUserDataHost();

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private AdaptiveUiController mController;

    @Before
    public void setUp() {
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mHost);
    }

    @Test
    public void testFromReturnsNullWhenNotAttached() {
        assertNull(AdaptiveUiController.from(mWindowAndroid));
    }

    @Test
    public void testAttachAndDetach() {
        AdaptiveUiControllerProvider.attach(mWindowAndroid, mController);
        assertEquals(mController, AdaptiveUiController.from(mWindowAndroid));

        AdaptiveUiControllerProvider.detach(mController);
        assertNull(AdaptiveUiController.from(mWindowAndroid));
    }
}
