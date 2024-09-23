// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.engine;

import org.chromium.build.BuildConfig;

/** Factory used to build concrete engines. */
public class EngineFactory {
    public InstallEngine getEngine() {
        return BuildConfig.IS_BUNDLE ? new SplitCompatEngine() : new ApkEngine();
    }
}
