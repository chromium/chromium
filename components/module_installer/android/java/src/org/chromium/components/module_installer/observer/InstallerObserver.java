// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.observer;

import org.chromium.build.annotations.NullMarked;

/** Listener for 'module installed' notifications. */
@NullMarked
public interface InstallerObserver {
    void onModuleInstalled();
}
