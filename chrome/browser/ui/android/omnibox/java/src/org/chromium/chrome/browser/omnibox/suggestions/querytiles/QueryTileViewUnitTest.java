// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.querytiles;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.Context;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.test.R;

/** Tests for {@link QueryTileView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class QueryTileViewUnitTest {
    private Context mContext;
    private QueryTileView mView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mView = new QueryTileView(mContext);
    }

    @Test
    public void setSelected_altersFocusedStateForKeyboardNavigation() {
        assertFalse(mView.isFocused());

        mView.setSelected(true);
        assertTrue(mView.isFocused());

        mView.setSelected(false);
        assertFalse(mView.isFocused());
    }

    @Test
    public void setImage() {
        Drawable d1 = new ColorDrawable();
        Drawable d2 = new ColorDrawable();
        ImageView thumbnail = mView.findViewById(R.id.thumbnail);

        mView.setImage(d1);
        assertEquals(d1, thumbnail.getDrawable());

        mView.setImage(d2);
        assertEquals(d2, thumbnail.getDrawable());

        mView.setImage(null);
        assertEquals(null, thumbnail.getDrawable());
    }

    @Test
    public void setText() {
        TextView title = mView.findViewById(R.id.title);

        mView.setTitle("aa");
        assertEquals("aa", title.getText());

        mView.setTitle("title");
        assertEquals("title", title.getText());

        mView.setTitle(null);
        assertTrue(TextUtils.isEmpty(title.getText()));
    }

    @Test
    public void setSelected_safeWithNoFocusListener() {
        mView.setSelected(true);
        mView.setSelected(false);
    }

    @Test
    public void setSelected_invokesFocusListenerOnlyWhenSelected() {
        Runnable listener = mock(Runnable.class);
        mView.setOnFocusViaSelectionListener(listener);

        // Select (passed through).
        mView.setSelected(true);
        verify(listener).run();
        clearInvocations(listener);

        // Deslect (ignored).
        mView.setSelected(false);
        verifyNoMoreInteractions(listener);

        // Select (passed through again).
        mView.setSelected(true);
        verify(listener).run();
    }

    @Test
    public void click_safeWithNoClickListener() {
        mView.performClick();
    }

    @Test
    public void click_invokesClickListenerWhenClicked() {
        View.OnClickListener listener = mock(View.OnClickListener.class);
        mView.setOnClickListener(listener);

        mView.performClick();
        verify(listener).onClick(mView);
    }
}
