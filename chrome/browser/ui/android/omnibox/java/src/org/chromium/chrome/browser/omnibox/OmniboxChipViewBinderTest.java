// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import com.google.android.material.button.MaterialButton;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** On-device unit tests for {@link OmniboxChipViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class OmniboxChipViewBinderTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    private MaterialButton mView;
    private PropertyModel mModel;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() {
        runOnUiThreadBlocking(
                () -> {
                    mView =
                            (MaterialButton)
                                    LayoutInflater.from(sActivity)
                                            .inflate(R.layout.omnibox_chip_full, null);
                    mModel = new PropertyModel(OmniboxChipProperties.ALL_KEYS);
                    PropertyModelChangeProcessor.create(mModel, mView, OmniboxChipViewBinder::bind);

                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    FrameLayout.LayoutParams.WRAP_CONTENT,
                                    FrameLayout.LayoutParams.WRAP_CONTENT);
                    sActivity.setContentView(mView, params);
                });
    }

    @Test
    @SmallTest
    public void testText() {
        String text = "test text";
        runOnUiThreadBlocking(() -> mModel.set(OmniboxChipProperties.TEXT, text));
        assertEquals(text, mView.getText().toString());
    }

    @Test
    @SmallTest
    public void testIcon() {
        Drawable icon = sActivity.getDrawable(android.R.drawable.ic_menu_add);
        runOnUiThreadBlocking(() -> mModel.set(OmniboxChipProperties.ICON, icon));
        assertEquals(icon, mView.getIcon());
    }

    @Test
    @SmallTest
    public void testContentDescription() {
        String contentDesc = "content description";
        runOnUiThreadBlocking(() -> mModel.set(OmniboxChipProperties.CONTENT_DESC, contentDesc));
        assertEquals(contentDesc, mView.getContentDescription().toString());
    }

    @Test
    @SmallTest
    public void testOnClick() {
        var onClick = Mockito.mock(Runnable.class);
        runOnUiThreadBlocking(() -> mModel.set(OmniboxChipProperties.ON_CLICK, onClick));
        runOnUiThreadBlocking(() -> mView.performClick());
        verify(onClick).run();
    }

    @Test
    @SmallTest
    public void testAvailableWidth() {
        runOnUiThreadBlocking(() -> mModel.set(OmniboxChipProperties.AVAILABLE_WIDTH, 100));
        assertEquals(View.VISIBLE, mView.getVisibility());
        assertEquals(100, mView.getMaxWidth());

        runOnUiThreadBlocking(() -> mModel.set(OmniboxChipProperties.AVAILABLE_WIDTH, 0));
        assertEquals(View.GONE, mView.getVisibility());
    }
}
