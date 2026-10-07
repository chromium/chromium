// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.widget.AnchoredPopupWindow;

/** Unit tests for the {@link AdaptiveButtonActionMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AdaptiveButtonActionMenuCoordinatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<Integer> mCallback;

    private ListMenuButton mMenuView;
    private int mMenuShownCount;

    @Before
    public void setUp() {
        AnchoredPopupWindow.setShowHookForTesting(() -> {});

        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mMenuView = new ListMenuButton(activity, /* attrs= */ null);
        // ListMenuButton only shows its menu when attached to a window.
        activity.setContentView(mMenuView);
        mMenuView.addPopupListener(() -> mMenuShownCount++);
    }

    @Test
    public void testCreateOnLongClickListener() {
        var coordinator = new AdaptiveButtonActionMenuCoordinator(/* showMenu= */ true);
        View.OnLongClickListener listener = coordinator.createOnLongClickListener(mCallback);

        listener.onLongClick(mMenuView);

        coordinator.getListMenuForTesting().clickItemForTesting(0);

        assertEquals(1, mMenuShownCount);
        verify(mCallback).onResult(R.id.customize_adaptive_button_menu_id);
    }

    @Test
    public void testCreateOnLongClickListener_clickHandlerIsNotModified() {
        var coordinator = new AdaptiveButtonActionMenuCoordinator(/* showMenu= */ true);
        View.OnLongClickListener listener = coordinator.createOnLongClickListener(mCallback);

        // Long click menuView, menu should be shown.
        listener.onLongClick(mMenuView);

        // Click menuView, nothing should happen.
        mMenuView.performClick();

        // Menu should have been shown once (on long click).
        assertEquals(1, mMenuShownCount);
    }

    @Test
    public void testCreateOnLongClickListener_showsToast() {
        var coordinator = spy(new AdaptiveButtonActionMenuCoordinator(/* showMenu= */ false));
        View.OnLongClickListener listener = coordinator.createOnLongClickListener(mCallback);

        String contentDescription = "Test Content Description";
        mMenuView.setContentDescription(contentDescription);

        listener.onLongClick(mMenuView);

        verify(coordinator).showAnchoredToastInternal(mMenuView, contentDescription);
        assertEquals(0, mMenuShownCount);
    }
}
