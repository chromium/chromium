// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.util;

import org.chromium.build.annotations.NullMarked;

/** Interface for the classes that need to be notified of password echo setting state changes. */
@NullMarked
public interface PasswordEchoSettingObserver {
    void onSettingChange();
}
