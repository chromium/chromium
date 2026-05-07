// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.toolbar.AdminPolicy;
import org.chromium.chrome.browser.ui.toolbar.SiteAccess;

import java.util.Objects;

/**
 * Represents the state of an extension action for a particular tab.
 *
 * <p>This object is returned by {@link ExtensionActionBridge}.
 */
@NullMarked
@JNINamespace("extensions")
public class ExtensionAction {
    private final String mId;
    private final String mName;
    private final String mTitle;
    private final String mAccessibleName;
    private final HoverCardState mHoverCardState;

    public static class HoverCardState {
        private final @SiteAccess int mSiteAccess;
        private final @Nullable String mSiteAccessTitle;
        private final @Nullable String mSiteAccessDescription;

        private final @AdminPolicy int mPolicy;
        private final @Nullable String mPolicyText;

        @CalledByNative
        public HoverCardState(
                @SiteAccess int siteAccess,
                @JniType("std::optional<std::string>") @Nullable String siteAccessTitle,
                @JniType("std::optional<std::string>") @Nullable String siteAccessDescription,
                @AdminPolicy int policy,
                @JniType("std::optional<std::string>") @Nullable String policyText) {
            mSiteAccess = siteAccess;
            mSiteAccessTitle = siteAccessTitle;
            mSiteAccessDescription = siteAccessDescription;
            mPolicy = policy;
            mPolicyText = policyText;
        }

        public @SiteAccess int getSiteAccess() {
            return mSiteAccess;
        }

        public @Nullable String getSiteAccessTitle() {
            return mSiteAccessTitle;
        }

        public @Nullable String getSiteAccessDescription() {
            return mSiteAccessDescription;
        }

        public @AdminPolicy int getAdminPolicy() {
            return mPolicy;
        }

        public @Nullable String getPolicyText() {
            return mPolicyText;
        }
    }

    @CalledByNative
    @VisibleForTesting
    public ExtensionAction(
            @JniType("std::string") String id,
            @JniType("std::string") String name,
            @JniType("std::string") String title,
            @JniType("std::string") String accessibleName,
            HoverCardState hoverCardState) {
        mId = id;
        mName = name;
        mTitle = title;
        mAccessibleName = accessibleName;
        mHoverCardState = hoverCardState;
    }

    public String getName() {
        return mName;
    }

    public String getTitle() {
        return mTitle;
    }

    public String getId() {
        return mId;
    }

    public String getAccessibleName() {
        return mAccessibleName;
    }

    public HoverCardState getHoverCardState() {
        return mHoverCardState;
    }

    @Override
    public boolean equals(Object o) {
        if (o instanceof ExtensionAction other) {
            return mId.equals(other.mId)
                    && mName.equals(other.mName)
                    && mAccessibleName.equals(other.mAccessibleName);
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mId, mName, mAccessibleName);
    }
}
