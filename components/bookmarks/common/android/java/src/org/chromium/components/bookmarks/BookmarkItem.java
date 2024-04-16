// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.bookmarks;

import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.url.GURL;

/** Contains data about a bookmark or bookmark folder. */
public class BookmarkItem {
    private final String mTitle;
    private final GURL mUrl;
    private final BookmarkId mId;
    private final boolean mIsFolder;
    private final BookmarkId mParentId;
    private final boolean mIsEditable;
    private final boolean mIsManaged;
    private final long mDateAdded;
    private final boolean mRead;
    private final long mDateLastOpened;
    private final boolean mIsAccountBookmark;

    private boolean mForceEditableForTesting;

    public BookmarkItem(
            BookmarkId id,
            String title,
            GURL url,
            boolean isFolder,
            BookmarkId parentId,
            boolean isEditable,
            boolean isManaged,
            long dateAdded,
            boolean read,
            long dateLastOpened,
            boolean isAccountBookmark) {
        mId = id;
        mTitle = title;
        mUrl = url;
        mIsFolder = isFolder;
        mParentId = parentId;
        mIsEditable = isEditable;
        mIsManaged = isManaged;
        mDateAdded = dateAdded;
        mRead = read;
        mDateLastOpened = dateLastOpened;
        mIsAccountBookmark = isAccountBookmark;
    }

    /** Returns the title of the bookmark item. */
    public String getTitle() {
        return mTitle;
    }

    /** Returns the url of the bookmark item. */
    public GURL getUrl() {
        return mUrl;
    }

    /** Returns the string to display for the item's url. */
    public String getUrlForDisplay() {
        return UrlFormatter.formatUrlForSecurityDisplay(
                getUrl(), SchemeDisplay.OMIT_HTTP_AND_HTTPS);
    }

    /** Returns whether item is a folder or a bookmark. */
    public boolean isFolder() {
        return mIsFolder;
    }

    /** Returns the parent id of the bookmark item. */
    public BookmarkId getParentId() {
        return mParentId;
    }

    /** Returns whether this bookmark can be edited. */
    public boolean isEditable() {
        return mForceEditableForTesting || mIsEditable;
    }

    /** Returns whether this bookmark's URL can be edited */
    public boolean isUrlEditable() {
        return isEditable() && mId.getType() == BookmarkType.NORMAL;
    }

    /** Returns whether this bookmark can be moved */
    public boolean isReorderable() {
        return isEditable() && mId.getType() == BookmarkType.NORMAL;
    }

    /** Returns whether this is a managed bookmark. */
    public boolean isManaged() {
        return mIsManaged;
    }

    /** Returns the {@link BookmarkId}. */
    public BookmarkId getId() {
        return mId;
    }

    /** Returns the timestamp in milliseconds since epoch that the bookmark is added. */
    public long getDateAdded() {
        return mDateAdded;
    }

    /**
     * Returns whether the bookmark is read. Only valid for {@link BookmarkType#READING_LIST}.
     * Defaults to "false" for other types.
     */
    public boolean isRead() {
        return mRead;
    }

    /**
     * Returns the timestamp in milliseconds since epoch that the bookmark was last opened. Folders
     * have a value of 0 for this, but should use the most recent child.
     */
    public long getDateLastOpened() {
        return mDateLastOpened;
    }

    /** Returns whether the bookmark is linked to your Google account. */
    public boolean isAccountBookmark() {
        return mIsAccountBookmark;
    }

    // TODO(crbug.com/40655824): Remove when BookmarkModel is stubbed in tests instead.
    public void forceEditableForTesting() {
        mForceEditableForTesting = true;
    }
}
