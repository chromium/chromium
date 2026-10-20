// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.textbubble;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Rect;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.widget.ChromePopupWindow;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.UiWidgetFactory;

/** Unit tests for {@link TextBubble}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TextBubbleUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private UiWidgetFactory mMockUiWidgetFactory;

    private Activity mActivity;
    private FrameLayout mRootView;
    private ChromePopupWindow mSpyPopupWindow;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mRootView = new FrameLayout(mActivity);
        mActivity.setContentView(
                mRootView,
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
        mActivity.getWindow().getDecorView().layout(0, 0, 1000, 1000);
        mRootView.layout(0, 0, 1000, 1000);

        mSpyPopupWindow = Mockito.spy(UiWidgetFactory.getInstance().createPopupWindow(mActivity));
        UiWidgetFactory.setInstance(mMockUiWidgetFactory);
        Mockito.when(mMockUiWidgetFactory.createPopupWindow(any())).thenReturn(mSpyPopupWindow);

        TextBubble.setSkipShowCheckForTesting(true);
    }

    @After
    public void tearDown() {
        TextBubble.dismissBubbles();
        UiWidgetFactory.setInstance(null);
        mActivity.finish();
    }

    @Test
    public void testCreateAndShow_createsContentViewOnDemand() {
        RectProvider anchorRectProvider = new RectProvider(new Rect(100, 100, 200, 200));
        TextBubble textBubble =
                new TextBubble.Builder(
                                mActivity,
                                mRootView,
                                anchorRectProvider,
                                "This is a long test string to exceed minimum size",
                                "This is a long test string to exceed minimum size")
                        .build();

        assertNull(textBubble.getTextBubbleContentViewForTesting());

        textBubble.show();

        View contentView = textBubble.getTextBubbleContentViewForTesting();
        assertNotNull(contentView);
        verify(mSpyPopupWindow).setContentView(contentView);
    }

    @Test
    public void testReShowAfterDismiss_rebuildsContentView() {
        RectProvider anchorRectProvider = new RectProvider(new Rect(100, 100, 200, 200));
        TextBubble textBubble =
                new TextBubble.Builder(
                                mActivity,
                                mRootView,
                                anchorRectProvider,
                                "This is a long test string to exceed minimum size",
                                "This is a long test string to exceed minimum size")
                        .build();

        textBubble.show();
        View firstView = textBubble.getTextBubbleContentViewForTesting();
        assertNotNull(firstView);

        textBubble.dismiss();
        assertNull(textBubble.getTextBubbleContentViewForTesting());

        textBubble.show();
        View secondView = textBubble.getTextBubbleContentViewForTesting();
        assertNotNull(secondView);
        assertNotSame(firstView, secondView);
    }

    @Test
    public void testSetMaxWidth_constrainsBubbleWidth() {
        RectProvider anchorRectProvider = new RectProvider(new Rect(100, 100, 200, 200));
        int maxWidthPx = 350;
        TextBubble textBubble =
                new TextBubble.Builder(
                                mActivity,
                                mRootView,
                                anchorRectProvider,
                                "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do"
                                        + " eiusmod tempor incididunt ut labore et dolore magna"
                                        + " aliqua.",
                                "Lorem ipsum dolor sit amet.")
                        .setMaxWidth(maxWidthPx)
                        .build();

        textBubble.show();

        assertTrue(mSpyPopupWindow.getWidth() <= maxWidthPx);
    }
}
