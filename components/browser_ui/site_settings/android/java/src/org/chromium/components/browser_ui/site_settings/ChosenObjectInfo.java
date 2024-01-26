// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import androidx.annotation.VisibleForTesting;

import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.io.Serializable;

/**
 * Information about an object (such as a USB device) the user has granted permission for an origin
 * to access.
 */
public class ChosenObjectInfo implements Serializable {
    private final @ContentSettingsType.EnumType int mContentSettingsType;
    private final String mOrigin;
    private final String mName;
    private final String mObject;
    private final boolean mIsManaged;

    @VisibleForTesting
    public ChosenObjectInfo(
            @ContentSettingsType.EnumType int contentSettingsType,
            String origin,
            String name,
            String object,
            boolean isManaged) {
        mContentSettingsType = contentSettingsType;
        mOrigin = origin;
        mName = name;
        mObject = object;
        mIsManaged = isManaged;
    }

    /** Returns the content settings type of the permission. */
    public @ContentSettingsType.EnumType int getContentSettingsType() {
        return mContentSettingsType;
    }

    /** Returns the origin that requested the permission. */
    public String getOrigin() {
        return mOrigin;
    }

    /** Returns the human readable name for the object to display in the UI. */
    public String getName() {
        return mName;
    }

    /** Returns the opaque object string that represents the object. */
    public String getObject() {
        return mObject;
    }

    /** Returns whether the object is managed by policy. */
    public boolean isManaged() {
        return mIsManaged;
    }

    /** Revokes permission for the origin to access the object if the object is not managed. */
    public void revoke(BrowserContextHandle browserContextHandle) {
        if (!mIsManaged) {
            WebsitePreferenceBridgeJni.get()
                    .revokeObjectPermission(
                            browserContextHandle, mContentSettingsType, mOrigin, mObject);
        }
    }
}
