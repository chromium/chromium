// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.BODY;
import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.ILLUSTRATION;
import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.IS_BUTTON_BAR_VISIBLE;
import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.IS_ILLUSTRATION_VISIBLE;
import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.ON_NEGATIVE_CLICKED;
import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.ON_POSITIVE_CLICKED;
import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.TITLE;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.text.method.LinkMovementMethod;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

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

/** Unit tests for {@link BottomBarEducationContentViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomBarEducationContentViewBinderUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mOnPositiveClicked;
    @Mock private Runnable mOnNegativeClicked;

    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void testIllustration() {
        Drawable drawable = new ColorDrawable(Color.RED);
        PropertyModel model =
                new PropertyModel.Builder(BottomBarEducationContentProperties.ALL_KEYS)
                        .with(ILLUSTRATION, drawable)
                        .with(IS_ILLUSTRATION_VISIBLE, true)
                        .build();
        ImageView illustration = bind(model).findViewById(R.id.education_illustration);
        assertSame(drawable, illustration.getDrawable());
        assertEquals(View.VISIBLE, illustration.getVisibility());

        model.set(IS_ILLUSTRATION_VISIBLE, false);
        assertEquals(View.GONE, illustration.getVisibility());

        model.set(IS_ILLUSTRATION_VISIBLE, true);
        assertEquals(View.VISIBLE, illustration.getVisibility());
    }

    @Test
    public void testTitleAndBody() {
        View view =
                bind(
                        new PropertyModel.Builder(BottomBarEducationContentProperties.ALL_KEYS)
                                .with(TITLE, "Title")
                                .with(BODY, "Body")
                                .build());

        TextView title = view.findViewById(R.id.education_title);
        assertEquals("Title", title.getText().toString());

        TextView body = view.findViewById(R.id.education_body);
        assertEquals("Body", body.getText().toString());
        // Needed for the settings link in the body to be clickable.
        assertTrue(body.getMovementMethod() instanceof LinkMovementMethod);
    }

    @Test
    public void testButtonBar() {
        View view =
                bind(
                        new PropertyModel.Builder(BottomBarEducationContentProperties.ALL_KEYS)
                                .with(IS_BUTTON_BAR_VISIBLE, true)
                                .with(ON_POSITIVE_CLICKED, mOnPositiveClicked)
                                .with(ON_NEGATIVE_CLICKED, mOnNegativeClicked)
                                .build());
        assertEquals(View.VISIBLE, view.findViewById(R.id.button_bar).getVisibility());

        view.findViewById(R.id.positive_button).performClick();
        verify(mOnPositiveClicked).run();

        view.findViewById(R.id.negative_button).performClick();
        verify(mOnNegativeClicked).run();
    }

    @Test
    public void testButtonBarHidden() {
        View view =
                bind(
                        new PropertyModel.Builder(BottomBarEducationContentProperties.ALL_KEYS)
                                .with(IS_BUTTON_BAR_VISIBLE, false)
                                .build());
        assertEquals(View.GONE, view.findViewById(R.id.button_bar).getVisibility());
    }

    private View bind(PropertyModel model) {
        View view =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.bottom_bar_education_content, /* root= */ null);
        PropertyModelChangeProcessor.create(model, view, BottomBarEducationContentViewBinder::bind);
        return view;
    }
}
