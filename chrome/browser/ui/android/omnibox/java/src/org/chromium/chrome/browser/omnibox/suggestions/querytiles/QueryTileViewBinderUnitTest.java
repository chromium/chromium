// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.querytiles;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;

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

/** Tests for {@link QueryTileViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class QueryTileViewBinderUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock QueryTileView mView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mModel = new PropertyModel(QueryTileViewProperties.ALL_UNIQUE_KEYS);
        PropertyModelChangeProcessor.create(mModel, mView, QueryTileViewBinder::bind);
    }

    @Test
    public void setImage() {
        Drawable d1 = new ColorDrawable();
        Drawable d2 = new ColorDrawable();

        mModel.set(QueryTileViewProperties.IMAGE, d1);
        verify(mView).setImage(d1);

        mModel.set(QueryTileViewProperties.IMAGE, d1);
        verifyNoMoreInteractions(mView);

        mModel.set(QueryTileViewProperties.IMAGE, d2);
        verify(mView).setImage(d2);

        mModel.set(QueryTileViewProperties.IMAGE, null);
        verify(mView).setImage(null);

        verifyNoMoreInteractions(mView);
    }

    @Test
    public void setTitle() {
        mModel.set(QueryTileViewProperties.TITLE, "Title");
        verify(mView).setTitle("Title");

        mModel.set(QueryTileViewProperties.TITLE, "Title");
        verifyNoMoreInteractions(mView);

        mModel.set(QueryTileViewProperties.TITLE, "Title2");
        verify(mView).setTitle("Title2");

        mModel.set(QueryTileViewProperties.TITLE, null);
        verify(mView).setTitle(null);
    }
}
