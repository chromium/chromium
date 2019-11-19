// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.viz.common.display;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Point;
import android.hardware.display.DisplayManager;
import android.provider.Settings.Global;
import android.view.Display;
import android.view.Surface;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;

import java.lang.reflect.Field;

/**
 * Provides static utility functions to query de-jelly settings.
 */
@JNINamespace("viz")
@MainDex
public class DeJellyUtils implements DisplayManager.DisplayListener, ComponentCallbacks {
    /** Singleton DeJellyUtils instance */
    private static DeJellyUtils sInstance;

    /**
     * @return The DeJellyUtils singleton.
     */
    private static DeJellyUtils getInstance() {
        if (sInstance == null) sInstance = new DeJellyUtils();
        return sInstance;
    }

    /** Whether the last processed rotation should have de-jelly applied. */
    private boolean mRotationOk;
    /** Whether the last processed configuration should have de-jelly applied. */
    private boolean mConfigurationOk;
    /** The last processed screen width. */
    private float mScreenWidth;
    /** Cached DisplayManager. */
    private DisplayManager mManager;
    /** Field used to identify in-use screen on some devices. */
    private Field mDisplayDeviceType;

    /** Int representing the sub-display, used with mDisplayDeviceType. */
    private static final int SEM_DISPLAY_DEVICE_TYPE_SUB = 5;

    private DeJellyUtils() {
        // TODO(ericrk): We should probably plumb display information from the
        // browser process, rather than re-querying it here.
        mManager = (DisplayManager) ContextUtils.getApplicationContext().getSystemService(
                Context.DISPLAY_SERVICE);
        // Register callbacks which will notify of future changes.
        mManager.registerDisplayListener(this, null);
        ContextUtils.getApplicationContext().registerComponentCallbacks(this);

        // Get current state, as we may need these values and callbacks will not yet have fired.
        onDisplayChanged(0);
        onConfigurationChanged(
                ContextUtils.getApplicationContext().getResources().getConfiguration());

        // See if we have a display device type field to query.
        try {
            Class configurationClass = Configuration.class;
            mDisplayDeviceType = configurationClass.getDeclaredField("semDisplayDeviceType");
        } catch (Exception e) {
            // We do not require mDisplayDeviceType to exist. Continue.
        }
    }

    // DisplayManager.DisplayListener implementation.
    @Override
    public void onDisplayAdded(int displayId) {}
    @Override
    public void onDisplayChanged(int displayId) {
        // Ignore non-default display.
        if (displayId != Display.DEFAULT_DISPLAY) return;

        Display display = mManager.getDisplay(displayId);

        // For now only support ROTATION_0.
        // TODO(ericrk): Allow this to be customized.
        int rotation = display.getRotation();
        mRotationOk = rotation == Surface.ROTATION_0;

        Point realSize = new Point();
        display.getRealSize(realSize);
        mScreenWidth = realSize.x;
    }
    @Override
    public void onDisplayRemoved(int displayId) {}

    // ComponentCallbacks implementation.
    @Override
    public void onConfigurationChanged(Configuration configuration) {
        if (mDisplayDeviceType != null) {
            try {
                mConfigurationOk =
                        mDisplayDeviceType.getInt(configuration) != SEM_DISPLAY_DEVICE_TYPE_SUB;
                return;
            } catch (Exception e) {
                // We don't require that this field exists, continue.
            }
        }
        // Configuration is always ok if we can't query mDisplayDeviceType.
        mConfigurationOk = true;
    }
    @Override
    public void onLowMemory() {}

    /**
     * @return Whether de-jelly is externally enabled. This indicates whether
     *         de-jelly may be used on a device in certain orientations /
     *         configurations, not whether it is actively being used.
     */
    public static boolean externallyEnableDeJelly() {
        return Global.getInt(ContextUtils.getApplicationContext().getContentResolver(),
                       "enable_de_jelly_for_chrome", 0)
                != 0;
    }

    /**
     * @return Whether the current screen / configuration should use the de-jelly effect.
     */
    @CalledByNative
    private static boolean useDeJelly() {
        DeJellyUtils utils = getInstance();

        // Device specific flag which force-disables jelly scroll on a per-frame basis.
        if (Global.getInt(ContextUtils.getApplicationContext().getContentResolver(),
                    "sem_support_scroll_filter", 1)
                == 0) {
            return false;
        }

        return utils.mRotationOk && utils.mConfigurationOk;
    }

    /**
     * @return the current screen width.
     */
    @CalledByNative
    private static float screenWidth() {
        return getInstance().mScreenWidth;
    }
};
