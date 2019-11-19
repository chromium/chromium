// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.builder;

import android.app.Activity;

import org.chromium.base.StrictModeContext;
import org.chromium.components.module_installer.engine.EngineFactory;
import org.chromium.components.module_installer.engine.InstallEngine;
import org.chromium.components.module_installer.engine.InstallListener;

/**
 * Proxy engine used by {@link Module}.
 * This engine's main purpose is to change the behaviour of isInstalled(...) so that
 * modules can be moved in and out from the base more easily.
 */
class ModuleEngine implements InstallEngine {
    private InstallEngine mInstallEngine;
    private EngineFactory mEngineFactory;

    private final String mImplClassName;

    public ModuleEngine(String implClassName) {
        this(implClassName, new EngineFactory());
    }

    public ModuleEngine(String implClassName, EngineFactory engineFactory) {
        mImplClassName = implClassName;
        mEngineFactory = engineFactory;
    }

    @Override
    public void initActivity(Activity activity) {
        getEngine().initActivity(activity);
    }

    @Override
    public boolean isInstalled(String moduleName) {
        // Accessing classes in the module may cause its DEX file to be loaded. And on some
        // devices that causes a read mode violation.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            Class.forName(mImplClassName);
            return true;
        } catch (ClassNotFoundException e) {
            return false;
        }
    }

    @Override
    public void installDeferred(String moduleName) {
        getEngine().installDeferred(moduleName);
    }

    @Override
    public void install(String moduleName, InstallListener listener) {
        getEngine().install(moduleName, listener);
    }

    private InstallEngine getEngine() {
        // Lazily instantiate the engine - related to crbug/1010887.
        if (mInstallEngine == null) {
            mInstallEngine = mEngineFactory.getEngine();
        }
        return mInstallEngine;
    }
}
