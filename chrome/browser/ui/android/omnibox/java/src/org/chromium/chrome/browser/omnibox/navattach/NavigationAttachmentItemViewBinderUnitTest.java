// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link NavigationAttachmentItemViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NavigationAttachmentItemViewBinderUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private @Mock Drawable mDrawable;

    private Activity mActivity;
    private PropertyModel mModel;
    private ConstraintLayout mView;

    @Before
    public void setUp() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mActivity = activity;
                            mModel = new PropertyModel(NavigationAttachmentItemProperties.ALL_KEYS);
                            mView =
                                    (ConstraintLayout)
                                            LayoutInflater.from(activity)
                                                    .inflate(
                                                            R.layout.navigation_attachment_item,
                                                            null);
                            PropertyModelChangeProcessor.create(
                                    mModel, mView, NavigationAttachmentItemViewBinder::bind);
                        });
    }

    @Test
    public void testSetThumbnail() {
        mModel.set(NavigationAttachmentItemProperties.THUMBNAIL, mDrawable);
        ImageView imageView = mView.findViewById(R.id.attachment_thumbnail);
        assertEquals(mDrawable, imageView.getDrawable());
    }

    @Test
    public void testSetTitle_emptyString() {
        mModel.set(NavigationAttachmentItemProperties.TITLE, "");
        TextView textView = mView.findViewById(R.id.attachment_title);
        assertEquals(View.GONE, textView.getVisibility());
    }

    @Test
    public void testSetTitle() {
        mModel.set(NavigationAttachmentItemProperties.TITLE, "My Attachment");
        TextView textView = mView.findViewById(R.id.attachment_title);
        assertEquals("My Attachment", textView.getText());
    }

    @Test
    public void testSetDescription_emptyTitle() {
        mModel.set(NavigationAttachmentItemProperties.TITLE, "");
        mModel.set(NavigationAttachmentItemProperties.DESCRIPTION, "My Description");
        TextView textView = mView.findViewById(R.id.attachment_description);
        assertEquals(View.GONE, textView.getVisibility());
    }

    @Test
    public void testSetDescription_withTitle() {
        mModel.set(NavigationAttachmentItemProperties.TITLE, "My Title");
        mModel.set(NavigationAttachmentItemProperties.DESCRIPTION, "My Description");

        TextView title = mView.findViewById(R.id.attachment_title);
        assertEquals("My Title", title.getText());
        TextView description = mView.findViewById(R.id.attachment_description);
        assertEquals("My Description", description.getText());
    }
}
