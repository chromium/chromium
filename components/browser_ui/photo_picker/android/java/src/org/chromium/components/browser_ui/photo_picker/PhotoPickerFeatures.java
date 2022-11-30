// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import org.chromium.base.Features;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Provides an API for querying the status of Photo Picker features.
 */
@JNINamespace("photo_picker::features")
public class PhotoPickerFeatures extends Features {
    public static final String ANDROID_MEDIA_PICKER_SUPPORT_NAME = "AndroidMediaPickerSupport";
    public static final String PHOTO_PICKER_VIDEO_SUPPORT_NAME = "PhotoPickerVideoSupport";

    // This list must be kept in sync with kFeaturesExposedToJava in native.
    public static final PhotoPickerFeatures ANDROID_MEDIA_PICKER_SUPPORT =
            new PhotoPickerFeatures(0, ANDROID_MEDIA_PICKER_SUPPORT_NAME);
    public static final PhotoPickerFeatures PHOTO_PICKER_VIDEO_SUPPORT =
            new PhotoPickerFeatures(1, PHOTO_PICKER_VIDEO_SUPPORT_NAME);

    private final int mOrdinal;

    private PhotoPickerFeatures(int ordinal, String name) {
        super(name);
        mOrdinal = ordinal;
    }

    @Override
    protected long getFeaturePointer() {
        return PhotoPickerFeaturesJni.get().getFeature(mOrdinal);
    }

    @NativeMethods
    interface Natives {
        long getFeature(int ordinal);
    }
}
