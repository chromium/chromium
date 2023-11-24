// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Content provider for testing content URLs. */
package org.chromium.chrome.test;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.AbstractCursor;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import java.io.File;
import java.io.IOException;
import java.net.URLConnection;
import java.util.HashMap;
import java.util.Map;

/**
 * Content provider for testing content:// urls.
 * Note: if you move this class, make sure you have also updated AndroidManifest.xml
 *
 * Important note about including chromium classes in this content provider:
 * TestContentProvider is part of ChromePublicTest APK. However the instrumentation tests
 * run in the process of the package under test, which is Chrome apk. Normally this is not
 * a problem, however when debug is set to true, Chromium build files enable multidex. In
 * multidex mode, the Chromium files main dex file is in Chrome apk, which are not accessible
 * from the process that runs this ContentProvider.
 *
 * One of the possible workarounds is running this ContentProvider in the same process with
 * Chrome apk (i.e the process that runs the instrumentation tests). However, this
 * requires declaring a sharedUserId between the Chrome apk and ChromePublicTestApk and
 * then setting the target process for ContentProvider for the instrumentation target package.
 *     android:process="{{manifest_package}}"
 *
 * Note that modifying the application manifest file could be problematic as Chrome has
 * side by side channels.
 *
 * The second one is moving the TestContentProvider to the ChromeTestSuport apk. This
 * seems a lot better path than above.
 */
public class TestContentProvider extends ContentProvider {
    private static final String ANDROID_DATA_FILE_PATH = "android/";
    private static final String AUTHORITY = "org.chromium.chrome.test.TestContentProvider";
    private static final String CONTENT_SCHEME = "content://";
    private static final String GET_RESOURCE_REQUEST_COUNT = "get_resource_request_count";
    private static final String RESET_RESOURCE_REQUEST_COUNTS = "reset_resource_request_counts";
    private static final String SET_DATA_PATH = "set_data_path";
    private static final String TAG = "TestContentProvider";
    private static final int EXPECTED_COLUMN_INDEX = 0;
    private Map<String, Integer> mResourceRequestCount;
    private String mDataFilePath;

    // Content providers can be accessed from multiple threads.
    private final Object mLock = new Object();

    public static String createContentUrl(String target) {
        return CONTENT_SCHEME + AUTHORITY + "/" + target;
    }

    private static Uri createRequestUri(final String target, String resource) {
        if (resource == null) {
            return Uri.parse(createContentUrl(target));
        } else {
            return Uri.parse(createContentUrl(target) + "?" + resource);
        }
    }

    public static void setDataFilePath(Context context, String resource) {
        Uri uri = createRequestUri(SET_DATA_PATH, resource);
        context.getContentResolver().query(uri, null, null, null, null);
    }

    public static int getResourceRequestCount(Context context, String resource) {
        Uri uri = createRequestUri(GET_RESOURCE_REQUEST_COUNT, resource);
        final Cursor cursor = context.getContentResolver().query(uri, null, null, null, null);
        try {
            cursor.moveToFirst();
            return cursor.getInt(EXPECTED_COLUMN_INDEX);
        } finally {
            cursor.close();
        }
    }

    public static void resetResourceRequestCounts(Context context) {
        Uri uri = createRequestUri(RESET_RESOURCE_REQUEST_COUNTS, null);
        // A null cursor is returned for this request.
        context.getContentResolver().query(uri, null, null, null, null);
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public ParcelFileDescriptor openFile(final Uri uri, String mode) {
        String resource = uri.getLastPathSegment();
        synchronized (mLock) {
            if (mResourceRequestCount.containsKey(resource)) {
                mResourceRequestCount.put(resource, mResourceRequestCount.get(resource) + 1);
            } else {
                mResourceRequestCount.put(resource, 1);
            }
        }
        try {
            return ParcelFileDescriptor.open(
                    new File(mDataFilePath + "/" + ANDROID_DATA_FILE_PATH + resource),
                    ParcelFileDescriptor.MODE_READ_ONLY);
        } catch (IOException e) {
            Log.e(TAG, e.getMessage(), e);
        }
        return null;
    }

    @Override
    public String getType(Uri uri) {
        return URLConnection.guessContentTypeFromName(uri.getLastPathSegment());
    }

    @Override
    public int update(Uri uri, ContentValues values, String where, String[] whereArgs) {
        return 0;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return 0;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    /** Cursor object for retrieving resource request counters. */
    private static class ProviderStateCursor extends AbstractCursor {
        private final int mResourceRequestCount;

        public ProviderStateCursor(int resourceRequestCount) {
            mResourceRequestCount = resourceRequestCount;
        }

        @Override
        public boolean isNull(int columnIndex) {
            return columnIndex != EXPECTED_COLUMN_INDEX;
        }

        @Override
        public int getCount() {
            return 1;
        }

        @Override
        public int getType(int columnIndex) {
            return columnIndex == EXPECTED_COLUMN_INDEX
                    ? Cursor.FIELD_TYPE_INTEGER
                    : Cursor.FIELD_TYPE_NULL;
        }

        private void unsupported() {
            throw new UnsupportedOperationException();
        }

        @Override
        public double getDouble(int columnIndex) {
            unsupported();
            return 0.0;
        }

        @Override
        public float getFloat(int columnIndex) {
            unsupported();
            return 0.0f;
        }

        @Override
        public int getInt(int columnIndex) {
            return columnIndex == EXPECTED_COLUMN_INDEX ? mResourceRequestCount : -1;
        }

        @Override
        public short getShort(int columnIndex) {
            unsupported();
            return 0;
        }

        @Override
        public long getLong(int columnIndex) {
            return getInt(columnIndex);
        }

        @Override
        public String getString(int columnIndex) {
            unsupported();
            return null;
        }

        @Override
        public String[] getColumnNames() {
            return new String[] {GET_RESOURCE_REQUEST_COUNT};
        }
    }

    @Override
    public Cursor query(
            Uri uri,
            String[] projection,
            String selection,
            String[] selectionArgs,
            String sortOrder) {
        synchronized (mLock) {
            String action = uri.getLastPathSegment();
            String resource = uri.getQuery();
            if (GET_RESOURCE_REQUEST_COUNT.equals(action)) {
                return new ProviderStateCursor(
                        mResourceRequestCount.containsKey(resource)
                                ? mResourceRequestCount.get(resource)
                                : 0);
            } else if (RESET_RESOURCE_REQUEST_COUNTS.equals(action)) {
                mResourceRequestCount = new HashMap<String, Integer>();
            } else if (SET_DATA_PATH.equals(action)) {
                mDataFilePath = resource;
            }
            return null;
        }
    }
}
