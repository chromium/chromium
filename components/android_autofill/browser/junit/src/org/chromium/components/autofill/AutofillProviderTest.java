// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Rect;
import android.graphics.RectF;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;
import android.view.autofill.AutofillValue;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;

import java.util.Arrays;

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

    // Virtual Id of the field with focus.
    private int mFocusVirtualId;

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private AutofillProviderJni mAutofillProviderJni;

    @Mock
    private RenderCoordinatesImpl mRenderCoordinates;

    /**
     * Helper class to simplify FormFieldData creation.
     */
    private static class FormFieldDataBuilder {
        String mName;
        String mLabel;
        String mValue;
        String mAutocompleteAttr;
        boolean mShouldAutocomplete;
        String mPlaceholder;
        String mType;
        String mId;
        String[] mOptionValues;
        String[] mOptionContents;
        boolean mIsCheckField;
        boolean mIsChecked;
        int mMaxLength;
        String mHeuristicType;
        String mServerType;
        String mComputedType;
        String[] mServerPredictions;
        Rect mBounds = new Rect();
        String[] mDatalistValues;
        String[] mDatalistLabels;
        boolean mVisible;
        boolean mIsAutofilled;

        public FormFieldData build() {
            return FormFieldData.createFormFieldData(mName, mLabel, mValue, mAutocompleteAttr,
                    mShouldAutocomplete, mPlaceholder, mType, mId, mOptionValues, mOptionContents,
                    mIsCheckField, mIsChecked, mMaxLength, mHeuristicType, mServerType,
                    mComputedType, mServerPredictions, mBounds.left, mBounds.top, mBounds.right,
                    mBounds.bottom, mDatalistValues, mDatalistLabels, mVisible, mIsAutofilled);
        }
    }

    /**
     * AutofillManagerWrapper which keeps track of the virtual id of the field with focus.
     */
    private class TestAutofillManagerWrapper extends AutofillManagerWrapper {
        public TestAutofillManagerWrapper(Context context) {
            super(context);
        }

        @Override
        public void notifyVirtualViewEntered(View parent, int childId, Rect absBounds) {
            mFocusVirtualId = childId;
            super.notifyVirtualViewEntered(parent, childId, absBounds);
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = Mockito.mock(Context.class);
        mWindowAndroid = Mockito.mock(WindowAndroid.class);
        mDisplayAndroid = Mockito.mock(DisplayAndroid.class);
        mWebContents = Mockito.mock(WebContents.class);
        mContainerView = Mockito.mock(ViewGroup.class);

        AutofillProvider.setAutofillManagerWrapperFactoryForTesting(
                (context) -> { return new TestAutofillManagerWrapper(context); });

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

        RenderCoordinatesImpl.setInstanceForTesting(mRenderCoordinates);
        when(mRenderCoordinates.getContentOffsetYPixInt()).thenReturn(0);

        mJniMocker.mock(AutofillProviderJni.TEST_HOOKS, mAutofillProviderJni);
    }

    @After
    public void tearDown() {
        RenderCoordinatesImpl.setInstanceForTesting(null);
    }

    @Test
    public void testTransformFormFieldToContainViewCoordinates() {
        FormFieldDataBuilder field1Builder = new FormFieldDataBuilder();
        field1Builder.mBounds = new Rect(/*left=*/10, /*top=*/20, /*right=*/300, /*bottom=*/60);
        FormFieldDataBuilder field2Builder = new FormFieldDataBuilder();
        field2Builder.mBounds = new Rect(/*left=*/20, /*top=*/100, /*right=*/400, /*bottom=*/200);

        FormData formData = new FormData(
                null, null, Arrays.asList(field1Builder.build(), field2Builder.build()));
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

    /**
     * Test that AutofillProvider#autofill() does not modify the AutofillProvider#isAutofilled()
     * state of unrelated fields.
     */
    @Test
    public void testAutofillDoesNotResetUnrelatedAutofillState() {
        FormFieldDataBuilder field1Builder = new FormFieldDataBuilder();
        field1Builder.mIsAutofilled = true;
        FormFieldDataBuilder field2Builder = new FormFieldDataBuilder();
        field2Builder.mIsAutofilled = false;
        FormFieldDataBuilder field3Builder = new FormFieldDataBuilder();
        field3Builder.mIsAutofilled = false;
        FormData formData = new FormData(null, null,
                Arrays.asList(field1Builder.build(), field2Builder.build(), field3Builder.build()));

        mAutofillProvider.startAutofillSession(formData, /*focus=*/1, /*x=*/0, /*y=*/0, /*width=*/0,
                /*height=*/0, /*hasServerPrediction=*/false);

        assertTrue(formData.mFields.get(0).isAutofilled());
        assertFalse(formData.mFields.get(1).isAutofilled());
        assertFalse(formData.mFields.get(2).isAutofilled());

        SparseArray fillResult = new SparseArray(2);
        fillResult.put(mFocusVirtualId, AutofillValue.forText("text"));
        mAutofillProvider.autofill(fillResult);

        // The field at index 1 is autofilled. The autofill state of the other fields should be
        // unchanged.
        assertTrue(formData.mFields.get(0).isAutofilled());
        assertTrue(formData.mFields.get(1).isAutofilled());
        assertFalse(formData.mFields.get(2).isAutofilled());
    }
}
