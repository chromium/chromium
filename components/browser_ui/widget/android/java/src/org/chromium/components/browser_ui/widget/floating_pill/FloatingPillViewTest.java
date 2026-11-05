// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.floating_pill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.PointerIcon;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.R;

/** Unit tests for {@link FloatingPillView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FloatingPillViewTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View.OnClickListener mOnClickListener;

    private Activity mActivity;
    private Resources mResources;
    private FloatingPillView mPill;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mResources = mActivity.getResources();
        mPill = new FloatingPillView(mActivity);
    }

    @Test
    public void testDefaultLook() {
        assertEquals(
                mResources.getDimensionPixelSize(R.dimen.floating_pill_height),
                mPill.getMinimumHeight());
        int padding = mResources.getDimensionPixelSize(R.dimen.floating_pill_padding_horizontal);
        assertEquals(padding, mPill.getPaddingStart());
        assertEquals(padding, mPill.getPaddingEnd());
        assertEquals(LinearLayout.HORIZONTAL, mPill.getOrientation());
        assertNotNull(mPill.getBackground());
    }

    @Test
    public void testXmlAttributesOverrideTheDefaultLook() {
        AttributeSet attrs =
                Robolectric.buildAttributeSet()
                        .addAttribute(android.R.attr.minHeight, "40dp")
                        .addAttribute(android.R.attr.paddingStart, "4dp")
                        .build();
        FloatingPillView pill = new FloatingPillView(mActivity, attrs);

        float density = mResources.getDisplayMetrics().density;
        assertEquals(Math.round(40 * density), pill.getMinimumHeight());
        assertEquals(Math.round(4 * density), pill.getPaddingStart());
    }

    @Test
    public void testAddButton() {
        Drawable drawable = new ColorDrawable();
        ImageButton button = mPill.addButton(drawable, "Pause", mOnClickListener);

        assertSame(button, mPill.getChildAt(mPill.getChildCount() - 1));
        assertSame(drawable, button.getDrawable());
        assertEquals("Pause", button.getContentDescription());
        assertEquals("Pause", button.getTooltipText());
        assertEquals(
                PointerIcon.getSystemIcon(mActivity, PointerIcon.TYPE_HAND),
                button.getPointerIcon());

        button.performClick();
        verify(mOnClickListener).onClick(button);
    }

    @Test
    public void testContentStaysBeforeButtons() {
        View content = new View(mActivity);
        mPill.addView(content);
        ImageButton first = mPill.addButton(new ColorDrawable(), "First", mOnClickListener);
        ImageButton second = mPill.addButton(new ColorDrawable(), "Second", mOnClickListener);

        // Content added after the buttons still goes before them.
        View laterContent = new View(mActivity);
        mPill.addView(laterContent);
        assertEquals(4, mPill.getChildCount());
        assertSame(content, mPill.getChildAt(0));
        assertSame(laterContent, mPill.getChildAt(1));
        assertSame(first, mPill.getChildAt(2));
        assertSame(second, mPill.getChildAt(3));

        // Once the buttons are removed, content goes at the end again.
        mPill.removeView(first);
        mPill.removeView(second);
        View lastContent = new View(mActivity);
        mPill.addView(lastContent);
        assertSame(lastContent, mPill.getChildAt(mPill.getChildCount() - 1));
    }
}
