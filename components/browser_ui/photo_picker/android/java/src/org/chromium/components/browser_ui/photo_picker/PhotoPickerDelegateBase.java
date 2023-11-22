// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import org.chromium.ui.base.PhotoPickerDelegate;

/**
 * A shared base implementation of {@link PhotoPickerDelegate}.
 *
 * <p>Routes queries, of which flavor of the Media Picker to run, to the feature flag params.
 */
public abstract class PhotoPickerDelegateBase implements PhotoPickerDelegate {
    protected PhotoPickerDelegateBase() {}

    @Override
    public boolean launchViaActionGetContent() {
        return PhotoPickerFeatures.launchViaActionGetContent();
    }

    @Override
    public boolean launchViaActionPickImages() {
        return PhotoPickerFeatures.launchViaActionPickImages();
    }

    @Override
    public boolean launchViaActionPickImagesPlus() {
        return PhotoPickerFeatures.launchViaActionPickImagesPlus();
    }

    @Override
    public boolean launchRegularWithoutBrowse() {
        return PhotoPickerFeatures.launchRegularWithoutBrowse();
    }
}
