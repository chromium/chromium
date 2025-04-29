// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.signin.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

/* Class containing IDs of resources for the history sync opt-in view. */
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

    public final @StringRes int titleId;
    public final @StringRes int subtitleId;

    public HistorySyncConfig() {
        this(/* titleId= */ 0, /* subtitleId= */ 0);
    }

    public HistorySyncConfig(@StringRes int titleId, @StringRes int subtitleId) {
        this.titleId = titleId == 0 ? R.string.history_sync_title : titleId;
        this.subtitleId = subtitleId == 0 ? R.string.history_sync_subtitle : subtitleId;
    }

    @Override
    public boolean equals(@Nullable Object object) {
        if (!(object instanceof HistorySyncConfig)) {
            return false;
        }
        HistorySyncConfig other = (HistorySyncConfig) object;
        return titleId == other.titleId && subtitleId == other.subtitleId;
    }

    @Override
    public int hashCode() {
        return Objects.hash(titleId, subtitleId);
    }
}
