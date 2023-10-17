// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps;

import android.graphics.Bitmap;
import android.util.Pair;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for the {@link AddToHomescreenViewBinder} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class AddToHomescreenViewBinderTest {
    @Test
    public void testViewBinder() {
        AddToHomescreenDialogView view = Mockito.mock(AddToHomescreenDialogView.class);
        Bitmap icon = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);

        // Construct the model.
        PropertyModel viewModel =
                new PropertyModel.Builder(AddToHomescreenProperties.ALL_KEYS).build();
        viewModel.set(AddToHomescreenProperties.CAN_SUBMIT, true);
        viewModel.set(AddToHomescreenProperties.ICON, new Pair<>(icon, false));
        viewModel.set(AddToHomescreenProperties.NATIVE_INSTALL_BUTTON_TEXT, "Install");
        viewModel.set(AddToHomescreenProperties.NATIVE_APP_RATING, 3.4f);
        viewModel.set(AddToHomescreenProperties.TITLE, "My App");
        viewModel.set(AddToHomescreenProperties.TYPE, AppType.NATIVE);
        viewModel.set(AddToHomescreenProperties.URL, "google.com");

        // Invoke the binder.
        PropertyModelChangeProcessor.create(viewModel, view, AddToHomescreenViewBinder::bind);

        // Verify that the binder called the right methods on the view, with the correct arguments.
        Mockito.verify(view, Mockito.times(1)).setCanSubmit(true);
        Mockito.verify(view, Mockito.times(1)).setIcon(icon, false);
        Mockito.verify(view, Mockito.times(1)).setNativeInstallButtonText("Install");
        Mockito.verify(view, Mockito.times(1)).setNativeAppRating(3.4f);
        Mockito.verify(view, Mockito.times(1)).setTitle("My App");
        Mockito.verify(view, Mockito.times(1)).setType(AppType.NATIVE);
        Mockito.verify(view, Mockito.times(1)).setUrl("google.com");
    }
}
