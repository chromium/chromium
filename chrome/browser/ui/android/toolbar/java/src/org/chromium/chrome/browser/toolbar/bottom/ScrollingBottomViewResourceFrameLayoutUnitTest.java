// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.ui.resources.Resource;
import org.chromium.ui.resources.ResourceFactory;
import org.chromium.ui.resources.ResourceFactoryJni;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/** Unit tests for {@link ScrollingBottomViewResourceFrameLayout}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ScrollingBottomViewResourceFrameLayoutUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ResourceFactory.Natives mResourceFactoryJni;
    @Mock private Callback<Resource> mOnResourceReady;

    private ScrollingBottomViewResourceFrameLayout mLayout;
    private ViewResourceAdapter mAdapter;

    @Before
    public void setUp() {
        ResourceFactoryJni.setInstanceForTesting(mResourceFactoryJni);
        Context context = ApplicationProvider.getApplicationContext();
        mLayout = new ScrollingBottomViewResourceFrameLayout(context, null);
        View shadow = new View(context);
        shadow.setId(R.id.bottom_container_top_shadow);
        mLayout.addView(shadow);
        mLayout.onFinishInflate();
        mAdapter = mLayout.getResourceAdapter();
        mAdapter.addOnResourceReadyCallback(mOnResourceReady);
    }

    @Test
    public void testRecaptureAfterSizeChange() {
        mLayout.layout(0, 0, 100, 56);
        mLayout.onModelTokenChange(new Object());
        mAdapter.onResourceRequested();
        verify(mOnResourceReady).onResult(any());

        mLayout.layout(0, 0, 100, 64);
        verify(mOnResourceReady, times(2)).onResult(any());

        mLayout.layout(0, 0, 100, 64);
        mAdapter.onResourceRequested();
        verify(mOnResourceReady, times(2)).onResult(any());
    }

    @Test
    public void testSizeChangeWhileInvisibleDefersCapture() {
        mLayout.layout(0, 0, 100, 56);
        mLayout.onModelTokenChange(new Object());
        mAdapter.onResourceRequested();
        verify(mOnResourceReady).onResult(any());

        mLayout.setVisibility(View.INVISIBLE);
        mLayout.layout(0, 0, 100, 64);
        verify(mOnResourceReady).onResult(any());

        mAdapter.onResourceRequested();
        verify(mOnResourceReady, times(2)).onResult(any());
    }
}
