// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.bookmarks;

import android.text.TextUtils;
import android.util.Log;

import org.jni_zero.CalledByNative;

/** Simple object representing the bookmark id. */
public class BookmarkId {
    public static final int INVALID_FOLDER_ID = -2;
    public static final int INVALID_ID = -1;

    private static final String LOG_TAG = "BookmarkId";
    private static final char TYPE_PARTNER = 'p';
    private static final char TYPE_READING_LIST = 'r';
    private static final int ROOT_FOLDER_ID = -1;

    private final long mId;
    private final @BookmarkType int mType;

    public BookmarkId(long id, @BookmarkType int type) {
        assert BookmarkType.NORMAL <= type && type <= BookmarkType.LAST;
        mId = id;
        mType = type;
    }

    /**
     * @param c The char representing the type.
     * @return The Bookmark type from a char representing the type.
     */
    private static @BookmarkType int getBookmarkTypeFromChar(char c) {
        switch (c) {
            case TYPE_PARTNER:
                return BookmarkType.PARTNER;
            case TYPE_READING_LIST:
                return BookmarkType.READING_LIST;
            default:
                return BookmarkType.NORMAL;
        }
    }

    /**
     * @param c The char representing the bookmark type.
     * @return Whether the char representing the bookmark type is a valid type.
     */
    private static boolean isValidBookmarkTypeFromChar(char c) {
        return c == TYPE_PARTNER || c == TYPE_READING_LIST;
    }

    /**
     * @param s The bookmark id string (Eg: p1 for partner bookmark id 1).
     * @return the Bookmark id from the string which is a concatenation of
     *         bookmark type and the bookmark id.
     */
    public static BookmarkId getBookmarkIdFromString(String s) {
        long id = ROOT_FOLDER_ID;
        @BookmarkType int type = BookmarkType.NORMAL;
        if (TextUtils.isEmpty(s)) return new BookmarkId(id, type);
        char folderTypeChar = s.charAt(0);
        if (isValidBookmarkTypeFromChar(folderTypeChar)) {
            type = getBookmarkTypeFromChar(folderTypeChar);
            s = s.substring(1);
        }
        try {
            id = Long.parseLong(s);
        } catch (NumberFormatException exception) {
            Log.e(LOG_TAG, "Error parsing url to extract the bookmark folder id.", exception);
        }
        return new BookmarkId(id, type);
    }

    /** @return The id of the bookmark. */
    @CalledByNative
    public long getId() {
        return mId;
    }

    /** Returns the bookmark type: {@link BookmarkType#NORMAL} or {@link BookmarkType#PARTNER}. */
    @CalledByNative
    public @BookmarkType int getType() {
        return mType;
    }

    /**
     * @param id The id of the bookmark.
     * @param type The bookmark type.
     * @return The BookmarkId Object.
     */
    @CalledByNative
    private static BookmarkId createBookmarkId(long id, @BookmarkType int type) {
        return new BookmarkId(id, type);
    }

    private String getBookmarkTypeString() {
        switch (mType) {
            case BookmarkType.PARTNER:
                return String.valueOf(TYPE_PARTNER);
            case BookmarkType.READING_LIST:
                return String.valueOf(TYPE_READING_LIST);
            case BookmarkType.NORMAL:
            default:
                return "";
        }
    }

    @Override
    public String toString() {
        return getBookmarkTypeString() + mId;
    }

    @Override
    public boolean equals(Object o) {
        if (!(o instanceof BookmarkId)) return false;
        BookmarkId item = (BookmarkId) o;
        return (item.mId == mId && item.mType == mType);
    }

    @Override
    public int hashCode() {
        return toString().hashCode();
    }
}
