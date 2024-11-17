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

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * Keeps an updated values of settings interesting for the stylus functionality. All methods are
 * expected to be called on the UI thread only. If this changed in the future, we should make sure
 * that the class is thread-safe.
 */
public class StylusWritingSettingsState {
    // System setting for direct writing service. This setting is currently found under
    // Settings->Advanced features->S Pen->"S Pen to text".
    private static final String URI_DIRECT_WRITING = "direct_writing";
    private static final String URI_STYLUS_HANDWRITING = "stylus_handwriting_enabled";

    @Nullable private String mDefaultInputMethod;
    @Nullable private Integer mDirectWritingSetting;
    private int mStylusHandWritingSetting;
    private final ObserverList<StylusWritingController> mObservers = new ObserverList<>();

    private static final StylusWritingSettingsState sInstance = new StylusWritingSettingsState();

    private StylusWritingSettingsState() {
        ThreadUtils.assertOnUiThread();
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
    private void update() {
        ContentResolver contentResolver = ContextUtils.getApplicationContext().getContentResolver();
        mDefaultInputMethod =
                Settings.Secure.getString(contentResolver, Settings.Secure.DEFAULT_INPUT_METHOD);
        try {
            mDirectWritingSetting = Settings.System.getInt(contentResolver, URI_DIRECT_WRITING);
        } catch (SecurityException | SettingNotFoundException e) {
            // On some devices, URI_DIRECT_WRITING is not readable and trying to do so will
            // throw a security exception. https://crbug.com/1356155.
            mDirectWritingSetting = null;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            mStylusHandWritingSetting =
                    Settings.Secure.getInt(contentResolver, URI_STYLUS_HANDWRITING, 1);
        } else {
            mStylusHandWritingSetting =
                    Settings.Global.getInt(contentResolver, URI_STYLUS_HANDWRITING, -1);
        }
    }

    private void notifyObservers() {
        for (StylusWritingController controller : mObservers) {
            controller.onSettingsChange();
        }
    }

    public static StylusWritingSettingsState getInstance() {
        return sInstance;
    }

    @Nullable
    public String getDefaultInputMethod() {
        ThreadUtils.assertOnUiThread();
        return mDefaultInputMethod;
    }

    @Nullable
    public Integer getDirectWritingSetting() {
        ThreadUtils.assertOnUiThread();
        return mDirectWritingSetting;
    }

    public int getStylusHandWritingSetting() {
        ThreadUtils.assertOnUiThread();
        return mStylusHandWritingSetting;
    }

    public boolean registerObserver(StylusWritingController controller) {
        ThreadUtils.assertOnUiThread();
        return mObservers.addObserver(controller);
    }

    public boolean unregisterObserver(StylusWritingController controller) {
        ThreadUtils.assertOnUiThread();
        return mObservers.removeObserver(controller);
    }
}
