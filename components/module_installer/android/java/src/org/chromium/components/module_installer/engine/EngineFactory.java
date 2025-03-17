// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.engine;

import org.chromium.base.BundleUtils;
import org.chromium.build.annotations.NullMarked;

/** Factory used to build concrete engines. */
@NullMarked
public class EngineFactory {
    public InstallEngine getEngine() {
        return BundleUtils.isBundle() ? new SplitCompatEngine() : new ApkEngine();
    }
}
