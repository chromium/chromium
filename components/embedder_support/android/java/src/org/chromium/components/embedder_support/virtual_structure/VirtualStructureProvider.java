// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.virtual_structure;

import android.view.ViewStructure;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.WebContents;

/**
 * Delegate to replace the default implementation of ContentView.onProvideVirtualStructure. Used by
 * embedders to customize the way they populate a ViewStructure based on the current WebContents.
 */
@NullMarked
public interface VirtualStructureProvider {

    void provideVirtualStructureForWebContents(
            final ViewStructure structure, WebContents webContents);
}
