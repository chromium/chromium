// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import android.text.TextUtils;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

/** Class containing strings for the history sync opt-in view. */
@NullMarked
public final class HistorySyncConfig {

    /** The visibility rule to apply to the history opt-in step in the sign-in flow. */
    @IntDef({OptInMode.NONE, OptInMode.OPTIONAL, OptInMode.REQUIRED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface OptInMode {
        /** Never show the history sync opt-in. */
        int NONE = 0;

        /** The history sync opt-in can be skipped (e.g. if the user declined too recently). */
        int OPTIONAL = 1;

        /** The history sync opt-in should always be shown. */
        int REQUIRED = 2;
    }

    public final String title;
    public final String subtitle;

    public HistorySyncConfig(String title, String subtitle) {
        assert !TextUtils.isEmpty(title);
        assert !TextUtils.isEmpty(subtitle);
        this.title = title;
        this.subtitle = subtitle;
    }

    @Override
    public boolean equals(@Nullable Object object) {
        if (!(object instanceof HistorySyncConfig)) {
            return false;
        }
        HistorySyncConfig other = (HistorySyncConfig) object;
        return Objects.equals(title, other.title) && Objects.equals(subtitle, other.subtitle);
    }

    @Override
    public int hashCode() {
        return Objects.hash(title, subtitle);
    }
}
