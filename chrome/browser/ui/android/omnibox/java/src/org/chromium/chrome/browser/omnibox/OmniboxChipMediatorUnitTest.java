// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;

import android.graphics.drawable.Drawable;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link OmniboxChipMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxChipMediatorUnitTest {
    private PropertyModel mModel;
    private OmniboxChipMediator mMediator;

    @Before
    public void setUp() {
        mModel = new PropertyModel(OmniboxChipProperties.ALL_KEYS);
        mMediator = new OmniboxChipMediator(mModel);
    }

    @Test
    public void updateChip() {
        String text = "text";
        Drawable icon = mock(Drawable.class);
        String contentDesc = "contentDesc";
        Runnable onClick = () -> {};

        mMediator.updateChip(text, icon, contentDesc, onClick);

        assertEquals(text, mModel.get(OmniboxChipProperties.TEXT));
        assertEquals(icon, mModel.get(OmniboxChipProperties.ICON));
        assertEquals(contentDesc, mModel.get(OmniboxChipProperties.CONTENT_DESC));
        assertEquals(onClick, mModel.get(OmniboxChipProperties.ON_CLICK));
    }

    @Test
    public void setAvailableWidth() {
        int width = 100;
        mMediator.setAvailableWidth(width);
        assertEquals(width, mModel.get(OmniboxChipProperties.AVAILABLE_WIDTH));
    }
}
