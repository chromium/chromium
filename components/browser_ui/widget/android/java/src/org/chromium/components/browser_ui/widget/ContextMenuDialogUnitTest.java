// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.app.ActionBar.LayoutParams;
import android.app.Activity;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPhoneWindow;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit test for {@link ContextMenuDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowPhoneWindow.class})
public class ContextMenuDialogUnitTest {
    ContextMenuDialog mDialog;

    Activity mActivity;
    View mMenuContentView;
    View mRootView;

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mRootView = new FrameLayout(mActivity);
        TextView textView = new TextView(mActivity);
        textView.setText("Test String");
        mMenuContentView = textView;
        mActivity.setContentView(
                mRootView, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
    }

    @Test
    public void testCreate_usePopupStyle() {
        mDialog = new ContextMenuDialog(mActivity, 0, 0, 0, 0, 0, 0, mRootView, mMenuContentView,
                /*isPopup=*/false, /*shouldRemoveScrim=*/true, 0, 0);
        mDialog.show();

        ShadowPhoneWindow window = (ShadowPhoneWindow) Shadows.shadowOf(mDialog.getWindow());
        Assert.assertTrue("FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS not in window flags.",
                window.getFlag(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS));
        Assert.assertTrue("FLAG_NOT_TOUCH_MODAL not in window flags.",
                window.getFlag(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL));
        Assert.assertFalse("FLAG_DIM_BEHIND is in flags.",
                window.getFlag(WindowManager.LayoutParams.FLAG_DIM_BEHIND));

        Assert.assertEquals("Dialog status bar color should match activity status bar color.",
                mActivity.getWindow().getStatusBarColor(), mDialog.getWindow().getStatusBarColor());
        Assert.assertEquals(
                "Dialog navigation bar color should match activity navigation bar color.",
                mActivity.getWindow().getNavigationBarColor(),
                mDialog.getWindow().getNavigationBarColor());
    }

    @Test
    public void testCreateDialog_useRegularStyle() {
        mDialog = new ContextMenuDialog(mActivity, 0, 0, 0, 0, 0, 0, mRootView, mMenuContentView,
                /*isPopup=*/false, /*shouldRemoveScrim=*/false, 0, 0);
        mDialog.show();

        // Only checks the flag is unset to make sure the setup for |shouldRemoveScrim| is not ran.
        ShadowPhoneWindow window = (ShadowPhoneWindow) Shadows.shadowOf(mDialog.getWindow());
        Assert.assertFalse("FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS is in window flags.",
                window.getFlag(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS));
        Assert.assertFalse("FLAG_NOT_TOUCH_MODAL is in window flags.",
                window.getFlag(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL));
    }
}
