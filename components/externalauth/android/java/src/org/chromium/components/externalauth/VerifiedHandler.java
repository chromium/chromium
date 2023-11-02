// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.externalauth;

import android.content.Context;
import android.os.Handler;
import android.os.Message;
import android.os.Messenger;
import android.text.TextUtils;

import java.util.HashMap;
import java.util.Map;

/**
 * Handler class that ignores any messages coming from external caller that doesn't meet the given
 * authentication requirements.
 */
public class VerifiedHandler extends Handler {
    private final int mAuthRequirements;
    private final String mCallerPackageToMatch;
    private final Map<Messenger, Boolean> mClientTrustMap = new HashMap<Messenger, Boolean>();
    private final Context mContext;
    private final ExternalAuthUtils mExternalAuthUtils;

    /**
     * Basic constructor for verified handler.
     * @param context The context to use for accessing the package manager.
     * @param externalAuthUtils An {@link ExternalAuthUtils}, to check the package. Can be acquired
     *                          from AppHooks.
     * @param authRequirements The requirements for authenticating the caller application.
     */
    public VerifiedHandler(
            Context context, ExternalAuthUtils externalAuthUtils, int authRequirements) {
        this(context, externalAuthUtils, authRequirements, "");
    }

    /**
     * Constructor with package name requirement.
     * @param context The context to use for accessing the package manager.
     * @param externalAuthUtils An {@link ExternalAuthUtils}, to check the package. Can be acquired
     *                          from AppHooks.
     * @param authRequirements The requirements for authenticating the caller application.
     * @param callerPackageToMatch The package name to match to.
     */
    public VerifiedHandler(Context context, ExternalAuthUtils externalAuthUtils,
            int authRequirements, String callerPackageToMatch) {
        mContext = context;
        mExternalAuthUtils = externalAuthUtils;
        mAuthRequirements = authRequirements;
        mCallerPackageToMatch = callerPackageToMatch;
    }

    @Override
    public boolean sendMessageAtTime(Message msg, long uptimeMillis) {
        Messenger client = msg.replyTo;
        if (!mClientTrustMap.containsKey(client)) mClientTrustMap.put(client, checkCallerIsValid());
        return (!mClientTrustMap.get(client)) ? false : super.sendMessageAtTime(msg, uptimeMillis);
    }

    /**
     * @return Whether the calling application is valid given the requirements
     *         set during construction.
     */
    public boolean checkCallerIsValid() {
        return TextUtils.isEmpty(mCallerPackageToMatch)
                ? mExternalAuthUtils.isCallerValid(mContext, mAuthRequirements)
                : mExternalAuthUtils.isCallerValidForPackage(
                        mContext, mAuthRequirements, mCallerPackageToMatch);
    }
}
