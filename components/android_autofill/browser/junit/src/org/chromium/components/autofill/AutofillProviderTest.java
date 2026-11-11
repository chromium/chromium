// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.MutableContextWrapper;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.util.SparseArray;
import android.view.View;
import android.view.autofill.AutofillManager;
import android.view.autofill.AutofillValue;
import android.view.autofill.VirtualViewFillInfo;
import android.widget.FrameLayout;

import androidx.annotation.RequiresApi;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.autofill.mojom.SubmissionSource;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;

import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.Collections;

/** The unit tests for AutofillProvider. */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({
    AndroidAutofillFeatures.ANDROID_AUTOFILL_IMPROVED_VISIBILITY_DETECTION_NAME,
    AndroidAutofillFeatures.ANDROID_AUTOFILL_SUPPRESS_OFFSCREEN_DATALIST_NAME,
    AndroidAutofillFeatures.ANDROID_AUTOFILL_REFETCH_MANAGER_IF_DISABLED_NAME,
})
public class AutofillProviderTest {
    private static final float EXPECTED_DIP_SCALE = 2;
    private static final String AUTOFILL_SERVICE_NAME = "autofill";
    private static final int SCROLL_X = 15;
    private static final int SCROLL_Y = 155;
    private static final int LOCATION_X = 25;
    private static final int LOCATION_Y = 255;

    private Context mContext;
    private MutableContextWrapper mContextWrapper;
    private WindowAndroid mWindowAndroid;
    private WebContents mWebContents;
    private TestContainerView mContainerView;
    private View mAnchorView;
    private AutofillProvider mAutofillProvider;
    private DisplayAndroid mDisplayAndroid;
    private final long mMockedNativeAndroidAutofillProvider = 1;

    // Virtual Id of the field with focus.
    private int mFocusVirtualId;
    private Rect mFocusBounds;

    // Virtual Id of the field to show the bottom sheet for.
    private int mDialogVirtualId;
    private SparseArray<VirtualViewFillInfo> mPrefillRequestInfos;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private AutofillProvider.Natives mNativeMock;
    @Mock private RenderCoordinatesImpl mRenderCoordinates;
    @Mock private AutofillManager mAutofillManager;
    @Mock private ViewAndroidDelegate mViewAndroidDelegate;

    private static class TestContainerView extends FrameLayout {
        private Rect mVisibleRect = new Rect();
        private boolean mIsVisible = true;
        private int mScreenX;
        private int mScreenY;

        TestContainerView(Context context) {
            super(context);
        }

        void setScreenGeometry(Rect visibleRect, int screenX, int screenY) {
            mVisibleRect = visibleRect;
            mScreenX = screenX;
            mScreenY = screenY;
        }

        void setIsVisible(boolean isVisible) {
            mIsVisible = isVisible;
        }

        @Override
        public boolean getGlobalVisibleRect(Rect r, Point globalOffset) {
            r.set(mVisibleRect);
            return mIsVisible;
        }

        @Override
        public void getLocationOnScreen(int[] outLocation) {
            outLocation[0] = mScreenX;
            outLocation[1] = mScreenY;
        }

        @Override
        public void getLocationInWindow(int[] outLocation) {
            outLocation[0] = mScreenX;
            outLocation[1] = mScreenY;
        }
    }

    /** AutofillManagerWrapper which keeps track of the virtual id of the field with focus. */
    private class TestAutofillManagerWrapper extends AutofillManagerWrapper {
        private boolean mDestroyed;

        public TestAutofillManagerWrapper(Context context) {
            super(context);
        }

        @Override
        public void destroy() {
            super.destroy();
            mDestroyed = true;
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
            mFocusBounds = absBounds;
            super.notifyVirtualViewEntered(parent, childId, absBounds);
        }

        @Override
        public boolean showAutofillDialog(View parent, int childId) {
            mDialogVirtualId = childId;
            return super.showAutofillDialog(parent, childId);
        }
    }

    private void setContainerScreenGeometry(Rect visibleRect, int screenX, int screenY) {
        mContainerView.setScreenGeometry(visibleRect, screenX, screenY);
    }

    @Before
    public void setUp() {
        mContext = Mockito.mock(Context.class);
        when(mContext.getSystemServiceName(AutofillManager.class))
                .thenReturn(AUTOFILL_SERVICE_NAME);
        when(mContext.getSystemService(AUTOFILL_SERVICE_NAME)).thenReturn(mAutofillManager);
        when(mAutofillManager.isEnabled()).thenReturn(true);
        mContextWrapper = new MutableContextWrapper(mContext);
        mWindowAndroid = Mockito.mock(WindowAndroid.class);
        mDisplayAndroid = Mockito.mock(DisplayAndroid.class);
        mWebContents = Mockito.mock(WebContents.class);
        mContainerView = new TestContainerView(ContextUtils.getApplicationContext());
        mAnchorView = new View(ContextUtils.getApplicationContext());

        AutofillProvider.setAutofillManagerWrapperFactoryForTesting(
                (context) -> new TestAutofillManagerWrapper(context));

        mAutofillProvider =
                new AutofillProvider(
                        new WeakReference<>(mContextWrapper),
                        mContainerView,
                        mWebContents,
                        "AutofillProviderTest") {
                    @Override
                    protected void initializeNativeAutofillProvider(WebContents webContents) {
                        setNativeAutofillProvider(mMockedNativeAndroidAutofillProvider);
                    }
                };

        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getDisplay()).thenReturn(mDisplayAndroid);
        when(mDisplayAndroid.getDipScale()).thenReturn(EXPECTED_DIP_SCALE);
        mContainerView.setScrollX(SCROLL_X);
        mContainerView.setScrollY(SCROLL_Y);
        when(mWebContents.getViewAndroidDelegate()).thenReturn(mViewAndroidDelegate);
        when(mViewAndroidDelegate.acquireView()).thenReturn(mAnchorView);

        setContainerScreenGeometry(new Rect(0, 0, 2000, 2000), LOCATION_X, LOCATION_Y);

        RenderCoordinatesImpl.setInstanceForTesting(mRenderCoordinates);
        when(mRenderCoordinates.getContentOffsetYPixInt()).thenReturn(0);
        when(mRenderCoordinates.getContentOffsetBottomPixInt()).thenReturn(0);

