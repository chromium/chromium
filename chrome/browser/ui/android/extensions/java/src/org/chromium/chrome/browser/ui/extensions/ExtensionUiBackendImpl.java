// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import android.graphics.Bitmap;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

/** The implementation of {@link ExtensionUiBackend}. */
@NullMarked
@ServiceImpl(ExtensionUiBackend.class)
public class ExtensionUiBackendImpl implements ExtensionUiBackend {
    public ExtensionUiBackendImpl() {}

    @Override
    public boolean isEnabled(Profile profile) {
        return ExtensionActionsBridge.extensionsEnabled(profile);
    }

    @Override
    public @Nullable Bitmap getExtensionOmniboxIcon(Profile profile, String extensionId) {
        return ExtensionUtilBridge.getExtensionOmniboxIcon(profile, extensionId);
    }

    @Override
    public void onOmniboxExtensionInputEntered(
            WebContents webContents, String url, boolean openInNewTab, boolean openInNewWindow) {
        ExtensionUtilBridge.onOmniboxExtensionInputEntered(
                webContents, url, openInNewTab, openInNewWindow);
    }
}
