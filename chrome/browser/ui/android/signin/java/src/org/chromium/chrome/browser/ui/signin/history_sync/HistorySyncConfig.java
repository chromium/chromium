// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.StringRes;

import org.chromium.chrome.browser.ui.signin.R;

/* Class containing IDs of resources for the history sync opt-in view. */
public final class HistorySyncConfig implements Parcelable {
    public final @StringRes int titleId;
    public final @StringRes int subtitleId;

    public static final Parcelable.Creator<HistorySyncConfig> CREATOR =
            new Parcelable.Creator<HistorySyncConfig>() {
                @Override
                public HistorySyncConfig createFromParcel(Parcel in) {
                    return new HistorySyncConfig(in);
                }

                @Override
                public HistorySyncConfig[] newArray(int size) {
                    return new HistorySyncConfig[size];
                }
            };

    public HistorySyncConfig() {
        this(
                /* titleId= */ R.string.history_sync_title,
                /* subtitleId= */ R.string.history_sync_subtitle);
    }

    public HistorySyncConfig(@StringRes int titleId, @StringRes int subtitleId) {
        this.titleId = titleId;
        this.subtitleId = subtitleId;
    }

    private HistorySyncConfig(Parcel in) {
        this(/* titleId= */ in.readInt(), /* subtitleId= */ in.readInt());
    }

    /** Implements {@link Parcelable} */
    @Override
    public int describeContents() {
        return 0;
    }

    /** Implements {@link Parcelable} */
    @Override
    public void writeToParcel(Parcel out, int flags) {
        out.writeInt(titleId);
        out.writeInt(subtitleId);
    }
}
