// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link NavigationAttachmentsMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NavigationAttachmentsMediatorUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock ViewGroup mViewGroup;
    private @Mock NavigationAttachmentsViewHolder mViewHolder;
    private @Mock NavigationAttachmentsPopup mPopup;

    private Context mContext;
    private PropertyModel mModel;
    private NavigationAttachmentsMediator mMediator;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mModel = new PropertyModel(NavigationAttachmentsProperties.ALL_KEYS);
        mViewHolder = new NavigationAttachmentsViewHolder(mViewGroup, mPopup);
        mMediator = new NavigationAttachmentsMediator(mModel, mViewHolder);
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
}
