// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

import androidx.core.app.PendingIntentCompat.Flags;

import org.chromium.base.IntentUtils;

/** Provides {@link PendingIntent} and the flags used to build the PendingIntent. */
public class PendingIntentProvider {
    private PendingIntent mPendingIntent;
    @Flags private final int mFlags;
    private final int mRequestCode;

    /**
     * Creates {@link PendingIntent}that triggers {@link android.content.BroadcastReceiver}.
     *
     * @see {@link PendingIntent#getBroadcast(Context, int, Intent, int)}.
     */
    public static PendingIntentProvider getBroadcast(
            Context context, int requestCode, Intent intent, int flags, boolean mutable) {
        flags = ensureCorrectFlags(flags, mutable);
        return new PendingIntentProvider(
                PendingIntent.getBroadcast(context, requestCode, intent, flags),
                flags,
                requestCode);
    }

    /** @see {@link #getBroadcast(Context, int, Intent, int, boolean)}. */
    public static PendingIntentProvider getBroadcast(
            Context context, int requestCode, Intent intent, int flags) {
        return getBroadcast(context, requestCode, intent, flags, /* mutable= */ false);
    }

    /**
     * Creates {@link PendingIntent} that triggers {@link android.app.Service}.
     *
     * @see {@link PendingIntent#getService(Context, int, Intent, int)} .
     */
    public static PendingIntentProvider getService(
            Context context, int requestCode, Intent intent, int flags, boolean mutable) {
        flags = ensureCorrectFlags(flags, mutable);
        return new PendingIntentProvider(
                PendingIntent.getService(context, requestCode, intent, flags), flags, requestCode);
    }

    /** @see {@link #getService(Context, int, Intent, int, boolean)}. */
    public static PendingIntentProvider getService(
            Context context, int requestCode, Intent intent, int flags) {
        return getService(context, requestCode, intent, flags, /* mutable= */ false);
    }

    /**
     * Creates {@link PendingIntent} that triggers {@link android.app.Activity}.
     *
     * @see {@link PendingIntent#getActivity(Context, int, Intent, int)}.
     */
    public static PendingIntentProvider getActivity(
            Context context, int requestCode, Intent intent, int flags, boolean mutable) {
        flags = ensureCorrectFlags(flags, mutable);
        return new PendingIntentProvider(
                PendingIntent.getActivity(context, requestCode, intent, flags), flags, requestCode);
    }

    /** @see {@link #getActivity(Context, int, Intent, int, boolean)}. */
    public static PendingIntentProvider getActivity(
            Context context, int requestCode, Intent intent, int flags) {
        return getActivity(context, requestCode, intent, flags, /* mutable= */ false);
    }

    /**
     * Creates a pending intent wrapper.
     * @param pendingIntent The actual {@link PendingIntent} wrapped in this class.
     * @param flags The flags for the {@link PendingIntent}.
     * @param requestCode The request code for the {@link PendingIntent}.
     */
    public PendingIntentProvider(PendingIntent pendingIntent, int flags, int requestCode) {
        mPendingIntent = pendingIntent;
        mFlags = flags;
        mRequestCode = requestCode;
    }

    /** Returns the {@link PendingIntent}. */
    public PendingIntent getPendingIntent() {
        return mPendingIntent;
    }

    /** Returns the flags of {@link PendingIntent}. */
    public @Flags int getFlags() {
        return mFlags;
    }

    /** Returns the request code for the {@link PendingIntent}. */
    public int getRequestCode() {
        return mRequestCode;
    }

    private static int ensureCorrectFlags(int flags, boolean mutable) {
        flags |= IntentUtils.getPendingIntentMutabilityFlag(mutable);
        return flags;
    }
}
