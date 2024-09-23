// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps;

import android.graphics.Bitmap;
import android.util.Pair;

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The mediator class in the MVC architecture of the add-to-homescreen component. The C++
 * counterpart of this class calls various Java set methods ({@link #setIcon}, {@link
 * #setWebAppInfo}, {@link #setWebAppInfoWithIcon}, and {@link #setNativeAppInfo}) when more
 * information about the app is available. These methods modify the model that lives on the Java
 * side.
 */
@JNINamespace("webapps")
class AddToHomescreenMediator implements AddToHomescreenViewDelegate {
    private long mNativeAddToHomescreenMediator;
    private PropertyModel mModel;
    private WindowAndroid mWindowAndroid;
    private AppData mNativeAppData;

    AddToHomescreenMediator(PropertyModel model, WindowAndroid windowAndroid) {
        mModel = model;
        mWindowAndroid = windowAndroid;
        mNativeAddToHomescreenMediator = AddToHomescreenMediatorJni.get().initialize(this);
    }

    void startForAppMenu(@NonNull WebContents webContents, int menuItemType) {
        if (mNativeAddToHomescreenMediator == 0) return;

        AddToHomescreenMediatorJni.get()
                .startForAppMenu(mNativeAddToHomescreenMediator, webContents, menuItemType);
    }

    @CalledByNative
    void setIcon(Bitmap icon, boolean isAdaptive) {
        Bitmap iconToShow = icon;
        if (isAdaptive) {
            iconToShow =
                    WebappsIconUtils.createHomeScreenIconFromWebIcon(icon, /* maskable= */ true);
        }

        mModel.set(AddToHomescreenProperties.ICON, new Pair<>(iconToShow, isAdaptive));
        mModel.set(AddToHomescreenProperties.CAN_SUBMIT, true);
    }

    @CalledByNative
    void setWebAppInfo(String title, String url, @AppType int appType) {
        mModel.set(AddToHomescreenProperties.TITLE, title);
        mModel.set(AddToHomescreenProperties.URL, url);
        mModel.set(AddToHomescreenProperties.TYPE, appType);
    }

    @CalledByNative
    void setNativeAppInfo(AppData nativeAppData) {
        mNativeAppData = nativeAppData;
        mModel.set(AddToHomescreenProperties.TITLE, nativeAppData.title());
        mModel.set(AddToHomescreenProperties.TYPE, AppType.NATIVE);
        mModel.set(AddToHomescreenProperties.NATIVE_APP_RATING, nativeAppData.rating());
        mModel.set(AddToHomescreenProperties.CAN_SUBMIT, true);
        mModel.set(
                AddToHomescreenProperties.NATIVE_INSTALL_BUTTON_TEXT,
                nativeAppData.installButtonText());
    }

    long getNativeMediator() {
        return mNativeAddToHomescreenMediator;
    }

    @Override
    public void onAddToHomescreen(String title, @AppType int selectedType) {
        if (mNativeAddToHomescreenMediator == 0) return;

        AddToHomescreenMediatorJni.get()
                .addToHomescreen(mNativeAddToHomescreenMediator, title, selectedType);
        destroyNative();
    }

    @Override
    public boolean onAppDetailsRequested() {
        if (mModel.get(AddToHomescreenProperties.TYPE) != AppType.NATIVE) {
            return false;
        }

        mWindowAndroid.showIntent(mNativeAppData.detailsIntent(), null, null);

        if (mNativeAddToHomescreenMediator != 0) {
            AddToHomescreenMediatorJni.get().onNativeDetailsShown(mNativeAddToHomescreenMediator);
        }
        return true;
    }

    @Override
    public void onViewDismissed() {
        if (mNativeAddToHomescreenMediator == 0) return;

        AddToHomescreenMediatorJni.get().onUiDismissed(mNativeAddToHomescreenMediator);
        destroyNative();
    }

    private void destroyNative() {
        if (mNativeAddToHomescreenMediator == 0) return;

        AddToHomescreenMediatorJni.get().destroy(mNativeAddToHomescreenMediator);
        mNativeAddToHomescreenMediator = 0;
    }

    @NativeMethods
    interface Natives {
        long initialize(AddToHomescreenMediator instance);

        void startForAppMenu(
                long nativeAddToHomescreenMediator, WebContents webContents, int menuItemType);

        void addToHomescreen(
                long nativeAddToHomescreenMediator, String title, @AppType int appType);

        void onNativeDetailsShown(long nativeAddToHomescreenMediator);

        void onUiDismissed(long nativeAddToHomescreenMediator);

        void destroy(long nativeAddToHomescreenMediator);
    }
}
