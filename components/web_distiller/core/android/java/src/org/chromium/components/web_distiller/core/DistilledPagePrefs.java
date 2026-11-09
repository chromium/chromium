// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.web_distiller.core;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.web_distiller.mojom.FontFamily;
import org.chromium.web_distiller.mojom.Theme;

import java.util.HashMap;
import java.util.Map;

/** Wrapper for the web_distiller::DistilledPagePrefs. */
@JNINamespace("web_distiller::android")
@NullMarked
public class DistilledPagePrefs {

    private final long mDistilledPagePrefsAndroid;
    private final Map<Observer, DistilledPagePrefsObserverWrapper> mObserverMap;

    /** Observer interface for observing DistilledPagePrefs changes. */
    public interface Observer {
        void onChangeFontFamily(int font);

        void onChangeTheme(int theme);

        void onChangeFontScaling(float scaling);

        void onChangeLinksEnabled(boolean enabled);
    }

    @JniType("web_distiller::android::DistilledPagePrefsObserverAndroid")
    interface NativeDistilledPagePrefsObserverAndroid extends JniTypeToken {}

    /** Wrapper for web_distiller::android::DistilledPagePrefsObserverAndroid. */
    static class DistilledPagePrefsObserverWrapper {
        private final Observer mDistilledPagePrefsObserver;
        private @Nullable JniUniquePtr<NativeDistilledPagePrefsObserverAndroid>
                mNativeDistilledPagePrefsObserverAndroidPtr;

        public DistilledPagePrefsObserverWrapper(Observer observer) {
            mNativeDistilledPagePrefsObserverAndroidPtr =
                    DistilledPagePrefsJni.get().initObserverAndroid(this);
            mDistilledPagePrefsObserver = observer;
        }

        @CalledByNative
        private void onChangeFontFamily(int fontFamily) {
            FontFamily.validate(fontFamily);
            mDistilledPagePrefsObserver.onChangeFontFamily(fontFamily);
        }

        @CalledByNative
        private void onChangeTheme(int theme) {
            Theme.validate(theme);
            mDistilledPagePrefsObserver.onChangeTheme(theme);
        }

        @CalledByNative
        private void onChangeFontScaling(float scaling) {
            mDistilledPagePrefsObserver.onChangeFontScaling(scaling);
        }

        @CalledByNative
        private void onChangeLinksEnabled(boolean enabled) {
            mDistilledPagePrefsObserver.onChangeLinksEnabled(enabled);
        }

        public void destroy() {
            if (mNativeDistilledPagePrefsObserverAndroidPtr != null) {
                mNativeDistilledPagePrefsObserverAndroidPtr.destroy();
                mNativeDistilledPagePrefsObserverAndroidPtr = null;
            }
        }

        public JniPtr<NativeDistilledPagePrefsObserverAndroid> getNativePtr() {
            assert mNativeDistilledPagePrefsObserverAndroidPtr != null;
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

    public void setLinksEnabled(boolean enabled) {
        DistilledPagePrefsJni.get().setLinksEnabled(mDistilledPagePrefsAndroid, enabled);
    }

    public boolean getLinksEnabled() {
        return DistilledPagePrefsJni.get().getLinksEnabled(mDistilledPagePrefsAndroid);
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

        void setLinksEnabled(long nativeDistilledPagePrefsAndroid, boolean enabled);

        boolean getLinksEnabled(long nativeDistilledPagePrefsAndroid);

        void addObserver(
                long nativeDistilledPagePrefsAndroid,
                JniPtr<NativeDistilledPagePrefsObserverAndroid> nativeObserverPtr);

        void removeObserver(
                long nativeDistilledPagePrefsAndroid,
                JniPtr<NativeDistilledPagePrefsObserverAndroid> nativeObserverPtr);

        JniUniquePtr<NativeDistilledPagePrefsObserverAndroid> initObserverAndroid(
                DistilledPagePrefsObserverWrapper self);
    }
}
