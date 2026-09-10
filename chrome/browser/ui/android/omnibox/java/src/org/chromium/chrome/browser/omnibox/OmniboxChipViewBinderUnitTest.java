// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.LayoutInflater;

import com.google.android.material.button.MaterialButton;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link OmniboxChipViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxChipViewBinderUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private Runnable mRunnable;

    private Activity mActivity;
    private MaterialButton mView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mView =
                (MaterialButton)
                        LayoutInflater.from(mActivity).inflate(R.layout.omnibox_chip_full, null);
        mModel = new PropertyModel(OmniboxChipProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mView, OmniboxChipViewBinder::bind);
    }

    @Test
    public void testTextAndAvailableWidth() {
        String text = "test text";
        mModel.set(OmniboxChipProperties.AVAILABLE_WIDTH, 0);
        mModel.set(OmniboxChipProperties.TEXT, text);
        // Text should be empty if available width is 0.
        assertTrue(TextUtils.isEmpty(mView.getText().toString()));

        mModel.set(OmniboxChipProperties.AVAILABLE_WIDTH, 1000);
        assertEquals(text, mView.getText().toString());
    }

    @Test
    public void testIcon() {
        Drawable icon = mActivity.getDrawable(android.R.drawable.ic_menu_add);
        mModel.set(OmniboxChipProperties.ICON, icon);
        assertNonNull(mView.getIcon());
    }

    @Test
    public void testContentDescription() {
        String contentDesc = "content description";
        mModel.set(OmniboxChipProperties.CONTENT_DESC, contentDesc);
        assertEquals(contentDesc, mView.getContentDescription().toString());
    }

    @Test
    public void testOnClick() {
        mModel.set(OmniboxChipProperties.ON_CLICK, mRunnable);
        mView.performClick();
        verify(mRunnable).run();
    }
}
