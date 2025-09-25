// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.dom_distiller.core;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.dom_distiller.mojom.FontFamily;
import org.chromium.dom_distiller.mojom.Theme;

import java.util.HashMap;
import java.util.Map;

/** Wrapper for the dom_distiller::DistilledPagePrefs. */
@JNINamespace("dom_distiller::android")
@NullMarked
public class DistilledPagePrefs {

    private final long mDistilledPagePrefsAndroid;
    private final Map<Observer, DistilledPagePrefsObserverWrapper> mObserverMap;

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
        mDistilledPagePrefsAndroid = DistilledPagePrefsJni.get().init(this, distilledPagePrefsPtr);
        mObserverMap = new HashMap<Observer, DistilledPagePrefsObserverWrapper>();
    }

    /**
     * Adds the observer to listen to changes in DistilledPagePrefs.
     *
     * @return whether the observerMap was changed as a result of the call.
     */
    public boolean addObserver(Observer obs) {
        if (mObserverMap.containsKey(obs)) return false;
        DistilledPagePrefsObserverWrapper wrappedObserver =
                new DistilledPagePrefsObserverWrapper(obs);
        DistilledPagePrefsJni.get()
                .addObserver(mDistilledPagePrefsAndroid, wrappedObserver.getNativePtr());

        mObserverMap.put(obs, wrappedObserver);
        return true;
    }

    /**
     * Removes the observer and unregisters it from DistilledPagePrefs changes.
     *
     * @return whether the observer was removed as a result of the call.
     */
    public boolean removeObserver(Observer obs) {
        DistilledPagePrefsObserverWrapper wrappedObserver = mObserverMap.remove(obs);
        if (wrappedObserver == null) return false;
        DistilledPagePrefsJni.get()
                .removeObserver(mDistilledPagePrefsAndroid, wrappedObserver.getNativePtr());

        wrappedObserver.destroy();
        return true;
    }

    public void setFontFamily(int fontFamily) {
        FontFamily.validate(fontFamily);
        DistilledPagePrefsJni.get().setFontFamily(mDistilledPagePrefsAndroid, fontFamily);
    }

    public int getFontFamily() {
        return DistilledPagePrefsJni.get().getFontFamily(mDistilledPagePrefsAndroid);
    }

    public void setUserPrefTheme(int theme) {
        Theme.validate(theme);
        DistilledPagePrefsJni.get().setUserPrefTheme(mDistilledPagePrefsAndroid, theme);
    }

    public void setDefaultTheme(int theme) {
        DistilledPagePrefsJni.get().setDefaultTheme(mDistilledPagePrefsAndroid, theme);
    }

    public int getTheme() {
        return DistilledPagePrefsJni.get().getTheme(mDistilledPagePrefsAndroid);
    }

    public void setFontScaling(float scaling) {
        DistilledPagePrefsJni.get().setUserPrefFontScaling(mDistilledPagePrefsAndroid, scaling);
    }

    public float getFontScaling() {
        return DistilledPagePrefsJni.get().getFontScaling(mDistilledPagePrefsAndroid);
    }

    @NativeMethods
    interface Natives {
        long init(DistilledPagePrefs self, long distilledPagePrefPtr);

        void setFontFamily(long nativeDistilledPagePrefsAndroid, int fontFamily);

        int getFontFamily(long nativeDistilledPagePrefsAndroid);

        void setUserPrefTheme(long nativeDistilledPagePrefsAndroid, int theme);

        void setDefaultTheme(long nativeDistilledPagePrefsAndroid, int theme);

        int getTheme(long nativeDistilledPagePrefsAndroid);

        void setUserPrefFontScaling(long nativeDistilledPagePrefsAndroid, float scaling);

        float getFontScaling(long nativeDistilledPagePrefsAndroid);

        void addObserver(long nativeDistilledPagePrefsAndroid, long nativeObserverPtr);

        void removeObserver(long nativeDistilledPagePrefsAndroid, long nativeObserverPtr);

        long initObserverAndroid(DistilledPagePrefsObserverWrapper self);

        void destroyObserverAndroid(long nativeDistilledPagePrefsObserverAndroid);
    }
}
