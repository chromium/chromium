// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.builder;

/**
 * Provides information about a dynamic feature module.
 */
public interface ModuleDescriptor {
    /**
     * Returns the list of native library names this module requires at runtime.
     */
    String[] getLibraries();
    /**
     * Returns the list of PAK resources files this module contains.
     */
    String[] getPaks();
}
