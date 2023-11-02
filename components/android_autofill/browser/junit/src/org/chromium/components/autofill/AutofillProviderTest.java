// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Rect;
import android.graphics.RectF;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;

import java.util.ArrayList;

/**
 * The unit tests for AutofillProvider.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AutofillProviderTest {
    private static final float EXPECTED_DIP_SCALE = 2;
    private static final int SCROLL_X = 15;
    private static final int SCROLL_Y = 155;
    private static final int LOCATION_X = 25;
    private static final int LOCATION_Y = 255;

    private Context mContext;
    private WindowAndroid mWindowAndroid;
    private WebContents mWebContents;
    private ViewGroup mContainerView;
    private AutofillProvider mAutofillProvider;
    private DisplayAndroid mDisplayAndroid;
    private long mMockedNativeAutofillProviderAndroid = 1;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = Mockito.mock(Context.class);
        mWindowAndroid = Mockito.mock(WindowAndroid.class);
        mDisplayAndroid = Mockito.mock(DisplayAndroid.class);
        mWebContents = Mockito.mock(WebContents.class);
        mContainerView = Mockito.mock(ViewGroup.class);

        mAutofillProvider = new AutofillProvider(
                mContext, mContainerView, mWebContents, "AutofillProviderTest") {
            @Override
            protected long initializeNativeAutofillProvider(WebContents webContents) {
                return mMockedNativeAutofillProviderAndroid;
            }
        };

        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getDisplay()).thenReturn(mDisplayAndroid);
        when(mDisplayAndroid.getDipScale()).thenReturn(EXPECTED_DIP_SCALE);
        when(mContainerView.getScrollX()).thenReturn(SCROLL_X);
        when(mContainerView.getScrollY()).thenReturn(SCROLL_Y);
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                Object[] args = invocation.getArguments();
                int[] location = (int[]) args[0];
                location[0] = LOCATION_X;
                location[1] = LOCATION_Y;
                return null;
            }
        })
                .when(mContainerView)
                .getLocationOnScreen(ArgumentMatchers.any());
    }

    @Test
    public void testTransformFormFieldToContainViewCoordinates() {
        ArrayList<FormFieldData> fields = new ArrayList<FormFieldData>(1);
        fields.add(FormFieldData.createFormFieldData(null, null, null, null, false, null, null,
                null, null, null, false, false, 0, null, null, null, null, 10 /* left */,
                20 /* top */, 300 /* right */, 60 /*bottom*/, null, null, true));
        fields.add(FormFieldData.createFormFieldData(null, null, null, null, false, null, null,
                null, null, null, false, false, 0, null, null, null, null, 20 /* left */,
                100 /* top */, 400 /* right */, 200 /*bottom*/, null, null, true));
        FormData formData = new FormData(null, null, fields);
        mAutofillProvider.transformFormFieldToContainViewCoordinates(formData);
        RectF result = formData.mFields.get(0).getBoundsInContainerViewCoordinates();
        assertEquals(10 * EXPECTED_DIP_SCALE + SCROLL_X, result.left, 0);
        assertEquals(20 * EXPECTED_DIP_SCALE + SCROLL_Y, result.top, 0);
        assertEquals(300 * EXPECTED_DIP_SCALE + SCROLL_X, result.right, 0);
        assertEquals(60 * EXPECTED_DIP_SCALE + SCROLL_Y, result.bottom, 0);

        result = formData.mFields.get(1).getBoundsInContainerViewCoordinates();
        assertEquals(20 * EXPECTED_DIP_SCALE + SCROLL_X, result.left, 0);
        assertEquals(100 * EXPECTED_DIP_SCALE + SCROLL_Y, result.top, 0);
        assertEquals(400 * EXPECTED_DIP_SCALE + SCROLL_X, result.right, 0);
        assertEquals(200 * EXPECTED_DIP_SCALE + SCROLL_Y, result.bottom, 0);
    }

    @Test
    public void testTransformToWindowBounds() {
        RectF source = new RectF(10, 20, 300, 400);
        final int offsetY = 10;
        Rect result = mAutofillProvider.transformToWindowBoundsWithOffsetY(source, offsetY);
        assertEquals(10 * EXPECTED_DIP_SCALE + LOCATION_X, result.left, 0);
        assertEquals(20 * EXPECTED_DIP_SCALE + LOCATION_Y + offsetY, result.top, 0);
        assertEquals(300 * EXPECTED_DIP_SCALE + LOCATION_X, result.right, 0);
        assertEquals(400 * EXPECTED_DIP_SCALE + LOCATION_Y + offsetY, result.bottom, 0);
    }
}
