// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Placeholder provider class to query whether the operating system has granted various security
 * permissions.
 */
@NullMarked
public abstract class OsAdditionalSecurityPermissionProvider {
    public interface Observer {
        /** Called when the Android-OS advanced-protection-mode setting changes. */
        void onAdvancedProtectionOsSettingChanged();
    }

    public void addObserver(Observer observer) {}

    public void removeObserver(Observer observer) {}

    /**
     * Returns whether the Android OS requests advanced-protection-mode. Implementations must allow
     * querying from any thread.
     */
    public boolean isAdvancedProtectionRequestedByOs() {
        return false;
    }

    /**
     * Returns message to display in site settings explaining why the operating system has denied
     * the javascript-optimizer permission.
     */
    public String getJavascriptOptimizerMessage(Context context) {
        return "";
    }

    /**
     * Returns {@link PropertyModel} for message-UI to notify user about new
     * advanced-protection-mode state.
     *
     * @param primaryButtonAction The action to run when the message-UI primary-button is clicked.
     */
    public @Nullable PropertyModel buildAdvancedProtectionMessagePropertyModel(
            Context context, @Nullable Runnable primaryButtonAction) {
        return null;
    }

    /**
     * Returns resource id for icon to use for operating-system-provided advanced protection
     * branding.
     */
    public int getAdvancedProtectionIconResourceId() {
        return 0;
    }

    /**
     * Returns non-black-and-white icon to use for operating-system-provided advanced protection
     * branding.
     */
    @Nullable
    public Drawable getColorfulAdvancedProtectionIcon(Context context) {
        return null;
    }

    /**
     * Returns resource id for string to display to inform that strict-https-first-mode is enforced
     * by the operating-system-provided advanced protection setting.
     */
    public int getHttpsFirstModeEnforcedByAndroidAdvancedProtectionWarningResourceId() {
        return 0;
    }

    /** Returns intent which launches OS-advanced-protection settings. */
    @Nullable
    public Intent getIntentForOsAdvancedProtectionSettings() {
        return null;
    }
}
