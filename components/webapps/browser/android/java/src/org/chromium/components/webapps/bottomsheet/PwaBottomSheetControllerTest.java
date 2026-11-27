// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.bottomsheet;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Bitmap;

import org.jni_zero.JniUniquePtr;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link PwaBottomSheetController}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class PwaBottomSheetControllerTest {
    private static class FakeWebContents extends MockWebContents {
        final List<WebContentsObserver> mObservers = new ArrayList<>();

        @Override
        public void addObserver(WebContentsObserver observer) {
            mObservers.add(observer);
        }

        @Override
        public void removeObserver(WebContentsObserver observer) {
            mObservers.remove(observer);
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private JniUniquePtr<PwaBottomSheetController.NativePwaBottomSheetController>
            mNativePwaBottomSheetController;

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private NavigationHandle mNavigationHandle;
    @Captor private ArgumentCaptor<BottomSheetObserver> mObserverCaptor;

    private PwaBottomSheetController mController;
    private FakeWebContents mWebContents;
    private Bitmap mIcon;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mController = new PwaBottomSheetController(activity);
        mWebContents = new FakeWebContents();
        mIcon = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);

        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(new UnownedUserDataHost());
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);
    }

    @Test
    public void testHideSheetStopsObservingWebContents() {
        BottomSheetObserver sheetObserver = requestShowAndCaptureObserver(/* showResult= */ true);
        Assert.assertEquals(1, mWebContents.mObservers.size());
        WebContentsObserver webContentsObserver = mWebContents.mObservers.get(0);

        when(mNavigationHandle.hasCommitted()).thenReturn(true);
        for (WebContentsObserver observer : new ArrayList<>(mWebContents.mObservers)) {
            observer.didFinishNavigationInPrimaryMainFrame(mNavigationHandle);
        }
        verify(mBottomSheetController, times(1)).hideContent(any(), eq(true));

        sheetObserver.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.NONE);

        Assert.assertTrue(mWebContents.mObservers.isEmpty());
        Assert.assertNull(webContentsObserver.getWebContents());
        Assert.assertNull(mController.getWebContentsForTesting());
        verify(mBottomSheetController).removeObserver(sheetObserver);
        verify(mNativePwaBottomSheetController).destroy();
    }

    @Test
    public void testRequestShowContentReturnsFalse_cleansUpWithoutObservingWebContents() {
        BottomSheetObserver sheetObserver = requestShowAndCaptureObserver(/* showResult= */ false);

        Assert.assertTrue(mWebContents.mObservers.isEmpty());
        Assert.assertNull(mController.getWebContentsForTesting());
        verify(mBottomSheetController).hideContent(any(), eq(false));
        verify(mBottomSheetController, times(1)).removeObserver(sheetObserver);
        verify(mNativePwaBottomSheetController, times(1)).destroy();
        mController.addWebAppScreenshot(mIcon);
    }

    @Test
    public void testEarlyReturnWhenBottomSheetControllerNull_cleansUpReferences() {
        BottomSheetControllerProvider.setInstanceForTesting(null);

        requestBottomSheetInstaller();

        Assert.assertNull(mController.getWebContentsForTesting());
        Assert.assertTrue(mWebContents.mObservers.isEmpty());
        verify(mNativePwaBottomSheetController, times(1)).destroy();
        mController.addWebAppScreenshot(mIcon);
    }

    @Test
    public void testAddWebAppScreenshotAfterDismissal_isNoOp() {
        BottomSheetObserver sheetObserver = requestShowAndCaptureObserver(/* showResult= */ true);
        sheetObserver.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.NONE);

        mController.addWebAppScreenshot(mIcon);

        Assert.assertNull(mController.getWebContentsForTesting());
        verify(mNativePwaBottomSheetController, times(1)).destroy();
    }

    private void requestBottomSheetInstaller() {
        mController.requestBottomSheetInstaller(
                mNativePwaBottomSheetController,
                mWindowAndroid,
                mWebContents,
                mIcon,
                /* isAdaptiveIcon= */ false,
                "Test App",
                "https://example.com",
                "Description");
    }

    private BottomSheetObserver requestShowAndCaptureObserver(boolean showResult) {
        when(mBottomSheetController.requestShowContent(any(), eq(true))).thenReturn(showResult);
        requestBottomSheetInstaller();
        verify(mBottomSheetController).addObserver(mObserverCaptor.capture());
        return mObserverCaptor.getValue();
    }
}
