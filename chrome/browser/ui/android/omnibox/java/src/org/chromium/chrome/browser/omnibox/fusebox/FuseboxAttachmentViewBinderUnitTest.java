// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.doReturn;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.constraintlayout.widget.ConstraintLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link FuseboxAttachmentViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxAttachmentViewBinderUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Drawable mDrawable;
    @Mock private Tab mTab;

    private ActivityController<TestActivity> mActivityController;
    private PropertyModel mModel;
    private ConstraintLayout mView;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        Activity activity = mActivityController.get();
        mModel = new PropertyModel(FuseboxAttachmentProperties.ALL_KEYS);
        mModel.set(FuseboxAttachmentProperties.COLOR_SCHEME, BrandedColorScheme.APP_DEFAULT);
        mView =
                (ConstraintLayout)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.fusebox_attachment_layout, /* root= */ null);
        mView.setLayoutParams(new LayoutParams(100, 100));
        PropertyModelChangeProcessor.create(mModel, mView, FuseboxAttachmentViewBinder::bind);
    }

    @After
    public void tearDown() {
        mActivityController.close();
        OmniboxResourceProvider.setTabFaviconFactory(null);
    }

    @Test
    public void testSetThumbnail() {
        FuseboxAttachment attachment =
                FuseboxAttachment.forFile(mDrawable, "Test", "text/plain", new byte[0]);
        attachment.setUploadIsComplete();
        mModel.set(FuseboxAttachmentProperties.ATTACHMENT, attachment);
        ImageView imageView = mView.findViewById(R.id.attachment_thumbnail);
        assertEquals(mDrawable, imageView.getDrawable());
    }

    @Test
    public void testSetTitle_emptyString() {
        FuseboxAttachment attachment =
                FuseboxAttachment.forFile(mDrawable, "", "text/plain", new byte[0]);
        attachment.setUploadIsComplete();
        mModel.set(FuseboxAttachmentProperties.ATTACHMENT, attachment);
        TextView textView = mView.findViewById(R.id.attachment_title);
        assertEquals(View.GONE, textView.getVisibility());
    }

    @Test
    public void testSetTitle() {
        FuseboxAttachment attachment =
                FuseboxAttachment.forFile(mDrawable, "My Attachment", "text/plain", new byte[0]);
        attachment.setUploadIsComplete();
        mModel.set(FuseboxAttachmentProperties.ATTACHMENT, attachment);
        TextView textView = mView.findViewById(R.id.attachment_title);
        assertEquals("My Attachment", textView.getText());
    }

    @Test
    public void testSetDescription_withTitle() {
        FuseboxAttachment attachment =
                FuseboxAttachment.forFile(mDrawable, "My Title", "text/plain", new byte[0]);
        attachment.setUploadIsComplete();
        mModel.set(FuseboxAttachmentProperties.ATTACHMENT, attachment);

        TextView title = mView.findViewById(R.id.attachment_title);
        assertEquals("My Title", title.getText());
    }

    @Test
    public void testSetThumbnail_fallbackWhenNull() {
        FuseboxAttachment attachment =
                FuseboxAttachment.forFile(
                        null, // null thumbnail should trigger fallback
                        "Test",
                        "text/plain",
                        new byte[0]);
        attachment.setUploadIsComplete();
        mModel.set(FuseboxAttachmentProperties.ATTACHMENT, attachment);
        ImageView imageView = mView.findViewById(R.id.attachment_thumbnail);
        // Should have fallback drawable, not null
        assertNotNull(
                "Fallback drawable should be set when thumbnail is null", imageView.getDrawable());
    }

    @Test
    public void testUploadPending() {
        FuseboxAttachment attachment =
                FuseboxAttachment.forFile(
                        null, // null thumbnail should trigger fallback
                        "Test",
                        "text/plain",
                        new byte[0]);
        mModel.set(FuseboxAttachmentProperties.ATTACHMENT, attachment);

        ImageView imageView = mView.findViewById(R.id.attachment_thumbnail);
        View spinner = mView.findViewById(R.id.attachment_spinner);
        TextView textView = mView.findViewById(R.id.attachment_title);

        assertEquals(View.GONE, imageView.getVisibility());
        assertEquals(View.VISIBLE, spinner.getVisibility());
        assertEquals(View.GONE, textView.getVisibility());

        attachment.setUploadIsComplete();
        FuseboxAttachmentViewBinder.bind(mModel, mView, FuseboxAttachmentProperties.ATTACHMENT);

        assertEquals(View.GONE, spinner.getVisibility());
        assertEquals(View.VISIBLE, imageView.getVisibility());
        assertEquals(View.VISIBLE, textView.getVisibility());
    }

    @Test
    public void testGetThumbnailDrawable() {
        Context context = mView.getContext();

        // File attachment with thumbnail.
        FuseboxAttachment fileWithThumb =
                FuseboxAttachment.forFile(mDrawable, "File", "text/plain", new byte[0]);
        assertEquals(
                mDrawable,
                FuseboxAttachmentViewBinder.getThumbnailDrawable(mModel, fileWithThumb, context));

        // File attachment without thumbnail (fallback).
        FuseboxAttachment fileNoThumb =
                FuseboxAttachment.forFile(null, "File", "text/plain", new byte[0]);
        Drawable fallback =
                FuseboxAttachmentViewBinder.getThumbnailDrawable(mModel, fileNoThumb, context);
        assertNotNull(fallback);
        assertNotEquals(mDrawable, fallback);

        // Image attachment with thumbnail.
        FuseboxAttachment imageWithThumb =
                FuseboxAttachment.forCameraImage(mDrawable, "Image", "image/png", new byte[0]);
        assertEquals(
                mDrawable,
                FuseboxAttachmentViewBinder.getThumbnailDrawable(mModel, imageWithThumb, context));

        // Image attachment without thumbnail (fallback).
        FuseboxAttachment imageNoThumb =
                FuseboxAttachment.forCameraImage(null, "Image", "image/png", new byte[0]);
        Drawable imageFallback =
                FuseboxAttachmentViewBinder.getThumbnailDrawable(mModel, imageNoThumb, context);
        assertNotNull(imageFallback);
        assertNotEquals(mDrawable, imageFallback);

        // Tab attachment with favicon.
        Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        OmniboxResourceProvider.setTabFaviconFactory(t -> bitmap);
        doReturn(1).when(mTab).getId();
        doReturn("Title").when(mTab).getTitle();

        FuseboxAttachment tabAttachment = FuseboxAttachment.forTab(mTab, context.getResources());
        Drawable tabDrawable =
                FuseboxAttachmentViewBinder.getThumbnailDrawable(mModel, tabAttachment, context);
        assertNotNull(tabDrawable);

        // Tab attachment without favicon (fallback).
        OmniboxResourceProvider.setTabFaviconFactory(t -> null);
        FuseboxAttachment tabAttachmentNoFavicon =
                FuseboxAttachment.forTab(mTab, context.getResources());
        Drawable tabDrawableNoFavicon =
                FuseboxAttachmentViewBinder.getThumbnailDrawable(
                        mModel, tabAttachmentNoFavicon, context);
        assertNotNull(tabDrawableNoFavicon);
    }
}
