// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.util;

import android.os.Build;
import android.text.ShowSecretsSetting;

import androidx.annotation.RequiresApi;

import org.chromium.base.ContextUtils;
import org.chromium.base.PasswordEchoSettingDelegate;
import org.chromium.build.annotations.NullMarked;

/** Implementation of {@link PasswordEchoSettingDelegate} using public Android APIs. */
@RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
@NullMarked
public class PasswordEchoSettingDelegateImpl implements PasswordEchoSettingDelegate {
    @Override
    public void registerCallback(Runnable callback) {
        ShowSecretsSetting.registerCallback(ContextUtils.getApplicationContext(), callback);
    }

    @Override
    public boolean isPhysicalSettingEnabled() {
        return ShowSecretsSetting.shouldShowPhysicalInput(ContextUtils.getApplicationContext());
    }

    @Override
    public boolean isTouchSettingEnabled() {
        return ShowSecretsSetting.shouldShowTouchInput(ContextUtils.getApplicationContext());
    }
}
