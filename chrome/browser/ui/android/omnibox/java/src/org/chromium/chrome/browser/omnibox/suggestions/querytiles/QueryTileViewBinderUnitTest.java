// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.querytiles;

import static org.junit.Assert.assertFalse;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
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
        // Note: if the assertion below fails, this test requires additional cases to be covered:
        // - changing to same value,
        // - changing to a different value.
        assertFalse(QueryTileViewProperties.TITLE instanceof WritableObjectPropertyKey);

        mModel =
                new PropertyModel.Builder(QueryTileViewProperties.ALL_UNIQUE_KEYS)
                        .with(QueryTileViewProperties.TITLE, "Title")
                        .build();
        PropertyModelChangeProcessor.create(mModel, mView, QueryTileViewBinder::bind);
        verify(mView).setTitle("Title");
    }

    @Test
    public void setOnFocusViaSelectionListener() {
        // Note: if the assertion below fails, this test requires additional cases to be covered:
        // - changing to same value,
        // - changing to a different value.
        assertFalse(
                QueryTileViewProperties.ON_FOCUS_VIA_SELECTION
                        instanceof WritableObjectPropertyKey);

        Runnable listener = () -> {};
        mModel =
                new PropertyModel.Builder(QueryTileViewProperties.ALL_UNIQUE_KEYS)
                        .with(QueryTileViewProperties.ON_FOCUS_VIA_SELECTION, listener)
                        .build();
        PropertyModelChangeProcessor.create(mModel, mView, QueryTileViewBinder::bind);
        verify(mView).setOnFocusViaSelectionListener(listener);
    }

    @Test
    public void setOnClickListener() {
        // Note: if the assertion below fails, this test requires additional cases to be covered:
        // - changing to same value,
        // - changing to a different value.
        assertFalse(QueryTileViewProperties.ON_CLICK instanceof WritableObjectPropertyKey);

        View.OnClickListener listener = v -> {};
        mModel =
                new PropertyModel.Builder(QueryTileViewProperties.ALL_UNIQUE_KEYS)
                        .with(QueryTileViewProperties.ON_CLICK, listener)
                        .build();
        PropertyModelChangeProcessor.create(mModel, mView, QueryTileViewBinder::bind);
        verify(mView).setOnClickListener(listener);
    }
}
