// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.widget.AnchoredPopupWindow;

/** Unit tests for ExtensionsMenuCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionsMenuCoordinatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mContext;

    private ListMenuButton mExtensionsMenuButton;
    @Mock private AnchoredPopupWindow mMenuWindow;

    private ExtensionsMenuCoordinator mExtensionsMenuCoordinator;

    @Before
    public void setUp() {
        mContext = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        mExtensionsMenuButton = new ListMenuButton(mContext, null);
        mContext.setContentView(mExtensionsMenuButton);
        mExtensionsMenuCoordinator =
                new ExtensionsMenuCoordinator(mContext, mExtensionsMenuButton, mMenuWindow);
    }

    @Test
    public void testShowMenu() {
        mExtensionsMenuCoordinator.showMenu();
        verify(mMenuWindow).show();
    }
}
