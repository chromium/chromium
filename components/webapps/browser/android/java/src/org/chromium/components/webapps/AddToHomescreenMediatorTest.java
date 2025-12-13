// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.util.Pair;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for the {@link AddToHomescreenMediator} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class AddToHomescreenMediatorTest {

    @Rule public MockitoRule mRule = MockitoJUnit.rule();

    @Mock private AddToHomescreenMediator.Natives mNativeMock;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private WebContents mWebContents;
    @Mock private Runnable mOnFlowCompleted;

    private final PropertyModel mPropertyModel =
            new PropertyModel.Builder(AddToHomescreenProperties.ALL_KEYS).build();

    private static final long NATIVE_POINTER = 12;

    @Before
    public void setUp() {
        AddToHomescreenMediatorJni.setInstanceForTesting(mNativeMock);
        when(mNativeMock.initialize(Mockito.any(), Mockito.any())).thenReturn(NATIVE_POINTER);
    }

    @Test
    @Feature({"Webapp"})
    public void testNativeApp() {
        AddToHomescreenMediator addToHomescreenMediator =
                new AddToHomescreenMediator(
                        mPropertyModel, mWindowAndroid, mWebContents, mOnFlowCompleted);

        // Prepare test parameters.
        Bitmap icon = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        AppData appData = new AppData(null, null);
        appData.setPackageInfo("Title", null, 3.4f, "Install", null, null);

        addToHomescreenMediator.setNativeAppInfo(appData);
        addToHomescreenMediator.setIcon(icon, false);

        // Assert #setNativeAppInfo assigns the correct properties to the model.
        Assert.assertEquals("Title", mPropertyModel.get(AddToHomescreenProperties.TITLE));
        Assert.assertEquals(
                new Pair(icon, false), mPropertyModel.get(AddToHomescreenProperties.ICON));
        Assert.assertEquals(AppType.NATIVE, mPropertyModel.get(AddToHomescreenProperties.TYPE));
        Assert.assertEquals(
                3.4f, mPropertyModel.get(AddToHomescreenProperties.NATIVE_APP_RATING), .01);
        Assert.assertEquals(true, mPropertyModel.get(AddToHomescreenProperties.CAN_SUBMIT));
        Assert.assertEquals(
                "Install",
                mPropertyModel.get(AddToHomescreenProperties.NATIVE_INSTALL_BUTTON_TEXT));
    }

    @Test
    @Feature({"Webapp"})
    public void testWebApp() {
        AddToHomescreenMediator addToHomescreenMediator =
                new AddToHomescreenMediator(
                        mPropertyModel, mWindowAndroid, mWebContents, mOnFlowCompleted);

        // Prepare test parameters.
        Bitmap icon = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        addToHomescreenMediator.setWebAppInfo("Title", "google.com", AppType.SHORTCUT);
        addToHomescreenMediator.setIcon(icon, true);

        // Assert #setWebAppInfo and #setIcon assign the correct properties to the model.
        Assert.assertEquals("Title", mPropertyModel.get(AddToHomescreenProperties.TITLE));
        Assert.assertEquals("google.com", mPropertyModel.get(AddToHomescreenProperties.URL));
        Assert.assertEquals(AppType.SHORTCUT, mPropertyModel.get(AddToHomescreenProperties.TYPE));
        Assert.assertNotEquals(icon, mPropertyModel.get(AddToHomescreenProperties.ICON).first);
        Assert.assertTrue(mPropertyModel.get(AddToHomescreenProperties.ICON).second);
        Assert.assertEquals(true, mPropertyModel.get(AddToHomescreenProperties.CAN_SUBMIT));
    }

    @Test
    @Feature({"Webapp"})
    public void testOnAddToHomescreen() {
        AddToHomescreenMediator addToHomescreenMediator =
                new AddToHomescreenMediator(
                        mPropertyModel, mWindowAndroid, mWebContents, mOnFlowCompleted);

        addToHomescreenMediator.onAddToHomescreen("Title");

        verify(mNativeMock).addToHomescreen(NATIVE_POINTER, "Title");

        // Verify that the native mediator is destroyed and the callback runs.
        verify(mNativeMock).destroy(NATIVE_POINTER);
        verify(mOnFlowCompleted).run();
    }

    @Test
    @Feature({"Webapp"})
    public void testOnViewDismissed() {
        AddToHomescreenMediator addToHomescreenMediator =
                new AddToHomescreenMediator(
                        mPropertyModel, mWindowAndroid, mWebContents, mOnFlowCompleted);

        addToHomescreenMediator.onViewDismissed();

        verify(mNativeMock).onUiDismissed(NATIVE_POINTER);

        // Verify that the native mediator is destroyed and the callback runs.
        verify(mNativeMock).destroy(NATIVE_POINTER);
        verify(mOnFlowCompleted).run();
    }
}
