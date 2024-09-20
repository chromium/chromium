// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.builder;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.build.BuildConfig;
import org.chromium.components.module_installer.engine.InstallEngine;
import org.chromium.components.module_installer.engine.InstallListener;

/**
 * Represents a feature module. Can be used to install the module, access its interface, etc. See
 * {@link ModuleInterface} for how to conveniently create an instance of the module class for a
 * specific feature module.
 *
 * @param <T> The interface of the module
 */
@JNINamespace("module_installer")
public class Module<T> {
    private final String mName;
    private final Class<T> mInterfaceClass;
    private final String mImplClassName;
    private ModuleDescriptor mModuleDescriptor;
    private Context mContext;
    private T mImpl;
    private InstallEngine mInstaller;
    private boolean mIsNativeLoaded;

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
            mInstaller = new ModuleEngine(mImplClassName);
        }
        return mInstaller;
    }

    @VisibleForTesting
    public void setInstallEngine(InstallEngine engine) {
        mInstaller = engine;
    }

    /** Returns true if the module is currently installed and can be accessed. */
    public boolean isInstalled() {
        return getInstallEngine().isInstalled(mName);
    }

    /** Requests install of the module. */
    public void install(InstallListener listener) {
        assert !isInstalled();
        getInstallEngine().install(mName, listener);
    }

    /** Requests deferred install of the module. */
    public void installDeferred() {
        getInstallEngine().installDeferred(mName);
    }

    /**
     * Returns the implementation of the module interface. Must only be called if the module is
     * installed.
     */
    public T getImpl() {
        T ret = mImpl;
        if (ret != null) {
            return ret;
        }
        assert isInstalled();
        ModuleDescriptor moduleDescriptor = getModuleDescriptor();
        if (moduleDescriptor.getLoadNativeOnGetImpl()) {
            // Load the module's native code and/or resources if they are present, and the
            // Chrome native library itself has been loaded.
            ensureNativeLoaded();
        }

        Object impl = instantiateReflectively(mImplClassName);
        try {
            ret = mInterfaceClass.cast(impl);
            mImpl = ret;
        } catch (ClassCastException e) {
            ClassLoader interfaceClassLoader = mInterfaceClass.getClassLoader();
            ClassLoader implClassLoader = impl.getClass().getClassLoader();
            throw new RuntimeException(
                    "Failure casting "
                            + mName
                            + " module class, interface ClassLoader: "
                            + interfaceClassLoader
                            + " (parent "
                            + interfaceClassLoader.getParent()
                            + ")"
                            + ", impl ClassLoader: "
                            + implClassLoader
                            + " (parent "
                            + implClassLoader.getParent()
                            + ")"
                            + ", equal: "
                            + interfaceClassLoader.equals(implClassLoader)
                            + " (parents equal: "
                            + interfaceClassLoader.getParent().equals(implClassLoader.getParent())
                            + ")",
                    e);
        }
        return ret;
    }

    /**
     * Loads native libraries and/or resources if and only if this is not already done, assuming
     * that the module is installed, and enableNativeLoad() has been called.
     */
    public void ensureNativeLoaded() {
        // Can only initialize native once per lifetime of Chrome.
        if (mIsNativeLoaded) return;
        assert LibraryLoader.getInstance().isInitialized();
        ModuleDescriptor moduleDescriptor = getModuleDescriptor();
        String[] libraries = moduleDescriptor.getLibraries();
        String[] paks = moduleDescriptor.getPaks();
        if (libraries.length > 0 || paks.length > 0) {
            ModuleJni.get().loadNative(mName, libraries, paks);
        }
        mIsNativeLoaded = true;
    }

    /**
     * Loads the {@link ModuleDescriptor} for a module.
     *
     * For bundles, uses reflection to load the descriptor from inside the
     * module. For APKs, returns an empty descriptor since APKs won't have
     * descriptors packaged into them.
     *
     * @return The module's {@link ModuleDescriptor}.
     */
    private ModuleDescriptor getModuleDescriptor() {
        ModuleDescriptor ret = mModuleDescriptor;
        if (ret == null) {
            if (BuildConfig.IS_BUNDLE) {
                ret =
                        (ModuleDescriptor)
                                instantiateReflectively(
                                        "org.chromium.components.module_installer.builder.ModuleDescriptor_"
                                                + mName);
            } else {
                ret =
                        new ModuleDescriptor() {
                            @Override
                            public String[] getLibraries() {
                                return new String[0];
                            }

                            @Override
                            public String[] getPaks() {
                                return new String[0];
                            }

                            @Override
                            public boolean getLoadNativeOnGetImpl() {
                                return false;
                            }
                        };
            }
            mModuleDescriptor = ret;
        }
        return ret;
    }

    /** Returns the Context associated with the module. */
    public Context getContext() {
        // Ensure mContext is initialized.
        getImpl();
        return mContext;
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
    private Object instantiateReflectively(String className) {
        Context context = mContext;
        if (context == null) {
            context = ContextUtils.getApplicationContext();
            String moduleName = mName;
            if (BundleUtils.isIsolatedSplitInstalled(moduleName)) {
                context = BundleUtils.createIsolatedSplitContext(context, moduleName);
            }
        }
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            Object ret = context.getClassLoader().loadClass(className).newInstance();
            // Cache only if reflection succeeded since the module might not have been installed.
            mContext = context;
            return ret;
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    @NativeMethods
    interface Natives {
        void loadNative(String name, String[] libraries, String[] paks);
    }
}
