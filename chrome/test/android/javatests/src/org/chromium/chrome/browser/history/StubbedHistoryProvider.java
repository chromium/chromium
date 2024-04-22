// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.text.TextUtils;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;

/** Stubs out the backends used by the native Android browsing history manager. */
public class StubbedHistoryProvider implements HistoryProvider {
    public final CallbackHelper markItemForRemovalCallback = new CallbackHelper();
    public final CallbackHelper removeItemsCallback = new CallbackHelper();

    private BrowsingHistoryObserver mObserver;
    private List<HistoryItem> mItems = new ArrayList<>();
    private List<HistoryItem> mSearchItems = new ArrayList<>();
    private List<HistoryItem> mRemovedItems = new ArrayList<>();

    /** The exclusive end position for the last query. **/
    private int mLastQueryEndPosition;

    private String mLastQuery;
    private int mPaging = 5;
    private boolean mHostOnly;

    private boolean mQueryAppsTriggered;

    @Override
    public void setObserver(BrowsingHistoryObserver observer) {
        mObserver = observer;
    }

    @Override
    public void queryHistory(String query, String appId) {
        mHostOnly = false;
        query(query);
    }

    @Override
    public void queryHistoryForHost(String hostName) {
        mHostOnly = true;
        query(hostName);
    }

    private void query(String query) {
        mLastQueryEndPosition = 0;
        mLastQuery = query;
        queryHistoryContinuation();
    }

    @Override
    public void queryHistoryContinuation() {
        // Simulate basic paging to facilitate testing loading more items.

        boolean isSearch = !TextUtils.isEmpty(mLastQuery);
        if (!isSearch) {
            mSearchItems.clear();
        } else if (mLastQueryEndPosition == 0) {
            mSearchItems.clear();
            // Start a new search; simulate basic search.
            mLastQuery = mLastQuery.toLowerCase(Locale.getDefault());
            for (HistoryItem item : mItems) {
                if (mHostOnly) {
                    if (item.getUrl()
                            .getHost()
                            .toLowerCase(Locale.getDefault())
                            .equals(mLastQuery)) {
                        mSearchItems.add(item);
                    }
                } else if (item.getUrl()
                                .getSpec()
                                .toLowerCase(Locale.getDefault())
                                .contains(mLastQuery)
                        || item.getTitle().toLowerCase(Locale.getDefault()).contains(mLastQuery)) {
                    mSearchItems.add(item);
                }
            }
        }

        int queryStartPosition = mLastQueryEndPosition;
        int queryStartPositionPlusPaging = mLastQueryEndPosition + mPaging;

        List<HistoryItem> targetItems = isSearch ? mSearchItems : mItems;
        boolean hasMoreItems = queryStartPositionPlusPaging < targetItems.size();
        int queryEndPosition = hasMoreItems ? queryStartPositionPlusPaging : targetItems.size();

        mLastQueryEndPosition = queryEndPosition;

        List<HistoryItem> items = targetItems.subList(queryStartPosition, queryEndPosition);
        mObserver.onQueryHistoryComplete(items, hasMoreItems);
    }

    @Override
    public void queryApps() {
        mQueryAppsTriggered = true;
    }

    public boolean isQueryAppsTriggered() {
        return mQueryAppsTriggered;
    }

    @Override
    public void getLastVisitToHostBeforeRecentNavigations(
            String hostName, Callback<Long> callback) {
        long timestamp = 0;
        if (mItems.size() > 0) {
            Collections.sort(
                    mItems,
                    new Comparator<HistoryItem>() {
                        @Override
                        public int compare(HistoryItem lhs, HistoryItem rhs) {
                            long timeDelta = lhs.getTimestamp() - rhs.getTimestamp();
                            if (timeDelta > 0) {
                                return -1;
                            } else if (timeDelta == 0) {
                                return 0;
                            } else {
                                return 1;
                            }
                        }
                    });
            timestamp = mItems.get(0).getTimestamp();
        }
        callback.onResult(Long.valueOf(timestamp));
    }

    @Override
    public void markItemForRemoval(HistoryItem item) {
        mRemovedItems.add(item);
        markItemForRemovalCallback.notifyCalled();
    }

    @Override
    public void removeItems() {
        for (HistoryItem item : mRemovedItems) {
            mItems.remove(item);
        }
        mRemovedItems.clear();
        removeItemsCallback.notifyCalled();
        mObserver.onHistoryDeleted();
    }

    @Override
    public void destroy() {}

    public void addItem(HistoryItem item) {
        mItems.add(item);
    }

    public void removeItem(HistoryItem item) {
        mItems.remove(item);
    }

    public void setPaging(int paging) {
        mPaging = paging;
    }

    public int getPaging() {
        return mPaging;
    }

    public static HistoryItem createHistoryItem(int which, long timestamp) {
        long[] nativeTimestamps = {timestamp * 1000};
        String appId = null;
        if (which == 0) {
            return new HistoryItem(
                    JUnitTestGURLs.SEARCH_URL,
                    "www.google.com",
                    "Google",
                    appId,
                    timestamp,
                    nativeTimestamps,
                    false);
        } else if (which == 1) {
            return new HistoryItem(
                    JUnitTestGURLs.EXAMPLE_URL,
                    "www.example.com",
                    "Foo",
                    appId,
                    timestamp,
                    nativeTimestamps,
                    false);
        } else if (which == 2) {
            return new HistoryItem(
                    JUnitTestGURLs.URL_1,
                    "www.one.com",
                    "Bar",
                    appId,
                    timestamp,
                    nativeTimestamps,
                    false);
        } else if (which == 3) {
            return new HistoryItem(
                    JUnitTestGURLs.URL_2,
                    "www.two.com",
                    "News",
                    appId,
                    timestamp,
                    nativeTimestamps,
                    false);
        } else if (which == 4) {
            return new HistoryItem(
                    JUnitTestGURLs.URL_3,
                    "www.three.com",
                    "Engineering",
                    appId,
                    timestamp,
                    nativeTimestamps,
                    false);
        } else if (which == 5) {
            return new HistoryItem(
                    JUnitTestGURLs.INITIAL_URL,
                    "initial.com",
                    "Cannot Visit",
                    appId,
                    timestamp,
                    nativeTimestamps,
                    true);
        } else {
            return null;
        }
    }
}
