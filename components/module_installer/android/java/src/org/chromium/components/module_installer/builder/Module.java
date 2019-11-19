// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.builder;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildConfig;
import org.chromium.base.StrictModeContext;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.module_installer.engine.InstallEngine;
import org.chromium.components.module_installer.engine.InstallListener;
import org.chromium.components.module_installer.util.Timer;

import java.util.HashSet;
import java.util.Set;
import java.util.TreeSet;

/**
 * Represents a feature module. Can be used to install the module, access its interface, etc. See
 * {@link ModuleInterface} for how to conveniently create an instance of the module class for a
 * specific feature module.
 *
 * @param <T> The interface of the module
 */
@JNINamespace("module_installer")
public class Module<T> {
    private static final Set<String> sInitializedModules = new HashSet<>();
    private static Set<String> sPendingNativeRegistrations = new TreeSet<>();

    private final String mName;
    private final Class<T> mInterfaceClass;
    private final String mImplClassName;
    private T mImpl;
    private InstallEngine mInstaller;

    /**
     * To be called after the main native library has been loaded. Any module instances created
     * before the native library is loaded have their native component queued for loading and
     * registration. Calling this methed completes that process.
     */
    public static void doDeferredNativeRegistrations() {
        if (sPendingNativeRegistrations == null) return;
        for (String name : sPendingNativeRegistrations) {
            loadNative(name);
        }
        sPendingNativeRegistrations = null;
    }

    /**
     * Instantiates a module.
     *
     * @param name The module's name as used with {@link ModuleInstaller}.
     * @param interfaceClass {@link Class} object of the module interface.
     * @param implClassName fully qualified class name of the implementation of the module's
     *                      interface.
     */
    public Module(String name, Class<T> interfaceClass, String implClassName) {
        mName = name;
        mInterfaceClass = interfaceClass;
        mImplClassName = implClassName;
    }

    @VisibleForTesting
    public InstallEngine getInstallEngine() {
        if (mInstaller == null) {
            try (Timer timer = new Timer()) {
                mInstaller = new ModuleEngine(mImplClassName);
            }
        }
        return mInstaller;
    }

    @VisibleForTesting
    public void setInstallEngine(InstallEngine engine) {
        mInstaller = engine;
    }

    /**
     * Returns true if the module is currently installed and can be accessed.
     */
    public boolean isInstalled() {
        try (Timer timer = new Timer()) {
            return getInstallEngine().isInstalled(mName);
        }
    }

    /**
     * Requests install of the module.
     */
    public void install(InstallListener listener) {
        try (Timer timer = new Timer()) {
            assert !isInstalled();
            getInstallEngine().install(mName, listener);
        }
    }

    /**
     * Requests deferred install of the module.
     */
    public void installDeferred() {
        try (Timer timer = new Timer()) {
            getInstallEngine().installDeferred(mName);
        }
    }

    /**
     * Returns the implementation of the module interface. Must only be called if the module is
     * installed.
     */
    public T getImpl() {
        try (Timer timer = new Timer()) {
            if (mImpl != null) return mImpl;

            assert isInstalled();
            // Load the module's native code and/or resources if they are present, and the Chrome
            // native library itself has been loaded.
            if (sPendingNativeRegistrations == null) {
                loadNative(mName);
            } else {
                // We have to defer native initialization because VR is calling getImpl in early
                // startup. As soon as VR stops doing that we want to deprecate deferred native
                // initialization.
                sPendingNativeRegistrations.add(mName);
            }
            mImpl = mInterfaceClass.cast(instantiateReflectively(mImplClassName));
            return mImpl;
        }
    }

    private static void loadNative(String name) {
        // Can only initialize native once per lifetime of Chrome.
        if (sInitializedModules.contains(name)) return;
        ModuleDescriptor moduleDescriptor = loadModuleDescriptor(name);
        String[] libraries = moduleDescriptor.getLibraries();
        String[] paks = moduleDescriptor.getPaks();
        if (libraries.length > 0 || paks.length > 0) {
            ModuleJni.get().loadNative(name, libraries, paks);
        }
        sInitializedModules.add(name);
    }

    /**
     * Loads the {@link ModuleDescriptor} for a module.
     *
     * For bundles, uses reflection to load the descriptor from inside the
     * module. For APKs, returns an empty descriptor since APKs won't have
     * descriptors packaged into them.
     *
     * @param name The module's name.
     * @return The module's {@link ModuleDescriptor}.
     */
    private static ModuleDescriptor loadModuleDescriptor(String name) {
        if (!BuildConfig.IS_BUNDLE) {
            return new ModuleDescriptor() {
                @Override
                public String[] getLibraries() {
                    return new String[0];
                }

                @Override
                public String[] getPaks() {
                    return new String[0];
                }
            };
        }

        return (ModuleDescriptor) instantiateReflectively(
                "org.chromium.components.module_installer.builder.ModuleDescriptor_" + name);
    }

    /**
     * Instantiates an object via reflection.
     *
     * Ignores strict mode violations since accessing code in a module may cause its DEX file to be
     * loaded and on some devices that can cause such a violation.
     *
     * @param className The object's class name.
     * @return The object.
     */
    private static Object instantiateReflectively(String className) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return Class.forName(className).newInstance();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    @NativeMethods
    interface Natives {
        void loadNative(String name, String[] libraries, String[] paks);
    }
}
