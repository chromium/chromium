// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.crash.browser;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.NativeLibraries;
import org.chromium.build.annotations.DoNotInline;
import org.chromium.build.annotations.UsedByReflection;

@JNINamespace("crashpad")
final class CrashpadMain {
    @UsedByReflection("crashpad_linux.cc")
    public static void main(String[] argv) {
        loadNativeLibraries();
        CrashpadMainJni.get().crashpadMain(argv);
    }

    /**
     * CrashpadMain is in trichrome chrome apk/dex, but NativeLibraries only exists
     * in trichrome library apk/dex and not in trichrome chrome apk/dex.
     * Referencing a class that doesn't exist causes R8 to not be able to inline
     * CrashpadMainJni#get into CrashpadMain#main.
     *
     * References to NativeLibraries are in a separate method to avoid this issue
     * and allow CrashpadMainJni#get to be inlined into CrashpadMain#main.
     * @DoNotInline is to avoid any similar inlining issues whenever this method
     * is referenced.
     */
    @DoNotInline
    private static void loadNativeLibraries() {
        try {
            for (String library : NativeLibraries.LIBRARIES) {
                System.loadLibrary(library);
            }
        } catch (UnsatisfiedLinkError e) {
            throw new RuntimeException(e);
        }
    }

    @NativeMethods
    interface Natives {
        void crashpadMain(String[] argv);
    }
}