        AutofillProviderJni.setInstanceForTesting(mNativeMock);
    }

    @Test
    public void testContextChangeReinitializesAutofillManager() {
        TestAutofillManagerWrapper oldManager =
                (TestAutofillManagerWrapper) mAutofillProvider.getAutofillManagerWrapper();
        assertFalse(oldManager.mDestroyed);

        // Change context
        Context newContext = Robolectric.buildActivity(Activity.class).get();
        mAutofillProvider.switchToContext(new WeakReference<>(newContext));

        // The old manager is destroyed and replaced with a new one.
        assertTrue(oldManager.mDestroyed);
        assertNotSame(oldManager, mAutofillProvider.getAutofillManagerWrapper());
    }

    @Test
    public void testHandlesNullContextGracefully() {
        mAutofillProvider.switchToContext(new ImmutableWeakReference<>(null));

        assertNotNull(mAutofillProvider.getAutofillManagerWrapper());
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
        when(mRenderCoordinates.getContentOffsetYPixInt()).thenReturn(offsetY);
        Rect result = mAutofillProvider.transformToWindowBounds(source);
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
                /* width= */ 50,
                /* height= */ 20,
                /* hasServerPrediction= */ false);

        assertTrue(formData.mFields.get(0).isAutofilled());
        assertFalse(formData.mFields.get(1).isAutofilled());
        assertFalse(formData.mFields.get(2).isAutofilled());

        SparseArray<AutofillValue> fillResult = new SparseArray<>(2);
        fillResult.put(mFocusVirtualId, AutofillValue.forText("text"));
        mAutofillProvider.autofill(fillResult);

        // The field at index 1 is autofilled. The autofill state of the other fields should be
        // unchanged.
        assertTrue(formData.mFields.get(0).isAutofilled());
        assertTrue(formData.mFields.get(1).isAutofilled());
        assertFalse(formData.mFields.get(2).isAutofilled());
    }

    @Test
    public void testAutofillWithHighSessionId() {
        FormFieldDataBuilder fieldBuilder = new FormFieldDataBuilder();
        FormData formData =
                new FormData(
                        /* sessionId= */ 0xffff,
                        /* name= */ null,
                        /* host= */ null,
                        Collections.singletonList(fieldBuilder.build()));

        mAutofillProvider.startAutofillSession(
                formData,
                /* focus= */ 0,
                /* x= */ 0,
                /* y= */ 0,
                /* width= */ 50,
                /* height= */ 20,
                /* hasServerPrediction= */ false);

        SparseArray<AutofillValue> fillResult = new SparseArray<>(1);
        fillResult.put(mFocusVirtualId, AutofillValue.forText("filled_value"));
        mAutofillProvider.autofill(fillResult);

        assertTrue(formData.mFields.get(0).isAutofilled());
        assertEquals("filled_value", formData.mFields.get(0).getValue());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
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
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
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
                /* width= */ 50,
                /* height= */ 20,
                /* hasServerPrediction= */ false);

        // showAutofillDialog should be called so it has to hold the correct virtualId.
        assertEquals(mDialogVirtualId, virtualId);
        // notifyVirtualViewEntered shouldn't be called so this has to be unset.
        assertEquals(0, mFocusVirtualId);

        verify(mNativeMock)
                .onShowBottomSheetResult(
                        mMockedNativeAndroidAutofillProvider,
                        /* isShown= */ true,
                        /* providedAutofillStructure= */ true);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
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
                /* width= */ 50,
                /* height= */ 20,
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
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
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
                /* width= */ 50,
                /* height= */ 20,
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
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
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
                /* width= */ 50,
                /* height= */ 20,
                /* hasServerPrediction= */ false);

        // showAutofillDialog shouldn't be called so this has to be 0.
        assertEquals(0, mDialogVirtualId);
        // notifyVirtualViewEntered should be called so this has to hold the correct virtualId.
        assertEquals(mFocusVirtualId, FormData.toFieldVirtualId(newSessionId, (short) focus));

        verify(mNativeMock, never()).onShowBottomSheetResult(anyLong(), anyBoolean(), anyBoolean());

        // Starting another session later that happens to reuse prefillSessionId must not trigger
        // the stale prefill request.
        mAutofillProvider.startAutofillSession(
                formData,
                focus,
                /* x= */ 0,
                /* y= */ 0,
                /* width= */ 50,
                /* height= */ 20,
                /* hasServerPrediction= */ false);
        assertEquals(0, mDialogVirtualId);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testSetWebContentsCancelsSessionAndClearsPrefillRequest() {
        int focus = 0;
        int sessionId = 1;
        FormData formData = setupPrefillRequest(sessionId);

        WebContents newWebContents = Mockito.mock(WebContents.class);
        when(newWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        mAutofillProvider.setWebContents(newWebContents);

        verify(mAutofillManager).cancel();

        // Starting a session on the new WebContents with the same sessionId must not match the
        // prefill request from the previous WebContents.
        mAutofillProvider.startAutofillSession(
                formData,
                focus,
                /* x= */ 0,
                /* y= */ 0,
                /* width= */ 50,
                /* height= */ 20,
                /* hasServerPrediction= */ false);
        assertEquals(0, mDialogVirtualId);
        assertEquals(FormData.toFieldVirtualId(sessionId, (short) focus), mFocusVirtualId);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testSetNativeAutofillProviderZeroCancelsSessionAndClearsPrefillRequest() {
        int focus = 0;
        int sessionId = 1;
        FormData formData = setupPrefillRequest(sessionId);

        mAutofillProvider.setNativeAutofillProvider(0);
        verify(mAutofillManager).cancel();

        mAutofillProvider.setNativeAutofillProvider(mMockedNativeAndroidAutofillProvider);
        mAutofillProvider.startAutofillSession(
                formData,
                focus,
                /* x= */ 0,
                /* y= */ 0,
                /* width= */ 50,
                /* height= */ 20,
                /* hasServerPrediction= */ false);
        assertEquals(0, mDialogVirtualId);
        assertEquals(FormData.toFieldVirtualId(sessionId, (short) focus), mFocusVirtualId);
    }

    @Test
    public void testCallsNativeToTriggerPasskeys() {
        mAutofillProvider.triggerPasskeyRequest();
        verify(mNativeMock).onTriggerPasskeyRequest(eq(mMockedNativeAndroidAutofillProvider));
    }

    @Test
    public void testCallsNativeToProvidePasskeyAvailability() {
        mAutofillProvider.shouldOfferPasskeyEntry();
        verify(mNativeMock).hasPasskeyRequest(eq(mMockedNativeAndroidAutofillProvider));
    }

    @Test
    public void testSuppressNotificationOutsideBounds() {
        setContainerScreenGeometry(new Rect(0, 0, 100, 100), 0, 0);

        // Reset mFocusVirtualId and mFocusBounds
        mFocusVirtualId = 0;
        mFocusBounds = null;

        FormFieldDataBuilder field1Builder = new FormFieldDataBuilder();
        FormFieldDataBuilder field2Builder = new FormFieldDataBuilder();
        FormData formData =
                new FormData(
                        123,
                        /* name= */ null,
                        /* host= */ null,
                        Arrays.asList(field1Builder.build(), field2Builder.build()));

        // Case 1: Field is entirely outside (above)
        // transformed: [0, -60, 100, -20] (if x=0, y=-30, w=50, h=20, dipScale=2, offsetY=0)
        mAutofillProvider.startAutofillSession(
                formData,
                /* focus= */ 0,
                /* x= */ 0,
                /* y= */ -30,
                /* width= */ 50,
                /* height= */ 20,
                /* hasServerPrediction= */ false);

        // Should be suppressed
        assertEquals(0, mFocusVirtualId);
        assertNull(mFocusBounds);

        // Case 2: Field is partially visible (top portion is cut off)
        // transformed: [0, -10, 100, 30] (if x=0, y=-5, w=50, h=20)
        // Intersect with [0, 0, 100, 100] -> [0, 0, 100, 30]
        mAutofillProvider.startAutofillSession(
                formData,
                /* focus= */ 1,
                /* x= */ 0,
                /* y= */ -5,
                /* width= */ 50,
                /* height= */ 20,
                /* hasServerPrediction= */ false);

        // Should NOT be suppressed
        assertEquals(FormData.toFieldVirtualId(123, (short) 1), mFocusVirtualId);
        // Bounds should be clamped
        assertNotNull(mFocusBounds);
        assertEquals(new Rect(0, 0, 100, 30), mFocusBounds);
    }

    @Test
    public void testSuppressNotificationOutsideBoundsWithOffsetY() {
        setContainerScreenGeometry(new Rect(0, 0, 100, 100), 0, 0);

        // Set offset Y to 40. Effective visible bounds top should be 40.
        when(mRenderCoordinates.getContentOffsetYPixInt()).thenReturn(40);

        // Reset mFocusVirtualId and mFocusBounds.
        mFocusVirtualId = 0;
        mFocusBounds = null;

        FormFieldDataBuilder field1Builder = new FormFieldDataBuilder();
        FormFieldDataBuilder field2Builder = new FormFieldDataBuilder();
        FormData formData =
                new FormData(
                        123,
                        /* name= */ null,
                        /* host= */ null,
                        Arrays.asList(field1Builder.build(), field2Builder.build()));

        // Case 1: Field is in the "offset" area (under toolbar) and should be suppressed.
        //
        // Input bounds: [x=0, y=-30, w=50, h=20] -> [left=0, top=-30, right=50, bottom=-10].
        // 1. Scale by dipScale=2: [left=0, top=-60, right=100, bottom=-20].
        // 2. Translate by container location [0,0] and add offsetY=40:
        //    [left=0, top=-20, right=100, bottom=20].
        //
        // Effective visible bounds top is 40.
        // Suppressed because the field's bottom (20) < effective top (40).
        mAutofillProvider.startAutofillSession(
                formData,
                /* focus= */ 0,
                /* x= */ 0,
                /* y= */ -30,
                /* width= */ 50,
                /* height= */ 20,
                /* hasServerPrediction= */ false);

        assertEquals(0, mFocusVirtualId);
        assertNull(mFocusBounds);

        // Case 2: Field is partially visible (crosses the effective top boundary).
        //
        // Input bounds: [x=0, y=-5, w=50, h=15] -> [left=0, top=-5, right=50, bottom=10].
        // 1. Scale by dipScale=2: [left=0, top=-10, right=100, bottom=20].
        // 2. Translate by container location [0,0] and add offsetY=40:
        //    [left=0, top=30, right=100, bottom=60].
        //
        // Intersect [0, 30, 100, 60] with effective visible bounds
        // [0, 40, 100, 100] -> [0, 40, 100, 60].
        mAutofillProvider.startAutofillSession(
                formData,
                /* focus= */ 1,
                /* x= */ 0,
                /* y= */ -5,
                /* width= */ 50,
                /* height= */ 15,
                /* hasServerPrediction= */ false);

        // Should NOT be suppressed.
        assertEquals(FormData.toFieldVirtualId(123, (short) 1), mFocusVirtualId);
        // Bounds should be clamped.
        assertNotNull(mFocusBounds);
        assertEquals(new Rect(0, 40, 100, 60), mFocusBounds);
    }

    @Test
    public void testSuppressEmptyBounds() {
        setContainerScreenGeometry(new Rect(0, 0, 100, 100), 0, 0);
        mFocusVirtualId = 0;
        mFocusBounds = null;

        FormFieldDataBuilder fieldBuilder = new FormFieldDataBuilder();
        FormData formData =
                new FormData(
                        123,
                        /* name= */ null,
                        /* host= */ null,
                        Collections.singletonList(fieldBuilder.build()));

        // Off-screen fields collapsed to 0x0 points by TransformToRootCoordinates() must be
        // suppressed even when the collapsed point lands on the top/left boundary or inside the
        // container's visible bounds.
        float[][] emptyPointInputs = {
            {10f, 0f}, // Top edge: [20, 0, 20, 0]
            {0f, 10f}, // Left edge: [0, 20, 0, 20]
            {10f, 10f}, // Interior: [20, 20, 20, 20]
        };

        for (float[] point : emptyPointInputs) {
            mAutofillProvider.startAutofillSession(
                    formData,
                    /* focus= */ 0,
                    /* x= */ point[0],
                    /* y= */ point[1],
                    /* width= */ 0,
                    /* height= */ 0,
                    /* hasServerPrediction= */ false);
            assertEquals(0, mFocusVirtualId);
            assertNull(mFocusBounds);

            mAutofillProvider.queryAutofillSuggestion();
            verify(mAutofillManager, never()).requestAutofill(any(), anyInt(), any());
        }
    }

    private FormData setUpBottomToolbarOffsetTest() {
        setContainerScreenGeometry(new Rect(0, 0, 100, 100), 0, 0);
        // Effective visible bounds: [0, 0, 100, 70].
        when(mRenderCoordinates.getContentOffsetBottomPixInt()).thenReturn(30);
        mFocusVirtualId = 0;
        mFocusBounds = null;

        FormFieldDataBuilder fieldBuilder = new FormFieldDataBuilder();
        return new FormData(
                123,
                /* name= */ null,
                /* host= */ null,
                Collections.singletonList(fieldBuilder.build()));
    }

    @Test
    public void testBottomToolbarOffset_suppressesFieldInsideBottomToolbar() {
        FormData formData = setUpBottomToolbarOffsetTest();

        // Field lies entirely inside the bottom toolbar region ([0, 70, 100, 100]).
        mAutofillProvider.startAutofillSession(
                formData,
                /* focus= */ 0,
                /* x= */ 0,
                /* y= */ 35,
                /* width= */ 50,
                /* height= */ 15,
                /* hasServerPrediction= */ false);

        assertEquals(0, mFocusVirtualId);
        assertNull(mFocusBounds);
        mAutofillProvider.queryAutofillSuggestion();
        verify(mAutofillManager, never()).requestAutofill(any(), anyInt(), any());
    }

    @Test
    public void testBottomToolbarOffset_clampsPartiallyOverlappingField() {
        FormData formData = setUpBottomToolbarOffsetTest();
        int fieldVirtualId = FormData.toFieldVirtualId(123, (short) 0);

        // Field partially overlaps the bottom toolbar ([0, 50, 100, 90]) -> clamped to
        // [0, 50, 100, 70].
        mAutofillProvider.startAutofillSession(
                formData,
                /* focus= */ 0,
                /* x= */ 0,
                /* y= */ 25,
                /* width= */ 50,
                /* height= */ 20,
                /* hasServerPrediction= */ false);

        assertEquals(fieldVirtualId, mFocusVirtualId);
        assertEquals(new Rect(0, 50, 100, 70), mFocusBounds);
        mAutofillProvider.queryAutofillSuggestion();
        verify(mAutofillManager)
                .requestAutofill(
                        eq(mContainerView), eq(fieldVirtualId), eq(new Rect(0, 50, 100, 70)));
    }

    @Test
    public void testBottomToolbarOffset_preservesFieldAboveBottomToolbar() {
        FormData formData = setUpBottomToolbarOffsetTest();
        int fieldVirtualId = FormData.toFieldVirtualId(123, (short) 0);

        // Field lies completely above the bottom toolbar ([0, 20, 100, 60]) -> unchanged.
        mAutofillProvider.startAutofillSession(
                formData,
                /* focus= */ 0,
                /* x= */ 0,
                /* y= */ 10,
                /* width= */ 50,
                /* height= */ 20,
                /* hasServerPrediction= */ false);

        assertEquals(fieldVirtualId, mFocusVirtualId);
        assertEquals(new Rect(0, 20, 100, 60), mFocusBounds);
        mAutofillProvider.queryAutofillSuggestion();
        verify(mAutofillManager)
                .requestAutofill(
                        eq(mContainerView), eq(fieldVirtualId), eq(new Rect(0, 20, 100, 60)));
    }

    @Test
    public void testSuppressNotificationWhenToolbarsObscureEntireContainer() {
        setContainerScreenGeometry(new Rect(0, 0, 100, 100), 0, 0);
        // Combined top (60) and bottom (50) browser controls exceed container height (100),
        // producing an inverted [0, 60, 100, 50] rect before normalization to [0, 0, 0, 0].
        when(mRenderCoordinates.getContentOffsetYPixInt()).thenReturn(60);
        when(mRenderCoordinates.getContentOffsetBottomPixInt()).thenReturn(50);

        mFocusVirtualId = 0;
        mFocusBounds = null;

        FormFieldDataBuilder fieldBuilder = new FormFieldDataBuilder();
        FormData formData =
                new FormData(
                        123,
                        /* name= */ null,
                        /* host= */ null,
                        Collections.singletonList(fieldBuilder.build()));

        // Transforms to [-10, -10, 90, 90], which spans both [0, 0, 0, 0] and [0, 60, 100, 50] so
        // Android's Rect.intersect() would falsely return true without the empty-bounds guards.
        mAutofillProvider.startAutofillSession(
                formData,
                /* focus= */ 0,
                /* x= */ -5,
                /* y= */ -35,
                /* width= */ 50,
                /* height= */ 50,
                /* hasServerPrediction= */ false);

        assertEquals(0, mFocusVirtualId);
        assertNull(mFocusBounds);
        mAutofillProvider.queryAutofillSuggestion();
        verify(mAutofillManager, never()).requestAutofill(any(), anyInt(), any());
    }

    @Test
    public void testSuppressNotificationWhenContainerNotVisible() {
        // Simulate View.getGlobalVisibleRect() mutating `rect` to non-empty dimensions before an
        // ancestor clips/hides the container and returns false.
        setContainerScreenGeometry(new Rect(0, 0, 100, 100), 0, 0);
        mContainerView.setIsVisible(false);

        mFocusVirtualId = 0;
        mFocusBounds = null;

        FormFieldDataBuilder fieldBuilder = new FormFieldDataBuilder();
        FormData formData =
                new FormData(
                        123,
                        /* name= */ null,
                        /* host= */ null,
                        Collections.singletonList(fieldBuilder.build()));

        mAutofillProvider.startAutofillSession(
                formData,
                /* focus= */ 0,
                /* x= */ 0,
                /* y= */ 10,
                /* width= */ 50,
                /* height= */ 20,
                /* hasServerPrediction= */ false);

        assertEquals(0, mFocusVirtualId);
        assertNull(mFocusBounds);
        mAutofillProvider.queryAutofillSuggestion();
        verify(mAutofillManager, never()).requestAutofill(any(), anyInt(), any());
    }

    @Test
    public void testShowDatalistPopup_suppressedWhenOutsideVisibleBounds() {
        setContainerScreenGeometry(new Rect(0, 0, 100, 100), 0, 0);

        Activity activity = Robolectric.buildActivity(Activity.class).get();
        mAutofillProvider.switchToContext(new WeakReference<>(activity));

        // Field is entirely outside the visible container bounds (e.g. top: -2000px).
        // Datalist anchor uses field bounds: [0, -2000, 100, -1900] * dipScale 2 ->
        // [0, -4000, 200, -3800], outside [0, 0, 100, 100].
        RectF offscreenBounds = new RectF(0, -2000, 100, -1900);
        FormFieldDataBuilder fieldBuilder = new FormFieldDataBuilder();
        fieldBuilder.mBounds = offscreenBounds;

        FormData form =
                new FormData(
                        /* sessionId= */ 123,
                        /* name= */ null,
                        /* host= */ null,
                        Collections.singletonList(fieldBuilder.build()));

        mAutofillProvider.startAutofillSession(
                form,
                /* focus= */ 0,
                offscreenBounds.left,
                offscreenBounds.top,
                offscreenBounds.width(),
                offscreenBounds.height(),
                /* hasServerPrediction= */ false);

        mAutofillProvider.showDatalistPopup(
                new String[] {"Value 1"}, new String[] {"Label 1"}, /* isRtl= */ false);

        // When bounds are completely outside visible bounds, popup anchor must not be set
        // and datalist popup must not be created.
        verify(mViewAndroidDelegate, never()).acquireView();
        verify(mNativeMock, never())
                .setAnchorViewRect(
                        anyLong(), any(), anyFloat(), anyFloat(), anyFloat(), anyFloat());
        assertNull(mAutofillProvider.getDatalistPopupForTesting());
    }

    @Test
    @Features.DisableFeatures({
        AndroidAutofillFeatures.ANDROID_AUTOFILL_SUPPRESS_OFFSCREEN_DATALIST_NAME
    })
    public void testShowDatalistPopup_notSuppressedWhenFeatureDisabled() {
        setContainerScreenGeometry(new Rect(0, 0, 100, 100), 0, 0);

        Activity activity = Robolectric.buildActivity(Activity.class).get();
        mAutofillProvider.switchToContext(new WeakReference<>(activity));

        // Field is entirely outside the visible container bounds (e.g. top: -2000px).
        RectF offscreenBounds = new RectF(0, -2000, 100, -1900);
        FormFieldDataBuilder fieldBuilder = new FormFieldDataBuilder();
        fieldBuilder.mBounds = offscreenBounds;

        FormData form =
                new FormData(
                        /* sessionId= */ 123,
                        /* name= */ null,
                        /* host= */ null,
                        Collections.singletonList(fieldBuilder.build()));

        mAutofillProvider.startAutofillSession(
                form,
                /* focus= */ 0,
                offscreenBounds.left,
                offscreenBounds.top,
                offscreenBounds.width(),
                offscreenBounds.height(),
                /* hasServerPrediction= */ false);

        mAutofillProvider.showDatalistPopup(
                new String[] {"Value 1"}, new String[] {"Label 1"}, /* isRtl= */ false);

        // When the feature is disabled, popup anchor must be set despite offscreen bounds.
        verify(mViewAndroidDelegate).acquireView();
        verify(mNativeMock)
                .setAnchorViewRect(
                        eq(mMockedNativeAndroidAutofillProvider),
                        eq(mAnchorView),
                        eq(offscreenBounds.left),
                        eq(offscreenBounds.top),
                        eq(offscreenBounds.width()),
                        eq(offscreenBounds.height()));
    }

    @Test
    public void testShowDatalistPopup_suppressedWhenBoundsAreEmpty() {
        setContainerScreenGeometry(new Rect(0, 0, 100, 100), 0, 0);

        Activity activity = Robolectric.buildActivity(Activity.class).get();
        mAutofillProvider.switchToContext(new WeakReference<>(activity));

        // Off-screen elements may be reported by Blink with empty bounds (0, 0, 0, 0).
        RectF emptyBounds = new RectF(0, 0, 0, 0);
        FormFieldDataBuilder fieldBuilder = new FormFieldDataBuilder();
        fieldBuilder.mBounds = emptyBounds;

        FormData form =
                new FormData(
                        /* sessionId= */ 123,
                        /* name= */ null,
                        /* host= */ null,
                        Collections.singletonList(fieldBuilder.build()));

        mAutofillProvider.startAutofillSession(
                form,
                /* focus= */ 0,
                emptyBounds.left,
                emptyBounds.top,
                emptyBounds.width(),
                emptyBounds.height(),
                /* hasServerPrediction= */ false);

        mAutofillProvider.showDatalistPopup(
                new String[] {"Value 1"}, new String[] {"Label 1"}, /* isRtl= */ false);

        verify(mViewAndroidDelegate, never()).acquireView();
        verify(mNativeMock, never())
                .setAnchorViewRect(
                        anyLong(), any(), anyFloat(), anyFloat(), anyFloat(), anyFloat());
        assertNull(mAutofillProvider.getDatalistPopupForTesting());
    }

    @Test
    public void testShowDatalistPopup_anchorsWhenInsideVisibleBounds() {
        setContainerScreenGeometry(new Rect(0, 0, 100, 100), 0, 0);

        Activity activity = Robolectric.buildActivity(Activity.class).get();
        mAutofillProvider.switchToContext(new WeakReference<>(activity));

        // Field is inside visible bounds: [0, 10, 50, 30] * dipScale 2 ->
        // [0, 20, 100, 60], within [0, 0, 100, 100].
        RectF visibleBounds = new RectF(0, 10, 50, 30);
        FormFieldDataBuilder fieldBuilder = new FormFieldDataBuilder();
        fieldBuilder.mBounds = visibleBounds;

        FormData form =
                new FormData(
                        /* sessionId= */ 456,
                        /* name= */ null,
                        /* host= */ null,
                        Collections.singletonList(fieldBuilder.build()));

        mAutofillProvider.startAutofillSession(
                form,
                /* focus= */ 0,
                visibleBounds.left,
                visibleBounds.top,
                visibleBounds.width(),
                visibleBounds.height(),
                /* hasServerPrediction= */ false);

        mAutofillProvider.showDatalistPopup(
                new String[] {"Value 1"}, new String[] {"Label 1"}, /* isRtl= */ false);

        // When bounds are visible, anchor must be acquired and positioned.
        verify(mViewAndroidDelegate).acquireView();
        verify(mNativeMock)
                .setAnchorViewRect(
                        eq(mMockedNativeAndroidAutofillProvider),
                        eq(mAnchorView),
                        eq(visibleBounds.left),
                        eq(visibleBounds.top),
                        eq(visibleBounds.width()),
                        eq(visibleBounds.height()));
    }

    @Test
    public void testShowDatalistPopup_dismissesStalePopupWhenMovedOutsideBounds() {
        setContainerScreenGeometry(new Rect(0, 0, 100, 100), 0, 0);

        Activity activity = Robolectric.buildActivity(Activity.class).get();
        mAutofillProvider.switchToContext(new WeakReference<>(activity));

        RectF offscreenBounds = new RectF(0, -2000, 100, -1900);
        FormFieldDataBuilder fieldBuilder = new FormFieldDataBuilder();
        fieldBuilder.mBounds = offscreenBounds;

        FormData form =
                new FormData(
                        /* sessionId= */ 123,
                        /* name= */ null,
                        /* host= */ null,
                        Collections.singletonList(fieldBuilder.build()));

        mAutofillProvider.startAutofillSession(
                form,
                /* focus= */ 0,
                offscreenBounds.left,
                offscreenBounds.top,
                offscreenBounds.width(),
                offscreenBounds.height(),
                /* hasServerPrediction= */ false);

        // A popup is currently showing, but the field moves outside visible bounds.
        // The stale popup must be dismissed.
        AutofillPopup mockPopup = Mockito.mock(AutofillPopup.class);
        mAutofillProvider.setDatalistPopupForTesting(mockPopup);

        mAutofillProvider.showDatalistPopup(
                new String[] {"Value 1"}, new String[] {"Label 1"}, /* isRtl= */ false);

        verify(mockPopup).dismiss();
        assertNull(mAutofillProvider.getDatalistPopupForTesting());
    }

    @Test
    public void testStartAutofillSession_dismissesDatalistPopup() {
        AutofillPopup mockPopup = Mockito.mock(AutofillPopup.class);
        mAutofillProvider.setDatalistPopupForTesting(mockPopup);

        FormFieldDataBuilder fieldBuilder = new FormFieldDataBuilder();
        FormData form =
                new FormData(
                        /* sessionId= */ 123,
                        /* name= */ null,
                        /* host= */ null,
                        Collections.singletonList(fieldBuilder.build()));

        mAutofillProvider.startAutofillSession(
                form,
                /* focus= */ 0,
                /* x= */ 0,
                /* y= */ 0,
                /* width= */ 50,
                /* height= */ 20,
                /* hasServerPrediction= */ false);

        verify(mockPopup).dismiss();
        assertNull(mAutofillProvider.getDatalistPopupForTesting());
    }

    @Test
    public void testCancelSession_dismissesDatalistPopup() {
        AutofillPopup mockPopup = Mockito.mock(AutofillPopup.class);
        mAutofillProvider.setDatalistPopupForTesting(mockPopup);

        mAutofillProvider.cancelSession();

        verify(mockPopup).dismiss();
        assertNull(mAutofillProvider.getDatalistPopupForTesting());
    }

    @Test
    public void testCallsNativeToProvidePasskeyAvailability_nativeNull() {
        mAutofillProvider.setNativeAutofillProvider(0);
        assertFalse(mAutofillProvider.shouldOfferPasskeyEntry());
        verify(mNativeMock, never()).hasPasskeyRequest(anyLong());
    }

    @Test
    public void testCallsNativeOnDidShowSuggestions() {
        FormFieldDataBuilder fieldBuilder = new FormFieldDataBuilder();
        FormData formData =
                new FormData(
                        /* sessionId= */ 123,
                        /* name= */ null,
                        /* host= */ null,
                        Collections.singletonList(fieldBuilder.build()));
        mAutofillProvider.startAutofillSession(
                formData,
                /* focus= */ 0,
                /* x= */ 0,
                /* y= */ 0,
                /* width= */ 50,
                /* height= */ 20,
                /* hasServerPrediction= */ false);

        mAutofillProvider.getAutofillManagerWrapper().notifyInputUiChange();
        verify(mNativeMock).onDidShowSuggestions(eq(mMockedNativeAndroidAutofillProvider));
    }

    @Test
    public void testIsAutofillDisabled() {
        when(mAutofillManager.isEnabled()).thenReturn(true);
        assertFalse(mAutofillProvider.isAutofillDisabled());

        when(mAutofillManager.isEnabled()).thenReturn(false);
        assertTrue(mAutofillProvider.isAutofillDisabled());
    }

    @Test
    public void testStartAutofillSessionRefetchesDisabledAutofillManager() {
        AutofillManager newAutofillManager =
                moveToActivityWithAutofillManager(/* isEnabled= */ true);
        when(newAutofillManager.getAutofillServiceComponentName())
                .thenReturn(new ComponentName("com.google.android.gms", "AutofillService"));
        HistogramWatcher providerWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillProviderUMA.UMA_AUTOFILL_PROVIDER,
                        AutofillProviderUMA.Provider.AWG);

        startSessionOnSingleField();

        verify(newAutofillManager).notifyViewEntered(any(View.class), anyInt(), any(Rect.class));
        verify(mAutofillManager, never())
                .notifyViewEntered(any(View.class), anyInt(), any(Rect.class));
        // An AutofillManager has only one AutofillCallback, which may be the app's.
        verify(newAutofillManager, never()).registerCallback(any());
        verify(mAutofillManager).unregisterCallback(any());
        assertFalse(mAutofillProvider.isAutofillDisabled());
        assertEquals(
                "com.google.android.gms",
                mAutofillProvider.getAutofillManagerWrapper().getPackageName());
        assertTrue(mAutofillProvider.getAutofillManagerWrapper().isAwGCurrentAutofillService());
        providerWatcher.assertExpected();

        // Verify AutofillProviderUMA also picked up the updated provider package.
        HistogramWatcher sessionWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION,
                        AutofillProviderUMA.NO_SUGGESTION_FORM_AUTOFILLED_AWG);
        simulateOnProvideAutofillStructure();
        SparseArray<AutofillValue> values = new SparseArray<>();
        values.put(FormData.toFieldVirtualId(123, (short) 0), AutofillValue.forText("alice"));
        mAutofillProvider.autofill(values);
        mAutofillProvider.onFormSubmitted(SubmissionSource.FORM_SUBMISSION);
        sessionWatcher.assertExpected();
    }

    @Test
    @Features.DisableFeatures(
            AndroidAutofillFeatures.ANDROID_AUTOFILL_REFETCH_MANAGER_IF_DISABLED_NAME)
    public void testStartAutofillSessionKeepsDisabledAutofillManagerWithoutFeature() {
        AutofillManager newAutofillManager =
                moveToActivityWithAutofillManager(/* isEnabled= */ true);

        startSessionOnSingleField();

        verify(newAutofillManager, never())
                .notifyViewEntered(any(View.class), anyInt(), any(Rect.class));
        assertTrue(mAutofillProvider.isAutofillDisabled());
    }

    @Test
    public void testIsAutofillDisabledRefetchesDisabledAutofillManager() {
        moveToActivityWithAutofillManager(/* isEnabled= */ true);

        assertFalse(mAutofillProvider.isAutofillDisabled());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testSendPrefillRequestRefetchesDisabledAutofillManager() {
        AutofillManager newAutofillManager =
                moveToActivityWithAutofillManager(/* isEnabled= */ true);
        FormFieldDataBuilder fieldBuilder = new FormFieldDataBuilder();
        fieldBuilder.mServerPredictions = new String[] {"USERNAME"};
        FormData formData =
                new FormData(123, null, null, Collections.singletonList(fieldBuilder.build()));

        mAutofillProvider.sendPrefillRequest(formData);

        verify(newAutofillManager).notifyVirtualViewsReady(any(View.class), any());
    }

    @Test
    public void testQueryAutofillSuggestionRefetchesDisabledAutofillManager() {
        setContainerScreenGeometry(new Rect(0, 0, 200, 500), LOCATION_X, LOCATION_Y);
        when(mAutofillManager.isEnabled()).thenReturn(false);
        startSessionOnSingleField();

        AutofillManager newAutofillManager =
                moveToActivityWithAutofillManager(/* isEnabled= */ true);
        mAutofillProvider.queryAutofillSuggestion();

        verify(newAutofillManager).requestAutofill(any(View.class), anyInt(), any(Rect.class));
    }

    @Test
    public void testMidSessionEventsDoNotRefetchDisabledAutofillManager() {
        when(mAutofillManager.isEnabled()).thenReturn(false);
        startSessionOnSingleField();

        AutofillManager newAutofillManager =
                moveToActivityWithAutofillManager(/* isEnabled= */ true);
        mAutofillProvider.onFormFieldDidChange(
                /* index= */ 0, /* x= */ 0, /* y= */ 0, /* width= */ 50, /* height= */ 20);
        mAutofillProvider.onFormSubmitted(SubmissionSource.FORM_SUBMISSION);

        verify(newAutofillManager, never()).notifyValueChanged(any(), anyInt(), any());
        verify(newAutofillManager, never()).commit();

        // A new session started afterwards does refetch.
        startSessionOnSingleField();
        verify(newAutofillManager).notifyViewEntered(any(View.class), anyInt(), any(Rect.class));
    }

    @Test
    public void testNonActivityContextDoesNotRefetchBeforeMovingToActivity() {
        when(mAutofillManager.isEnabled()).thenReturn(false);
        AutofillManager otherManager = Mockito.mock(AutofillManager.class);
        when(mContext.getSystemService(AUTOFILL_SERVICE_NAME)).thenReturn(otherManager);

        startSessionOnSingleField();

        verify(otherManager, never()).isEnabled();
        verify(mAutofillManager, never()).unregisterCallback(any());
        assertTrue(mAutofillProvider.isAutofillDisabled());
    }

    @Test
    public void testActivityCreatedWebViewDoesNotRefetchWhenAutofillIsOff() {
        Activity activity = Robolectric.buildActivity(Activity.class).get();
        AutofillManager disabledManager = Mockito.mock(AutofillManager.class);
        when(disabledManager.isEnabled()).thenReturn(false);
        AutofillManager replacementManager = Mockito.mock(AutofillManager.class);
        final AutofillManager[] current = {disabledManager};
        Context activityContext =
                new ContextWrapper(activity) {
                    @Override
                    public Object getSystemService(String name) {
                        if (AUTOFILL_SERVICE_NAME.equals(name)) {
                            return current[0];
                        }
                        return super.getSystemService(name);
                    }
                };
        mAutofillProvider.switchToContext(new WeakReference<>(activityContext));
        current[0] = replacementManager;

        assertTrue(mAutofillProvider.isAutofillDisabled());
        startSessionOnSingleField();

        verify(replacementManager, never()).isEnabled();
        verify(disabledManager, never()).unregisterCallback(any());
    }

    @Test
    public void testRefetchRunsAtMostOnceWhenAutofillIsOffInActivity() {
        final int[] systemServiceCalls = {0};
        when(mAutofillManager.isEnabled()).thenReturn(false);
        AutofillManager disabledActivityManager = Mockito.mock(AutofillManager.class);
        when(disabledActivityManager.isEnabled()).thenReturn(false);
        Activity activity = Robolectric.buildActivity(Activity.class).get();
        Context activityContext =
                new ContextWrapper(activity) {
                    @Override
                    public Object getSystemService(String name) {
                        if (AUTOFILL_SERVICE_NAME.equals(name)) {
                            systemServiceCalls[0]++;
                            return disabledActivityManager;
                        }
                        return super.getSystemService(name);
                    }
                };
        mContextWrapper.setBaseContext(activityContext);

        assertTrue(mAutofillProvider.isAutofillDisabled());
        startSessionOnSingleField();
        assertTrue(mAutofillProvider.isAutofillDisabled());

        verify(mAutofillManager).unregisterCallback(any());
        // getSystemService() is queried only once on the first entry point; subsequent calls check
        // the stored Activity manager directly without re-fetching.
        assertEquals(1, systemServiceCalls[0]);
        verify(disabledActivityManager, times(4)).isEnabled();
    }

    @Test
    public void testDestroyUnregistersCallbackOfDisabledAutofillManager() {
        when(mAutofillManager.isEnabled()).thenReturn(false);

        mAutofillProvider.destroy();

        verify(mAutofillManager).unregisterCallback(any());
    }

    @Test
    public void testDestroyedWrapperDoesNotRefetchAutofillManager() {
        AutofillManager newAutofillManager =
                moveToActivityWithAutofillManager(/* isEnabled= */ true);

        mAutofillProvider.destroy();

        assertTrue(mAutofillProvider.isAutofillDisabled());
        verify(newAutofillManager, never()).isEnabled();
    }

    /**
     * Disables the initial (non-Activity) AutofillManager and moves {@link #mContextWrapper} to an
     * {@link Activity} returning a new {@link AutofillManager} with the given enabled state.
     */
    private AutofillManager moveToActivityWithAutofillManager(boolean isEnabled) {
        when(mAutofillManager.isEnabled()).thenReturn(false);
        AutofillManager newAutofillManager = Mockito.mock(AutofillManager.class);
        when(newAutofillManager.isEnabled()).thenReturn(isEnabled);
        Activity activity = Robolectric.buildActivity(Activity.class).get();
        Context activityContext =
                new ContextWrapper(activity) {
                    @Override
                    public Object getSystemService(String name) {
                        if (AUTOFILL_SERVICE_NAME.equals(name)) {
                            return newAutofillManager;
                        }
                        return super.getSystemService(name);
                    }
                };
        mContextWrapper.setBaseContext(activityContext);
        return newAutofillManager;
    }

    private void startSessionOnSingleField() {
        FormFieldDataBuilder fieldBuilder = new FormFieldDataBuilder();
        FormData formData =
                new FormData(
                        /* sessionId= */ 123,
                        /* name= */ null,
                        /* host= */ null,
                        Collections.singletonList(fieldBuilder.build()));
        mAutofillProvider.startAutofillSession(
                formData,
                /* focus= */ 0,
                /* x= */ 0,
                /* y= */ 0,
                /* width= */ 50,
                /* height= */ 20,
                /* hasServerPrediction= */ false);
    }

    @Test
    @Features.EnableFeatures({
        AndroidAutofillFeatures.ANDROID_AUTOFILL_FIELDS_UPDATED_ON_SELECT_NAME
    })
    public void testOnFormFieldDidChange_unfocusedFieldChanged_featureEnabled() {
        int sessionId = 123;
        FormFieldDataBuilder field1Builder = new FormFieldDataBuilder();
        FormFieldDataBuilder field2Builder = new FormFieldDataBuilder();
        FormData formData =
                new FormData(
                        sessionId,
                        /* name= */ null,
                        /* host= */ null,
                        Arrays.asList(field1Builder.build(), field2Builder.build()));

        int field0VirtualId = FormData.toFieldVirtualId(sessionId, (short) 0);
        int field1VirtualId = FormData.toFieldVirtualId(sessionId, (short) 1);

        // Start session focused on field 0.
        mAutofillProvider.startAutofillSession(
                formData,
                /* focus= */ 0,
                /* x= */ 0,
                /* y= */ 0,
                /* width= */ 50,
                /* height= */ 20,
                /* hasServerPrediction= */ false);

        assertEquals(field0VirtualId, mFocusVirtualId);

        // Mutate unfocused field 1.
        mAutofillProvider.onFormFieldDidChange(
                /* index= */ 1, /* x= */ 0, /* y= */ 0, /* width= */ 50, /* height= */ 20);

        // Focus should remain on field 0 and not shift to field 1.
        assertEquals(field0VirtualId, mFocusVirtualId);
        verify(mAutofillManager, never()).notifyViewExited(eq(mContainerView), eq(field0VirtualId));
        verify(mAutofillManager, never())
                .notifyViewEntered(eq(mContainerView), eq(field1VirtualId), any());
    }

    @Test
    @Features.DisableFeatures({
        AndroidAutofillFeatures.ANDROID_AUTOFILL_FIELDS_UPDATED_ON_SELECT_NAME
    })
    public void testOnFormFieldDidChange_unfocusedFieldChanged_featureDisabled() {
        int sessionId = 123;
        FormFieldDataBuilder field1Builder = new FormFieldDataBuilder();
        FormFieldDataBuilder field2Builder = new FormFieldDataBuilder();
        FormData formData =
                new FormData(
                        sessionId,
                        /* name= */ null,
                        /* host= */ null,
                        Arrays.asList(field1Builder.build(), field2Builder.build()));

        int field0VirtualId = FormData.toFieldVirtualId(sessionId, (short) 0);
        int field1VirtualId = FormData.toFieldVirtualId(sessionId, (short) 1);

        // Start session focused on field 0.
        mAutofillProvider.startAutofillSession(
                formData,
                /* focus= */ 0,
                /* x= */ 0,
                /* y= */ 0,
                /* width= */ 50,
                /* height= */ 20,
                /* hasServerPrediction= */ false);

        assertEquals(field0VirtualId, mFocusVirtualId);

        // Mutate unfocused field 1.
        mAutofillProvider.onFormFieldDidChange(
                /* index= */ 1, /* x= */ 0, /* y= */ 0, /* width= */ 50, /* height= */ 20);

        // With the feature disabled (killswitch), focus shifts to field 1.
        assertEquals(field1VirtualId, mFocusVirtualId);
        verify(mAutofillManager).notifyViewExited(eq(mContainerView), eq(field0VirtualId));
        verify(mAutofillManager).notifyViewEntered(eq(mContainerView), eq(field1VirtualId), any());
    }

    @Test
    @Features.EnableFeatures({
        AndroidAutofillFeatures.ANDROID_AUTOFILL_FIELDS_UPDATED_ON_SELECT_NAME
    })
    public void testOnFormFieldDidChange_noFocusedField_featureEnabled() {
        int sessionId = 123;
        FormFieldDataBuilder field1Builder = new FormFieldDataBuilder();
        FormFieldDataBuilder field2Builder = new FormFieldDataBuilder();
        FormData formData =
                new FormData(
                        sessionId,
                        /* name= */ null,
                        /* host= */ null,
                        Arrays.asList(field1Builder.build(), field2Builder.build()));

        int field0VirtualId = FormData.toFieldVirtualId(sessionId, (short) 0);

        // Start session focused on field 0.
        mAutofillProvider.startAutofillSession(
                formData,
                /* focus= */ 0,
                /* x= */ 0,
                /* y= */ 0,
                /* width= */ 50,
                /* height= */ 20,
                /* hasServerPrediction= */ false);

        // Clear focus.
        mAutofillProvider.onFocusChanged(
                /* focusOnForm= */ false,
                /* focusField= */ 0,
                /* x= */ 0,
                /* y= */ 0,
                /* width= */ 0,
                /* height= */ 0);

        // Value changes on field 0 when no field is currently focused.
        mAutofillProvider.onFormFieldDidChange(
                /* index= */ 0, /* x= */ 0, /* y= */ 0, /* width= */ 50, /* height= */ 20);

        // Focus should be gained on field 0.
        assertEquals(field0VirtualId, mFocusVirtualId);
        verify(mAutofillManager, Mockito.atLeastOnce())
                .notifyViewEntered(eq(mContainerView), eq(field0VirtualId), any());
    }

    FormData setupPrefillRequest(int sessionId) {
        FormFieldDataBuilder field1Builder = new FormFieldDataBuilder();
        field1Builder.mBounds =
                new RectF(/* left= */ 10, /* top= */ 20, /* right= */ 300, /* bottom= */ 60);
        field1Builder.mOrigin = "https://field.host.com/";

        FormFieldDataBuilder field2Builder = new FormFieldDataBuilder();
        field2Builder.mBounds =
                new RectF(/* left= */ 20, /* top= */ 100, /* right= */ 400, /* bottom= */ 200);
        field2Builder.mOrigin = "https://field2.host.com/";

        FormData formData =
                new FormData(
                        sessionId,
                        /* name= */ null,
                        /* host= */ "https://host.com/",
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
