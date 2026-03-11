// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.util;

import android.database.ContentObserver;
import android.provider.Settings;

import org.chromium.base.ContextUtils;
import org.chromium.base.PasswordEchoSettingDelegate;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;

/**
 * Legacy implementation of {@link PasswordEchoSettingDelegate} that uses the {@link
 * Settings.System.TEXT_SHOW_PASSWORD} setting.
 */
@NullMarked
class LegacyPasswordEchoSettingDelegateImpl implements PasswordEchoSettingDelegate {
    private boolean mEnabled;

    LegacyPasswordEchoSettingDelegateImpl() {
        mEnabled = isLegacySettingEnabled();
    }

    @Override
    public void registerCallback(Runnable callback) {
        ContextUtils.getApplicationContext()
                .getContentResolver()
                .registerContentObserver(
                        Settings.System.getUriFor(Settings.System.TEXT_SHOW_PASSWORD),
                        false,
                        new ContentObserver(ThreadUtils.getUiThreadHandler()) {
                            @Override
                            public void onChange(boolean selfChange) {
                                mEnabled = isLegacySettingEnabled();
                                callback.run();
                            }
                        });
    }

    @Override
    public boolean isPhysicalSettingEnabled() {
        return mEnabled;
    }

    @Override
    public boolean isTouchSettingEnabled() {
        return mEnabled;
    }

    private boolean isLegacySettingEnabled() {
        // The default value for the legacy setting is 1 (momentarily show). Password echoing
        // has historically been enabled by default on Android. This default applies if the user
        // has never explicitly configured the setting.
        return Settings.System.getInt(
                        ContextUtils.getApplicationContext().getContentResolver(),
                        Settings.System.TEXT_SHOW_PASSWORD,
                        1)
                == 1;
    }
}
