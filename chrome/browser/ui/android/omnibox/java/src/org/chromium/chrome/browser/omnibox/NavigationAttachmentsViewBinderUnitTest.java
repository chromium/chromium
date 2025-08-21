// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.view.View;
import android.view.ViewGroup;

import androidx.constraintlayout.widget.Group;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link NavigationAttachmentsViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NavigationAttachmentsViewBinderUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock ViewGroup mParent;
    private @Mock Group mNavigationView;

    private PropertyModel mModel;
    private NavigationAttachmentsViewHolder mViewHolder;

    @Before
    public void setUp() {
        doReturn(mNavigationView).when(mParent).findViewById(R.id.location_bar_navigation_toolbar);
        mModel = new PropertyModel(NavigationAttachmentsProperties.ALL_KEYS);
        mViewHolder = new NavigationAttachmentsViewHolder(mParent);
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
}
