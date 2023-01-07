// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.engine;

/**
 * Engine used by APK builds.
 *
 * This class exposes no behavior - it's main purpose is to help our compile-time optimizers
 * to exclude libraries that should not be included in APK builds (for example, SplitCompat).
 */
class ApkEngine implements InstallEngine {
    @Override
    public void install(String moduleName, InstallListener listener) {
        // This method should never be called in APK builds.
        // Adding a fallback call for completeness.
        listener.onComplete(false);
    }
}
