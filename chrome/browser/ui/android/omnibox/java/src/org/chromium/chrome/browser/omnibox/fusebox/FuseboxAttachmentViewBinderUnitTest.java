// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

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
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerViewAdapter.FuseboxAttachmentType;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link FuseboxAttachmentViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxAttachmentViewBinderUnitTest {
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
                            mModel = new PropertyModel(FuseboxAttachmentProperties.ALL_KEYS);
                            mView =
                                    (ConstraintLayout)
                                            LayoutInflater.from(activity)
                                                    .inflate(
                                                            R.layout.fusebox_attachment_layout,
                                                            null);
                            PropertyModelChangeProcessor.create(
                                    mModel, mView, FuseboxAttachmentViewBinder::bind);
                        });
    }

    @Test
    public void testSetThumbnail() {
        FuseboxAttachment attachment =
                new FuseboxAttachment(
                        FuseboxAttachmentType.ATTACHMENT_FILE,
                        mDrawable,
                        "Test",
                        "text/plain",
                        new byte[0]);
        mModel.set(FuseboxAttachmentProperties.ATTACHMENT, attachment);
        ImageView imageView = mView.findViewById(R.id.attachment_thumbnail);
        assertEquals(mDrawable, imageView.getDrawable());
    }

    @Test
    public void testSetTitle_emptyString() {
        FuseboxAttachment attachment =
                new FuseboxAttachment(
                        FuseboxAttachmentType.ATTACHMENT_FILE,
                        mDrawable,
                        "",
                        "text/plain",
                        new byte[0]);
        mModel.set(FuseboxAttachmentProperties.ATTACHMENT, attachment);
        TextView textView = mView.findViewById(R.id.attachment_title);
        assertEquals(View.GONE, textView.getVisibility());
    }

    @Test
    public void testSetTitle() {
        FuseboxAttachment attachment =
                new FuseboxAttachment(
                        FuseboxAttachmentType.ATTACHMENT_FILE,
                        mDrawable,
                        "My Attachment",
                        "text/plain",
                        new byte[0]);
        mModel.set(FuseboxAttachmentProperties.ATTACHMENT, attachment);
        TextView textView = mView.findViewById(R.id.attachment_title);
        assertEquals("My Attachment", textView.getText());
    }

    @Test
    public void testSetDescription_withTitle() {
        FuseboxAttachment attachment =
                new FuseboxAttachment(
                        FuseboxAttachmentType.ATTACHMENT_FILE,
                        mDrawable,
                        "My Title",
                        "text/plain",
                        new byte[0]);
        mModel.set(FuseboxAttachmentProperties.ATTACHMENT, attachment);

        TextView title = mView.findViewById(R.id.attachment_title);
        assertEquals("My Title", title.getText());
    }

    @Test
    public void testSetThumbnail_fallbackWhenNull() {
        FuseboxAttachment attachment =
                new FuseboxAttachment(
                        FuseboxAttachmentType.ATTACHMENT_FILE,
                        null, // null thumbnail should trigger fallback
                        "Test",
                        "text/plain",
                        new byte[0]);
        mModel.set(FuseboxAttachmentProperties.ATTACHMENT, attachment);
        ImageView imageView = mView.findViewById(R.id.attachment_thumbnail);
        // Should have fallback drawable, not null
        assertNotNull(
                "Fallback drawable should be set when thumbnail is null", imageView.getDrawable());
    }
}
