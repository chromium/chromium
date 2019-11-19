// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.engine;

/**
 * Broadcast listener for dynamic feature module installs.
 */
public interface InstallListener {
    /**
     * Called when the install has completed.
     *
     * @param success True if the module was installed successfully.
     */
    void onComplete(boolean success);
}
