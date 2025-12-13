// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import android.content.ContentResolver;
import android.database.ContentObserver;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import android.provider.Settings.SettingNotFoundException;

import androidx.annotation.AnyThread;
import androidx.annotation.MainThread;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Keeps an updated values of settings interesting for the stylus functionality.
 *
 * <p>Settings can be (lazily) initialized and queried on any thread. However, observers are run on
 * the UI thread and must also be registered/unregistered on the UI thread.
 *
 * <p>Note that settings may change in-between getter calls from a non-UI thread.
 */
@NullMarked
public class StylusWritingSettingsState {
    // System setting for direct writing service. This setting is currently found under
    // Settings->Advanced features->S Pen->"S Pen to text".
    private static final String URI_DIRECT_WRITING = "direct_writing";
    private static final String URI_STYLUS_HANDWRITING = "stylus_handwriting_enabled";

    private final AtomicReference<@Nullable String> mDefaultInputMethod = new AtomicReference<>();
    private final AtomicReference<@Nullable Integer> mDirectWritingSetting =
            new AtomicReference<>();
    private final AtomicInteger mStylusHandWritingSetting = new AtomicInteger(0);

    // Lazily construct on the UI thread in getObserverList(), as the state object may be created on
    // a background thread. Must only be accessed from the UI thread.
    @Nullable private ObserverList<StylusWritingController> mObservers;

    private static class LazyHolder {
        static final StylusWritingSettingsState INSTANCE = new StylusWritingSettingsState();
    }

    @AnyThread
    private StylusWritingSettingsState() {
        ContentResolver contentResolver = ContextUtils.getApplicationContext().getContentResolver();
        ContentObserver stateObserver =
                new ContentObserver(ThreadUtils.getUiThreadHandler()) {
                    @Override
                    public boolean deliverSelfNotifications() {
                        // We want onChange(..) to be called even if the settings change was
                        // triggered from this application (or process).
                        return true;
                    }

                    @Override
                    public void onChange(boolean selfChange) {
                        update();
                        notifyObservers();
                    }
                };
        List<Uri> urisToObserve = new ArrayList<>();
        urisToObserve.add(Settings.Secure.getUriFor(Settings.Secure.DEFAULT_INPUT_METHOD));
        try {
            urisToObserve.add(Settings.System.getUriFor(URI_DIRECT_WRITING));
        } catch (SecurityException e) {
            // On some devices, URI_DIRECT_WRITING is not readable and trying to do so will
            // throw a security exception. https://crbug.com/1356155.
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            urisToObserve.add(Settings.Secure.getUriFor(URI_STYLUS_HANDWRITING));
        } else {
            urisToObserve.add(Settings.Global.getUriFor(URI_STYLUS_HANDWRITING));
        }

        for (Uri uri : urisToObserve) {
            if (uri != null) {
                contentResolver.registerContentObserver(uri, false, stateObserver);
            }
        }
        update();
    }

    // Updates the settings values for stylus
    @AnyThread
    private void update() {
        ContentResolver contentResolver = ContextUtils.getApplicationContext().getContentResolver();
        mDefaultInputMethod.set(
                Settings.Secure.getString(contentResolver, Settings.Secure.DEFAULT_INPUT_METHOD));
        try {
            mDirectWritingSetting.set(Settings.System.getInt(contentResolver, URI_DIRECT_WRITING));
        } catch (SecurityException | SettingNotFoundException e) {
            // On some devices, URI_DIRECT_WRITING is not readable and trying to do so will
            // throw a security exception. https://crbug.com/1356155.
            mDirectWritingSetting.set(null);
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            mStylusHandWritingSetting.set(
                    Settings.Secure.getInt(contentResolver, URI_STYLUS_HANDWRITING, 1));
        } else {
            mStylusHandWritingSetting.set(
                    Settings.Global.getInt(contentResolver, URI_STYLUS_HANDWRITING, -1));
        }
    }

    @MainThread
    private ObserverList<StylusWritingController> getObserverList() {
        if (mObservers == null) {
            mObservers = new ObserverList();
        }
        return mObservers;
    }

    @MainThread
    private void notifyObservers() {
        for (StylusWritingController controller : getObserverList()) {
            controller.onSettingsChange();
        }
    }

    @AnyThread
    public static StylusWritingSettingsState getInstance() {
        return LazyHolder.INSTANCE;
    }

    @AnyThread
    public @Nullable String getDefaultInputMethod() {
        return mDefaultInputMethod.get();
    }

    @AnyThread
    public @Nullable Integer getDirectWritingSetting() {
        return mDirectWritingSetting.get();
    }

    @AnyThread
    public int getStylusHandWritingSetting() {
        return mStylusHandWritingSetting.get();
    }

    @MainThread
    public boolean registerObserver(StylusWritingController controller) {
        ThreadUtils.assertOnUiThread();
        return getObserverList().addObserver(controller);
    }

    @MainThread
    public boolean unregisterObserver(StylusWritingController controller) {
        ThreadUtils.assertOnUiThread();
        return getObserverList().removeObserver(controller);
    }
}
