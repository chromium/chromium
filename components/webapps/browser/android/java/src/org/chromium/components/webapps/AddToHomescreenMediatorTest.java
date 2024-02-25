// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps;

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
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for the {@link AddToHomescreenMediator} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class AddToHomescreenMediatorTest {
    @Rule public JniMocker mocker = new JniMocker();

    @Mock private AddToHomescreenMediator.Natives mNativeMock;
    @Mock private WindowAndroid mWindowAndroid;

    private PropertyModel mPropertyModel =
            new PropertyModel.Builder(AddToHomescreenProperties.ALL_KEYS).build();

    private static final long NATIVE_POINTER = 12;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(AddToHomescreenMediatorJni.TEST_HOOKS, mNativeMock);
        when(mNativeMock.initialize(Mockito.any())).thenReturn(NATIVE_POINTER);
    }

    @Test
    @Feature({"Webapp"})
    public void testNativeApp() {
        AddToHomescreenMediator addToHomescreenMediator =
                new AddToHomescreenMediator(mPropertyModel, mWindowAndroid);

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
                new AddToHomescreenMediator(mPropertyModel, mWindowAndroid);

        // Prepare test parameters.
        Bitmap icon = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        addToHomescreenMediator.setWebAppInfo("Title", "google.com", AppType.SHORTCUT);
        addToHomescreenMediator.setIcon(icon, true);

        // Assert #setWebAppInfoWithIcon assigns the correct properties to the model.
        Assert.assertEquals("Title", mPropertyModel.get(AddToHomescreenProperties.TITLE));
        Assert.assertEquals("google.com", mPropertyModel.get(AddToHomescreenProperties.URL));
        Assert.assertEquals(AppType.SHORTCUT, mPropertyModel.get(AddToHomescreenProperties.TYPE));
        Assert.assertNotEquals(icon, mPropertyModel.get(AddToHomescreenProperties.ICON).first);
        Assert.assertTrue(mPropertyModel.get(AddToHomescreenProperties.ICON).second);
        Assert.assertEquals(true, mPropertyModel.get(AddToHomescreenProperties.CAN_SUBMIT));
    }
}
