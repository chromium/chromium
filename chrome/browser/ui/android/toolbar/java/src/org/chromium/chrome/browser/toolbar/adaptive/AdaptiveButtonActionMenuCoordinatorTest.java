// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

import android.view.View;
import android.view.ViewGroup;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.widget.AnchoredPopupWindow;

/** Unit tests for the {@link AdaptiveButtonActionMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {AdaptiveButtonActionMenuCoordinatorTest.ShadowAnchoredPopupWindow.class})
public class AdaptiveButtonActionMenuCoordinatorTest {
    /** Shadow disabling {@code showPopupWindow()} which hangs under robolectric. */
    @Implements(AnchoredPopupWindow.class)
    public static class ShadowAnchoredPopupWindow {
        @Implementation
        protected void showPopupWindow() {}
    }

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private Callback<Integer> mCallback;

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR})
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    public void testMenu() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AdaptiveButtonActionMenuCoordinator coordinator =
                    new AdaptiveButtonActionMenuCoordinator();
            View.OnLongClickListener listener = coordinator.createOnLongClickListener(mCallback);

            ListMenuButton menuView =
                    spy(new ListMenuButton(ApplicationProvider.getApplicationContext(),
                            Robolectric.buildAttributeSet().build()));
            doReturn(ApplicationProvider.getApplicationContext().getResources())
                    .when(menuView)
                    .getResources();

            listener.onLongClick(menuView);
            ViewGroup menuContent = (ViewGroup) coordinator.getContentViewForTesting();
            menuContent.getChildAt(0).performClick();
        });

        // TODO(bttk): complete this test
        // verify(mCallback).onResult(R.id.customize_adaptive_button_menu_id);
    }
}
