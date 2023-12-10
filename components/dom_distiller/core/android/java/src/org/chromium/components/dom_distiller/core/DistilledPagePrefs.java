// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.dom_distiller.core;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.dom_distiller.mojom.FontFamily;
import org.chromium.dom_distiller.mojom.Theme;

import java.util.HashMap;
import java.util.Map;

/** Wrapper for the dom_distiller::DistilledPagePrefs. */
@JNINamespace("dom_distiller::android")
public final class DistilledPagePrefs {

    private final long mDistilledPagePrefsAndroid;
    private Map<Observer, DistilledPagePrefsObserverWrapper> mObserverMap;

    /** Observer interface for observing DistilledPagePrefs changes. */
    public interface Observer {
        void onChangeFontFamily(int font);

        void onChangeTheme(int theme);

        void onChangeFontScaling(float scaling);
    }

    /** Wrapper for dom_distiller::android::DistilledPagePrefsObserverAndroid. */
    static class DistilledPagePrefsObserverWrapper {
        private final Observer mDistilledPagePrefsObserver;
        private final long mNativeDistilledPagePrefsObserverAndroidPtr;

        public DistilledPagePrefsObserverWrapper(Observer observer) {
            mNativeDistilledPagePrefsObserverAndroidPtr =
                    DistilledPagePrefsJni.get().initObserverAndroid(this);
            mDistilledPagePrefsObserver = observer;
        }

        @CalledByNative("DistilledPagePrefsObserverWrapper")
        private void onChangeFontFamily(int fontFamily) {
            FontFamily.validate(fontFamily);
            mDistilledPagePrefsObserver.onChangeFontFamily(fontFamily);
        }

        @CalledByNative("DistilledPagePrefsObserverWrapper")
        private void onChangeTheme(int theme) {
            Theme.validate(theme);
            mDistilledPagePrefsObserver.onChangeTheme(theme);
        }

        @CalledByNative("DistilledPagePrefsObserverWrapper")
        private void onChangeFontScaling(float scaling) {
            mDistilledPagePrefsObserver.onChangeFontScaling(scaling);
        }

        public void destroy() {
            DistilledPagePrefsJni.get()
                    .destroyObserverAndroid(mNativeDistilledPagePrefsObserverAndroidPtr);
        }

        public long getNativePtr() {
            return mNativeDistilledPagePrefsObserverAndroidPtr;
        }
    }

    DistilledPagePrefs(long distilledPagePrefsPtr) {
        mDistilledPagePrefsAndroid =
                DistilledPagePrefsJni.get().init(DistilledPagePrefs.this, distilledPagePrefsPtr);
        mObserverMap = new HashMap<Observer, DistilledPagePrefsObserverWrapper>();
    }

    /*
     * Adds the observer to listen to changes in DistilledPagePrefs.
     * @return whether the observerMap was changed as a result of the call.
     */
    public boolean addObserver(Observer obs) {
        if (mObserverMap.containsKey(obs)) return false;
        DistilledPagePrefsObserverWrapper wrappedObserver =
                new DistilledPagePrefsObserverWrapper(obs);
        DistilledPagePrefsJni.get()
                .addObserver(
                        mDistilledPagePrefsAndroid,
                        DistilledPagePrefs.this,
                        wrappedObserver.getNativePtr());
        mObserverMap.put(obs, wrappedObserver);
        return true;
    }

    /*
     * Removes the observer and unregisters it from DistilledPagePrefs changes.
     * @return whether the observer was removed as a result of the call.
     */
    public boolean removeObserver(Observer obs) {
        DistilledPagePrefsObserverWrapper wrappedObserver = mObserverMap.remove(obs);
        if (wrappedObserver == null) return false;
        DistilledPagePrefsJni.get()
                .removeObserver(
                        mDistilledPagePrefsAndroid,
                        DistilledPagePrefs.this,
                        wrappedObserver.getNativePtr());
        wrappedObserver.destroy();
        return true;
    }

    public void setFontFamily(int fontFamily) {
        FontFamily.validate(fontFamily);
        DistilledPagePrefsJni.get()
                .setFontFamily(mDistilledPagePrefsAndroid, DistilledPagePrefs.this, fontFamily);
    }

    public int getFontFamily() {
        return DistilledPagePrefsJni.get()
                .getFontFamily(mDistilledPagePrefsAndroid, DistilledPagePrefs.this);
    }

    public void setTheme(int theme) {
        Theme.validate(theme);
        DistilledPagePrefsJni.get()
                .setTheme(mDistilledPagePrefsAndroid, DistilledPagePrefs.this, theme);
    }

    public int getTheme() {
        return DistilledPagePrefsJni.get()
                .getTheme(mDistilledPagePrefsAndroid, DistilledPagePrefs.this);
    }

    public void setFontScaling(float scaling) {
        DistilledPagePrefsJni.get()
                .setFontScaling(mDistilledPagePrefsAndroid, DistilledPagePrefs.this, scaling);
    }

    public float getFontScaling() {
        return DistilledPagePrefsJni.get()
                .getFontScaling(mDistilledPagePrefsAndroid, DistilledPagePrefs.this);
    }

    @NativeMethods
    interface Natives {
        long init(DistilledPagePrefs caller, long distilledPagePrefPtr);

        void setFontFamily(
                long nativeDistilledPagePrefsAndroid, DistilledPagePrefs caller, int fontFamily);

        int getFontFamily(long nativeDistilledPagePrefsAndroid, DistilledPagePrefs caller);

        void setTheme(long nativeDistilledPagePrefsAndroid, DistilledPagePrefs caller, int theme);

        int getTheme(long nativeDistilledPagePrefsAndroid, DistilledPagePrefs caller);

        void setFontScaling(
                long nativeDistilledPagePrefsAndroid, DistilledPagePrefs caller, float scaling);

        float getFontScaling(long nativeDistilledPagePrefsAndroid, DistilledPagePrefs caller);

        void addObserver(
                long nativeDistilledPagePrefsAndroid,
                DistilledPagePrefs caller,
                long nativeObserverPtr);

        void removeObserver(
                long nativeDistilledPagePrefsAndroid,
                DistilledPagePrefs caller,
                long nativeObserverPtr);

        long initObserverAndroid(DistilledPagePrefsObserverWrapper caller);

        void destroyObserverAndroid(long nativeDistilledPagePrefsObserverAndroid);
    }
}
