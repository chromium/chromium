// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.Intent;
import android.provider.MediaStore;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link NavigationAttachmentsMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NavigationAttachmentsMediatorUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock ViewGroup mViewGroup;
    private @Mock NavigationAttachmentsViewHolder mViewHolder;
    private @Mock NavigationAttachmentsPopup mPopup;
    private @Mock WindowAndroid mWindowAndroid;

    private PropertyModel mModel;
    private NavigationAttachmentsMediator mMediator;

    @Before
    public void setUp() {
        mModel = new PropertyModel(NavigationAttachmentsProperties.ALL_KEYS);
        mViewHolder = new NavigationAttachmentsViewHolder(mViewGroup, mPopup);
        mMediator = new NavigationAttachmentsMediator(mWindowAndroid, mModel, mViewHolder);
    }

    @Test
    public void initialState_toolbarIsHidden() {
        assertFalse(mModel.get(NavigationAttachmentsProperties.TOOLBAR_VISIBLE));
    }

    @Test
    public void onUrlFocusChange_toolbarVisibleWhenFocused() {
        mMediator.onUrlFocusChange(true);
        assertTrue(mModel.get(NavigationAttachmentsProperties.TOOLBAR_VISIBLE));
    }

    @Test
    public void onUrlFocusChange_toolbarHiddenWhenNotFocused() {
        // Show it first
        mMediator.onUrlFocusChange(true);
        assertTrue(mModel.get(NavigationAttachmentsProperties.TOOLBAR_VISIBLE));

        // Then hide it
        mMediator.onUrlFocusChange(false);
        assertFalse(mModel.get(NavigationAttachmentsProperties.TOOLBAR_VISIBLE));
    }

    @Test
    public void onAddButtonClicked_togglePopup() {
        Runnable runnable = mModel.get(NavigationAttachmentsProperties.BUTTON_ADD_CLICKED);
        assertNotNull(runnable);

        // Show popup.
        doReturn(false).when(mPopup).isShowing();
        runnable.run();
        verify(mPopup).show();

        // Hide popup.
        doReturn(true).when(mPopup).isShowing();
        runnable.run();
        verify(mPopup).dismiss();
    }

    @Test
    public void onCameraButtonClicked_launchesCamera() {
        Runnable runnable = mModel.get(NavigationAttachmentsProperties.POPUP_CAMERA_CLICKED);
        assertNotNull(runnable);

        runnable.run();

        verify(mPopup).dismiss();
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mWindowAndroid).showCancelableIntent(intentCaptor.capture(), any(), any());

        Intent intent = intentCaptor.getValue();
        assertEquals(MediaStore.ACTION_IMAGE_CAPTURE, intent.getAction());
    }

    @Test
    public void onGalleryButtonClicked_launchesImagePicker() {
        Runnable runnable = mModel.get(NavigationAttachmentsProperties.POPUP_GALLERY_CLICKED);
        assertNotNull(runnable);

        runnable.run();

        verify(mPopup).dismiss();
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mWindowAndroid).showCancelableIntent(intentCaptor.capture(), any(), any());

        Intent intent = intentCaptor.getValue();
        assertEquals(Intent.ACTION_GET_CONTENT, intent.getAction());
        assertTrue(intent.hasCategory(Intent.CATEGORY_OPENABLE));
        assertEquals("image/*", intent.getType());
        assertTrue(intent.getBooleanExtra(Intent.EXTRA_ALLOW_MULTIPLE, false));
    }
}
