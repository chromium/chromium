// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.graphics.ColorFilter;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.res.ResourcesCompat;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Robolectric tests for {@link PermissionDialogCustomViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PermissionDialogCustomViewBinderTest {
    private Activity mActivity;
    private View mCustomView;
    private PropertyModel mPropertyModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private static final String MESSAGE_TEXT = "test";

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mCustomView = LayoutInflater.from(mActivity).inflate(R.layout.permission_dialog, null);
        mPropertyModel =
                new PropertyModel.Builder(PermissionDialogCustomViewProperties.ALL_KEYS)
                        .with(PermissionDialogCustomViewProperties.MESSAGE_TEXT, MESSAGE_TEXT)
                        .build();
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mPropertyModel, mCustomView, PermissionDialogCustomViewBinder::bind);
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

        TextViewWithCompoundDrawables permissionDialogMessageText =
                mCustomView.findViewById(R.id.text);

        Drawable[] expected = {drawable, null, null, null};
        assertEquals(expected, permissionDialogMessageText.getCompoundDrawablesRelative());
        assertEquals(
                null,
                permissionDialogMessageText.getCompoundDrawablesRelative()[0].getColorFilter());
    }

    @Test
    @SmallTest
    public void testIcon_WithTint() {
        Drawable drawable =
                ResourcesCompat.getDrawable(
                        mActivity.getResources(),
                        R.drawable.ic_folder_blue_24dp,
                        mActivity.getTheme());
        int iconTint = R.color.default_icon_color_accent1_tint_list;

        mPropertyModel.set(PermissionDialogCustomViewProperties.ICON, drawable);
        mPropertyModel.set(
                PermissionDialogCustomViewProperties.ICON_TINT,
                AppCompatResources.getColorStateList(mActivity, iconTint));

        TextViewWithCompoundDrawables permissionDialogMessageText =
                mCustomView.findViewById(R.id.text);

        Drawable[] expectedIcon = {drawable, null, null, null};
        ColorFilter expectedTint =
                new PorterDuffColorFilter(mActivity.getColor(iconTint), PorterDuff.Mode.SRC_IN);
        assertEquals(expectedIcon, permissionDialogMessageText.getCompoundDrawablesRelative());
        assertEquals(
                expectedTint,
                permissionDialogMessageText.getCompoundDrawablesRelative()[0].getColorFilter());
    }

    @Test
    @SmallTest
    public void testIcon_ResetTint() {
        testIcon_WithTint();

        mPropertyModel.set(PermissionDialogCustomViewProperties.ICON_TINT, null);

        TextViewWithCompoundDrawables permissionDialogMessageText =
                mCustomView.findViewById(R.id.text);
        assertEquals(
                null,
                permissionDialogMessageText.getCompoundDrawablesRelative()[0].getColorFilter());
    }
}
