// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.content.res.ResourcesCompat;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Robolectric tests for {@link PermissionOneTimeDialogCustomViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PermissionOneTimeDialogCustomViewBinderTest {
    private Activity mActivity;
    private View mCustomView;
    private PropertyModel mPropertyModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private static final String MESSAGE_TEXT =
            "permission.site wants to use your device's location";

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mCustomView =
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.permission_dialog_one_time_permission, null);
        mPropertyModel =
                new PropertyModel.Builder(PermissionDialogCustomViewProperties.ALL_KEYS)
                        .with(PermissionDialogCustomViewProperties.MESSAGE_TEXT, MESSAGE_TEXT)
                        .build();
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mPropertyModel, mCustomView, PermissionOneTimeDialogCustomViewBinder::bind);
    }

    @After
    public void tearDown() {
        mPropertyModelChangeProcessor.destroy();
    }

    @Test
    @SmallTest
    public void testMessageText() {
        TextView permissionDialogMessageText = mCustomView.findViewById(R.id.text);

        assertEquals(MESSAGE_TEXT, permissionDialogMessageText.getText().toString());
    }

    @Test
    @SmallTest
    public void testIcon_WithoutTint() {
        Drawable drawable =
                ResourcesCompat.getDrawable(
                        mActivity.getResources(),
                        R.drawable.ic_folder_blue_24dp,
                        mActivity.getTheme());

        mPropertyModel.set(PermissionDialogCustomViewProperties.ICON, drawable);
        ImageView iconView = mCustomView.findViewById(R.id.icon);

        assertEquals(drawable, iconView.getDrawable());
        assertEquals(null, iconView.getColorFilter());
    }

    @Test
    @SmallTest
    public void testIcon_WithTint() {
        Drawable drawable =
                ResourcesCompat.getDrawable(
                        mActivity.getResources(),
                        R.drawable.ic_folder_blue_24dp,
                        mActivity.getTheme());
        ColorStateList tintList =
                mActivity.getColorStateList(R.color.default_icon_color_accent1_tint_list);
        mPropertyModel.set(PermissionDialogCustomViewProperties.ICON, drawable);
        mPropertyModel.set(PermissionDialogCustomViewProperties.ICON_TINT, tintList);

        ImageView iconView = mCustomView.findViewById(R.id.icon);

        assertEquals(iconView.getDrawable(), drawable);
        assertEquals(tintList, iconView.getImageTintList());
    }

    @Test
    @SmallTest
    public void testIcon_ResetTint() {
        testIcon_WithTint();
        mPropertyModel.set(PermissionDialogCustomViewProperties.ICON_TINT, null);

        ImageView iconView = mCustomView.findViewById(R.id.icon);
        assertNull(iconView.getImageTintList());
    }
}
