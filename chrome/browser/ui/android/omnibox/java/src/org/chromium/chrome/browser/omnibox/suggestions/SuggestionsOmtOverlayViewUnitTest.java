// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.os.Build;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdown.SuggestionLayoutScrollListener;
import org.chromium.components.omnibox.OmniboxFeatureList;

@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.BAKLAVA)
@EnableFeatures(OmniboxFeatureList.OMNIBOX_PREFETCH_SELECTED_SUGGESTIONS_OMT_ANDROID)
public class SuggestionsOmtOverlayViewUnitTest {
    private Context mContext;
    private OmniboxSuggestionsDropdown mDropdown;
    private SuggestionsOmtOverlayView mOverlayView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mDropdown =
                new OmniboxSuggestionsDropdown(
                        mContext, null, new SuggestionLayoutScrollListener(mContext));
        mOverlayView = new SuggestionsOmtOverlayView(mContext, mDropdown);
    }

    @After
    public void tearDown() {
        mOverlayView.destroy();
        mDropdown.destroy();
    }

    @Test
    public void testOverlayLifecycle_threadAndAttachment() throws Exception {
        var parent = new FrameLayout(mContext);

        assertNotNull(mOverlayView.getInputThreadForTesting());
        assertNotNull(mOverlayView.getInputHandlerForTesting());
        assertTrue(mOverlayView.getInputThreadForTesting().isAlive());

        mOverlayView.attachToParent(parent);
        assertEquals(1, parent.getChildCount());

        mOverlayView.onTouchTransferred();
        assertTrue(mOverlayView.isTouchTransferActive());

        mOverlayView.destroy();
        assertEquals(0, parent.getChildCount());
        assertFalse(mOverlayView.isTouchTransferActive());
        mOverlayView.getInputThreadForTesting().join();
        assertFalse(mOverlayView.getInputThreadForTesting().isAlive());
    }

    @Test
    public void testOverlayBounds_initialAttachment() {
        var parent = new FrameLayout(mContext);

        mDropdown.layout(0, 0, 100, 200);
        mOverlayView.attachToParent(parent);

        var layoutParams = mOverlayView.getLayoutParams();
        assertNotNull(layoutParams);
        assertEquals(100, layoutParams.width);
        assertEquals(200, layoutParams.height);
        assertEquals(View.GONE, mOverlayView.getVisibility());
    }

    @Test
    public void testOverlayTouchTransfer_stateManagement() {
        assertFalse(mOverlayView.isTouchTransferActive());

        mOverlayView.onTouchTransferred();
        assertTrue(mOverlayView.isTouchTransferActive());

        mOverlayView.resetTouchTransfer();
        assertFalse(mOverlayView.isTouchTransferActive());

        mOverlayView.onTouchTransferred();
        assertTrue(mOverlayView.isTouchTransferActive());

        mOverlayView.onGestureEnded();
        RobolectricUtil.runAllBackgroundAndUi();
        assertFalse(mOverlayView.isTouchTransferActive());

        mOverlayView.onTouchTransferred();
        assertTrue(mOverlayView.isTouchTransferActive());

        mOverlayView.surfaceDestroyed(mOverlayView.getHolder());
        assertFalse(mOverlayView.isTouchTransferActive());
    }
}
