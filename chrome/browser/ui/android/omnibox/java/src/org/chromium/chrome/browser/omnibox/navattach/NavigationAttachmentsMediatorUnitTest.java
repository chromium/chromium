// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.Intent;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Unit tests for {@link NavigationAttachmentsMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NavigationAttachmentsMediatorUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock ViewGroup mViewGroup;
    private @Mock NavigationAttachmentsViewHolder mViewHolder;
    private @Mock NavigationAttachmentsPopup mPopup;
    private @Mock WindowAndroid mWindowAndroid;

    private Context mContext;
    private PropertyModel mModel;
    private NavigationAttachmentsMediator mMediator;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mModel = new PropertyModel(NavigationAttachmentsProperties.ALL_KEYS);
        mViewHolder = new NavigationAttachmentsViewHolder(mViewGroup, mPopup);
        mMediator =
                Mockito.spy(
                        new NavigationAttachmentsMediator(
                                mContext, mWindowAndroid, mModel, mViewHolder, new ModelList()));
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
    public void onCameraClicked_permissionGranted_launchesCamera() {
        doReturn(true).when(mWindowAndroid).hasPermission(any());
        doNothing().when(mMediator).launchCamera();

        mMediator.onCameraClicked();

        verify(mMediator).launchCamera();
        verify(mWindowAndroid, never()).requestPermissions(any(), any());
    }

    @Test
    public void onCameraClicked_permissionDenied_requestsPermission() {
        doReturn(false).when(mWindowAndroid).hasPermission(any());
        doNothing().when(mMediator).launchCamera();

        mMediator.onCameraClicked();

        verify(mMediator, never()).launchCamera();
        verify(mWindowAndroid).requestPermissions(any(), any());
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

    @Test
    public void onFileButtonClicked_launchesFilePicker() {
        Runnable runnable = mModel.get(NavigationAttachmentsProperties.POPUP_FILE_CLICKED);
        assertNotNull(runnable);

        runnable.run();

        verify(mPopup).dismiss();
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mWindowAndroid).showCancelableIntent(intentCaptor.capture(), any(), any());

        Intent intent = intentCaptor.getValue();
        assertEquals(Intent.ACTION_OPEN_DOCUMENT, intent.getAction());
        assertTrue(intent.hasCategory(Intent.CATEGORY_OPENABLE));
        assertEquals("*/*", intent.getType());
        assertTrue(intent.getBooleanExtra(Intent.EXTRA_ALLOW_MULTIPLE, false));
    }

    @Test
    public void addAttachment_setsAttachmentsVisible() {
        mMediator.addAttachment(null, "title", "description");
        assertTrue(mModel.get(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE));
    }

    @Test
    public void onUseAiModeChanged_off_clearsAttachments() {
        ModelList modelList = new ModelList();
        mMediator =
                new NavigationAttachmentsMediator(
                        mContext, mWindowAndroid, mModel, mViewHolder, modelList);
        modelList.add(new SimpleRecyclerViewAdapter.ListItem(0, new PropertyModel()));
        assertEquals(1, modelList.size());

        mModel.set(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE, true);
        mMediator.onUseAiModeChanged(false);
        assertFalse(mModel.get(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE));
        assertEquals(0, modelList.size());
    }
}
