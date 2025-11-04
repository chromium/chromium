// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.app.Activity;
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
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageView;

/** Unit tests for {@link NavigationAttachmentsViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NavigationAttachmentsViewBinderUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock ViewGroup mParent;
    private @Mock Group mAttachmentsToolbar;
    private @Mock NavigationAttachmentsPopup mPopup;
    private @Mock ChromeImageView mAddButton;
    private @Mock ChromeImageView mSettingsButton;
    private @Mock Button mCameraButton;
    private @Mock Button mGalleryButton;
    private @Mock Button mFileButton;
    private @Mock NavigationAttachmentsRecyclerView mRecyclerView;
    private @Mock ButtonCompat mRequestType;
    private @Mock View mRecentTabsHeader;
    private @Mock ViewGroup mPopupRequestTypeGroup;

    private Activity mActivity;
    private PropertyModel mModel;
    private NavigationAttachmentsViewHolder mViewHolder;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();

        doReturn(mActivity).when(mParent).getContext();
        // Please use parentView.getResources() in ViewBinder.
        doReturn(mActivity.getResources()).when(mParent).getResources();

        doReturn(mAttachmentsToolbar)
                .when(mParent)
                .findViewById(R.id.location_bar_attachments_toolbar);
        doReturn(mAddButton).when(mParent).findViewById(R.id.location_bar_attachments_add);
        doReturn(mSettingsButton)
                .when(mParent)
                .findViewById(R.id.location_bar_attachments_settings);
        doReturn(mRecyclerView).when(mParent).findViewById(R.id.location_bar_attachments);
        doReturn(mRequestType).when(mParent).findViewById(R.id.fusebox_request_type);
        doReturn(mRecentTabsHeader).when(mParent).findViewById(R.id.fusebox_recent_tabs_header);
        mModel = new PropertyModel(NavigationAttachmentsProperties.ALL_KEYS);
        mViewHolder = new NavigationAttachmentsViewHolder(mParent, mPopup);
        mViewHolder.popup.mCameraButton = mCameraButton;
        mViewHolder.popup.mGalleryButton = mGalleryButton;
        mViewHolder.popup.mFileButton = mFileButton;
        mViewHolder.popup.mRecentTabsHeader = mRecentTabsHeader;
        mViewHolder.popup.mAutocompleteRequestTypeGroup = mPopupRequestTypeGroup;
        PropertyModelChangeProcessor.create(
                mModel, mViewHolder, NavigationAttachmentsViewBinder::bind);
    }

    @Test
    public void toolbarVisible_setsVisibility() {
        mModel.set(
                NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE,
                AutocompleteRequestType.AI_MODE);
        mModel.set(NavigationAttachmentsProperties.ATTACHMENTS_TOOLBAR_VISIBLE, true);
        verify(mAttachmentsToolbar).setVisibility(View.VISIBLE);

        mModel.set(NavigationAttachmentsProperties.ATTACHMENTS_TOOLBAR_VISIBLE, false);
        verify(mAttachmentsToolbar).setVisibility(View.GONE);
    }

    @Test
    public void attachmentsVisible_setsVisibilityAndTogglesSwitch() {
        mModel.set(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE, true);
        verify(mRecyclerView).setVisibility(View.VISIBLE);

        mModel.set(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE, false);
        verify(mRecyclerView).setVisibility(View.GONE);
    }

    @Test
    public void adapter_isSet() {
        SimpleRecyclerViewAdapter adapter = mock(SimpleRecyclerViewAdapter.class);
        mModel.set(NavigationAttachmentsProperties.ADAPTER, adapter);
        verify(mRecyclerView).setAdapter(adapter);
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

    @Test
    public void fileButtonClickListener_isCalled() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(NavigationAttachmentsProperties.POPUP_FILE_CLICKED, runnable);

        ArgumentCaptor<View.OnClickListener> listenerCaptor =
                ArgumentCaptor.forClass(View.OnClickListener.class);
        verify(mFileButton).setOnClickListener(listenerCaptor.capture());
        listenerCaptor.getValue().onClick(mFileButton);

        verify(runnable).run();
    }

    @Test
    public void recentTabsHeader() {
        mModel.set(NavigationAttachmentsProperties.RECENT_TABS_HEADER_VISIBLE, true);
        verify(mPopup.mRecentTabsHeader).setVisibility(View.VISIBLE);
        mModel.set(NavigationAttachmentsProperties.RECENT_TABS_HEADER_VISIBLE, false);
        verify(mPopup.mRecentTabsHeader).setVisibility(View.GONE);
    }

    @Test
    public void autocompleteRequestTypeClicked_setsListener() {
        Runnable runnable = mock(Runnable.class);
        mModel.set(NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE_CLICKED, runnable);
        verify(mRequestType).setOnClickListener(any());
    }
}
