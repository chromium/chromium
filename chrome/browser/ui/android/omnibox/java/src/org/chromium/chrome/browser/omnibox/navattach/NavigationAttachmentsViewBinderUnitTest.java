// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import androidx.constraintlayout.widget.Group;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ChromeImageButton;

/** Unit tests for {@link NavigationAttachmentsViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NavigationAttachmentsViewBinderUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock ViewGroup mParent;
    private @Mock Group mNavigationView;
    private @Mock NavigationAttachmentsPopup mPopup;
    private @Mock ChromeImageButton mAddButton;
    private @Mock Button mCameraButton;
    private @Mock Button mGalleryButton;

    private PropertyModel mModel;
    private NavigationAttachmentsViewHolder mViewHolder;

    @Before
    public void setUp() {
        doReturn(mNavigationView).when(mParent).findViewById(R.id.location_bar_navigation_toolbar);
        doReturn(mAddButton).when(mParent).findViewById(R.id.location_bar_attachments_add);
        mModel = new PropertyModel(NavigationAttachmentsProperties.ALL_KEYS);
        mViewHolder = new NavigationAttachmentsViewHolder(mParent, mPopup);
        mViewHolder.popup.mCameraButton = mCameraButton;
        mViewHolder.popup.mGalleryButton = mGalleryButton;
        PropertyModelChangeProcessor.create(
                mModel, mViewHolder, NavigationAttachmentsViewBinder::bind);
    }

    @Test
    public void toolbarVisible_setsVisibility() {
        mModel.set(NavigationAttachmentsProperties.TOOLBAR_VISIBLE, true);
        verify(mNavigationView).setVisibility(View.VISIBLE);

        mModel.set(NavigationAttachmentsProperties.TOOLBAR_VISIBLE, false);
        verify(mNavigationView).setVisibility(View.GONE);
    }

    @Test
    public void addButtonClickListener_isCalled() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(NavigationAttachmentsProperties.BUTTON_ADD_CLICKED, runnable);

        ArgumentCaptor<View.OnClickListener> listenerCaptor =
                ArgumentCaptor.forClass(View.OnClickListener.class);
        verify(mAddButton).setOnClickListener(listenerCaptor.capture());
        listenerCaptor.getValue().onClick(mAddButton);

        verify(runnable).run();
    }

    @Test
    public void cameraButtonClickListener_isCalled() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(NavigationAttachmentsProperties.POPUP_CAMERA_CLICKED, runnable);

        ArgumentCaptor<View.OnClickListener> listenerCaptor =
                ArgumentCaptor.forClass(View.OnClickListener.class);
        verify(mCameraButton).setOnClickListener(listenerCaptor.capture());
        listenerCaptor.getValue().onClick(mCameraButton);

        verify(runnable).run();
    }

    @Test
    public void galleryButtonClickListener_isCalled() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(NavigationAttachmentsProperties.POPUP_GALLERY_CLICKED, runnable);

        ArgumentCaptor<View.OnClickListener> listenerCaptor =
                ArgumentCaptor.forClass(View.OnClickListener.class);
        verify(mGalleryButton).setOnClickListener(listenerCaptor.capture());
        listenerCaptor.getValue().onClick(mGalleryButton);

        verify(runnable).run();
    }
}
