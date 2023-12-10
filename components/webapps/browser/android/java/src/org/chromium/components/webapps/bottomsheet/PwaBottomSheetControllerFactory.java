// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.bottomsheet;

import android.content.Context;

import org.chromium.ui.base.WindowAndroid;

/** A factory for producing a {@link PwaBottomSheetController}. */
public class PwaBottomSheetControllerFactory {
    public static PwaBottomSheetController createPwaBottomSheetController(Context context) {
        return new PwaBottomSheetController(context);
    }

    public static void attach(WindowAndroid windowAndroid, PwaBottomSheetController controller) {
        PwaBottomSheetControllerProvider.attach(windowAndroid, controller);
    }

    public static void detach(PwaBottomSheetController controller) {
        PwaBottomSheetControllerProvider.detach(controller);
    }
}
