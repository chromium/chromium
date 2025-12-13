// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.chrome.browser.profiles.Profile;

/** The implementation of {@link ExtensionUiBackend}. */
@NullMarked
@ServiceImpl(ExtensionUiBackend.class)
public class ExtensionUiBackendImpl implements ExtensionUiBackend {
    public ExtensionUiBackendImpl() {}

    @Override
    public boolean isEnabled(Profile profile) {
        return ExtensionActionsBridge.extensionsEnabled(profile);
    }
}
