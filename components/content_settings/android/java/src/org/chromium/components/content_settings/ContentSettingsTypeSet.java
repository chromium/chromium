// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_settings;

/** Java counter part of content_settings::ContentSettingsTypeSet. */
public final class ContentSettingsTypeSet {
    private final @ContentSettingsType.EnumType int mType;

    public ContentSettingsTypeSet(@ContentSettingsType.EnumType int type) {
        mType = type;
    }

    /** Returns whether type is in this set. */
    public boolean contains(@ContentSettingsType.EnumType int type) {
        return containsAllTypes() || mType == type;
    }

    /** If this set contains all content settings type. */
    public boolean containsAllTypes() {
        return mType == ContentSettingsType.DEFAULT;
    }

    /**
     * Get the content settings type held in this set. Called only when {@link #containsAllTypes} is
     * false.
     */
    public @ContentSettingsType.EnumType int getType() {
        assert !containsAllTypes();
        return mType;
    }

    @Override
    public boolean equals(Object obj) {
        return (obj instanceof ContentSettingsTypeSet)
                && mType == ((ContentSettingsTypeSet) obj).mType;
    }
}
