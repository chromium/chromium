// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.ui.listmenu.ListMenuButton;
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

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<Integer> mCallback;

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
    public void testCreateOnLongClickListener() {
        AdaptiveButtonActionMenuCoordinator coordinator = new AdaptiveButtonActionMenuCoordinator();
        View.OnLongClickListener listener = coordinator.createOnLongClickListener(mCallback);

        ListMenuButton menuView =
                spy(
                        new ListMenuButton(
                                ApplicationProvider.getApplicationContext(),
                                Robolectric.buildAttributeSet().build()));
        doReturn(ApplicationProvider.getApplicationContext().getResources())
                .when(menuView)
                .getResources();

        listener.onLongClick(menuView);

        ViewGroup menuContent = (ViewGroup) coordinator.getContentViewForTesting();
        ListView menuListView = menuContent.findViewById(R.id.app_menu_list);
        menuListView.performItemClick(null, 0, menuListView.getAdapter().getItemId(0));

        verify(menuView).showMenu();
        verify(mCallback).onResult(R.id.customize_adaptive_button_menu_id);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2)
    public void testCreateOnLongClickListener_clickHandlerIsNotModified() {
        AdaptiveButtonActionMenuCoordinator coordinator = new AdaptiveButtonActionMenuCoordinator();
        View.OnLongClickListener listener = coordinator.createOnLongClickListener(mCallback);

        ListMenuButton menuView =
                spy(
                        new ListMenuButton(
                                ApplicationProvider.getApplicationContext(),
                                Robolectric.buildAttributeSet().build()));
        doReturn(ApplicationProvider.getApplicationContext().getResources())
                .when(menuView)
                .getResources();

        // Long click menuView, menu should be shown.
        listener.onLongClick(menuView);

        // Click menuView, nothing should happen.
        menuView.performClick();

        // Menu should have been shown once (on long click).
        verify(menuView).showMenu();
    }
}
