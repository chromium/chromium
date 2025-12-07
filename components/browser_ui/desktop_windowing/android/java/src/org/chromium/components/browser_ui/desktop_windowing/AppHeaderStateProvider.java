// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.desktop_windowing;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Interface to provide app header information. */
@NullMarked
public interface AppHeaderStateProvider {

    /**
     * @return The window's {@link AppHeaderState} information.
     */
    @Nullable AppHeaderState getAppHeaderState();
}
