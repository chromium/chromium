// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import org.chromium.ui.base.PhotoPickerDelegate;

/**
 * A shared base implementation of {@link PhotoPickerDelegate}.
 *
 * Routes video support queries to the feature flag.
 */
public abstract class PhotoPickerDelegateBase implements PhotoPickerDelegate {
    protected PhotoPickerDelegateBase() {}

    @Override
    public boolean preferAndroidMediaPicker() {
        return PhotoPickerFeatures.ANDROID_MEDIA_PICKER_SUPPORT.isEnabled();
    }
}
