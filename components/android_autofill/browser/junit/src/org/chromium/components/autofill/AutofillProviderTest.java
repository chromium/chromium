// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;
import android.view.autofill.AutofillManager;
import android.view.autofill.AutofillValue;
import android.view.autofill.VirtualViewFillInfo;

import androidx.annotation.RequiresApi;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.JniMocker;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;

import java.util.Arrays;
import java.util.Collections;

/** The unit tests for AutofillProvider. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({AndroidAutofillFeatures.ANDROID_AUTOFILL_BOTTOM_SHEET_WORKAROUND_NAME})
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
    private long mMockedNativeAndroidAutofillProvider = 1;

    // Virtual Id of the field with focus.
    private int mFocusVirtualId;

    // Virtual Id of the field to show the bottom sheet for.
    private int mDialogVirtualId;
    private SparseArray<VirtualViewFillInfo> mPrefillRequestInfos;

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock private AutofillProvider.Natives mNativeMock;
    @Mock private RenderCoordinatesImpl mRenderCoordinates;
    @Mock private AutofillManager mAutofillManager;

    /** AutofillManagerWrapper which keeps track of the virtual id of the field with focus. */
    private class TestAutofillManagerWrapper extends AutofillManagerWrapper {

        public TestAutofillManagerWrapper(Context context) {
            super(context);
        }

        @Override
        public void notifyVirtualViewsReady(
                View parent, SparseArray<VirtualViewFillInfo> viewFillInfos) {
            mPrefillRequestInfos = viewFillInfos;
            super.notifyVirtualViewsReady(parent, viewFillInfos);
        }

        @Override
        public void notifyVirtualViewEntered(View parent, int childId, Rect absBounds) {
            mFocusVirtualId = childId;
            super.notifyVirtualViewEntered(parent, childId, absBounds);
        }

        @Override
        public boolean showAutofillDialog(View parent, int childId) {
            mDialogVirtualId = childId;
            return super.showAutofillDialog(parent, childId);
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mContext = Mockito.mock(Context.class);
        when(mContext.getSystemService(AutofillManager.class)).thenReturn(mAutofillManager);
        when(mAutofillManager.isEnabled()).thenReturn(true);
        mWindowAndroid = Mockito.mock(WindowAndroid.class);
        mDisplayAndroid = Mockito.mock(DisplayAndroid.class);
        mWebContents = Mockito.mock(WebContents.class);
        mContainerView = Mockito.mock(ViewGroup.class);

        AutofillProvider.setAutofillManagerWrapperFactoryForTesting(
                (context) -> {
                    return new TestAutofillManagerWrapper(context);
                });

        mAutofillProvider =
                new AutofillProvider(
                        mContext, mContainerView, mWebContents, "AutofillProviderTest") {
                    @Override
                    protected void initializeNativeAutofillProvider(WebContents webContents) {
                        setNativeAutofillProvider(mMockedNativeAndroidAutofillProvider);
                    }
                };

        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getDisplay()).thenReturn(mDisplayAndroid);
        when(mDisplayAndroid.getDipScale()).thenReturn(EXPECTED_DIP_SCALE);
        when(mContainerView.getScrollX()).thenReturn(SCROLL_X);
        when(mContainerView.getScrollY()).thenReturn(SCROLL_Y);
        doAnswer(
                        new Answer<Void>() {
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
                .getLocationOnScreen(any());

        RenderCoordinatesImpl.setInstanceForTesting(mRenderCoordinates);
        when(mRenderCoordinates.getContentOffsetYPixInt()).thenReturn(0);

        mJniMocker.mock(AutofillProviderJni.TEST_HOOKS, mNativeMock);
    }

    @Test
    public void testTransformFormFieldToContainViewCoordinates() {
        FormFieldDataBuilder field1Builder = new FormFieldDataBuilder();
        field1Builder.mBounds =
                new RectF(/* left= */ 10, /* top= */ 20, /* right= */ 300, /* bottom= */ 60);
        FormFieldDataBuilder field2Builder = new FormFieldDataBuilder();
        field2Builder.mBounds =
                new RectF(/* left= */ 20, /* top= */ 100, /* right= */ 400, /* bottom= */ 200);

        FormData formData =
                new FormData(
                        /* sessionId= */ 123,
                        /* name= */ null,
                        /* host= */ null,
                        Arrays.asList(field1Builder.build(), field2Builder.build()));
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
        FormData formData =
                new FormData(
                        /* sessionId= */ 123,
                        /* name= */ null,
                        /* host= */ null,
                        Arrays.asList(
                                field1Builder.build(),
                                field2Builder.build(),
                                field3Builder.build()));

        mAutofillProvider.startAutofillSession(
                formData,
                /* focus= */ 1,
                /* x= */ 0,
                /* y= */ 0,
                /* width= */ 0,
                /* height= */ 0,
                /* hasServerPrediction= */ false);

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

    @Test
    @Config(minSdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testSendingPrefillRequestUsesCorrectHints() {
        FormFieldDataBuilder field1Builder = new FormFieldDataBuilder();
        field1Builder.mServerPredictions = new String[] {"NAME_FIRST", "NAME_LAST"};
        FormData formData =
                new FormData(123, null, null, Collections.singletonList(field1Builder.build()));

        mAutofillProvider.sendPrefillRequest(formData);
        // Creating a new request here shouldn't affect the results, that's better than saving the
        // prefill request in the provider.
        PrefillRequest randomRequest = new PrefillRequest(formData);
        SparseArray<VirtualViewFillInfo> expectedInfos = randomRequest.getPrefillHints();

        assertEquals(
                expectedInfos.valueAt(0).getAutofillHints(),
                mPrefillRequestInfos.valueAt(0).getAutofillHints());
    }

    @Test
    @Config(minSdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testStartSessionWithPrefillRequestWithShowingBottomSheet() {
        int focus = 1;
        int sessionId = 123;
        int virtualId = FormData.toFieldVirtualId(sessionId, (short) focus);
        FormData formData = setupPrefillRequest(sessionId);
        simulateOnProvideAutofillStructure();
        when(mAutofillManager.showAutofillDialog(any(), eq(virtualId))).thenReturn(true);
        mAutofillProvider.startAutofillSession(
                formData,
                focus,
                /* x= */ 0,
                /* y= */ 0,
                /* width= */ 0,
                /* height= */ 0,
                /* hasServerPrediction= */ false);

        // showAutofillDialog should be called so it has to hold the correct virtualId.
        assertEquals(mDialogVirtualId, virtualId);
        // notifyVirtualViewEntered shouldn't be called so this has to be unset.
        assertEquals(mFocusVirtualId, 0);

        verify(mNativeMock)
                .onShowBottomSheetResult(
                        mMockedNativeAndroidAutofillProvider,
                        /* isShown= */ true,
                        /* provided_structure= */ true);
    }

    @Test
    @Config(minSdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testStartSessionWithPrefillRequestWithoutShowingBottomSheet() {
        int focus = 1;
        int sessionId = 123;
        int virtualId = FormData.toFieldVirtualId(sessionId, (short) focus);
        FormData formData = setupPrefillRequest(sessionId);
        simulateOnProvideAutofillStructure();
        when(mAutofillManager.showAutofillDialog(any(), eq(virtualId))).thenReturn(false);

        mAutofillProvider.startAutofillSession(
                formData,
                focus,
                /* x= */ 0,
                /* y= */ 0,
                /* width= */ 0,
                /* height= */ 0,
                /* hasServerPrediction= */ false);

        // shouldAutofillDialog returns false so we call notifyVirtualViewEntered as well and both
        // of them will have the correct virtualId.
        assertEquals(mDialogVirtualId, virtualId);
        assertEquals(mFocusVirtualId, virtualId);

        verify(mNativeMock)
                .onShowBottomSheetResult(
                        mMockedNativeAndroidAutofillProvider,
                        /* isShown= */ false,
                        /* providedAutofillStructure= */ true);
    }

    @Test
    @Config(minSdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void
            testStartSessionWithPrefillRequestWithoutShowingBottomSheetAndNoAutofillStructure() {
        int focus = 1;
        int sessionId = 123;
        int virtualId = FormData.toFieldVirtualId(sessionId, (short) focus);
        FormData formData = setupPrefillRequest(sessionId);
        when(mAutofillManager.showAutofillDialog(any(), eq(virtualId))).thenReturn(false);

        mAutofillProvider.startAutofillSession(
                formData,
                focus,
                /* x= */ 0,
                /* y= */ 0,
                /* width= */ 0,
                /* height= */ 0,
                /* hasServerPrediction= */ false);

        // shouldAutofillDialog returns false so we call notifyVirtualViewEntered as well and both
        // of them will have the correct virtualId.
        assertEquals(mDialogVirtualId, virtualId);
        assertEquals(mFocusVirtualId, virtualId);

        verify(mNativeMock)
                .onShowBottomSheetResult(
                        mMockedNativeAndroidAutofillProvider,
                        /* isShown= */ false,
                        /* providedAutofillStructure= */ false);
    }

    @Test
    @Config(minSdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testStartSessionWithDifferentSessionIdThanPrefillRequest() {
        int focus = 1;
        int prefillSessionId = 123;
        int newSessionId = 456;
        int virtualId = FormData.toFieldVirtualId(prefillSessionId, (short) focus);
        FormData formData = setupPrefillRequest(prefillSessionId);
        when(mAutofillManager.showAutofillDialog(any(), eq(virtualId))).thenReturn(false);

        FormData newFormData =
                new FormData(newSessionId, /* name= */ null, /* host= */ null, formData.mFields);
        mAutofillProvider.startAutofillSession(
                newFormData,
                focus,
                /* x= */ 0,
                /* y= */ 0,
                /* width= */ 0,
                /* height= */ 0,
                /* hasServerPrediction= */ false);

        // showAutofillDialog shouldn't be called so this has to be 0.
        assertEquals(mDialogVirtualId, 0);
        // notifyVirtualViewEntered should be called so this has to hold the correct virtualId.
        assertEquals(mFocusVirtualId, FormData.toFieldVirtualId(newSessionId, (short) focus));

        verify(mNativeMock, never()).onShowBottomSheetResult(anyLong(), anyBoolean(), anyBoolean());
    }

    FormData setupPrefillRequest(int sessionId) {
        FormFieldDataBuilder field1Builder = new FormFieldDataBuilder();
        field1Builder.mBounds =
                new RectF(/* left= */ 10, /* top= */ 20, /* right= */ 300, /* bottom= */ 60);
        FormFieldDataBuilder field2Builder = new FormFieldDataBuilder();
        field2Builder.mBounds =
                new RectF(/* left= */ 20, /* top= */ 100, /* right= */ 400, /* bottom= */ 200);

        FormData formData =
                new FormData(
                        sessionId,
                        /* name= */ null,
                        /* host= */ null,
                        Arrays.asList(field1Builder.build(), field2Builder.build()));
        mAutofillProvider.sendPrefillRequest(formData);

        return formData;
    }

    /**
     * Simulates a call from the Android Autofill framework to the AutofillProvider to provide the
     * Autofill ViewStructure.
     */
    void simulateOnProvideAutofillStructure() {
        mAutofillProvider.onProvideAutoFillVirtualStructure(
                new TestViewStructure(), /* flags= */ 0);
    }
}
