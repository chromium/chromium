// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.os.SystemClock;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A class for controlling the page info discoverability metrics reporting.
 */
public class PageInfoDiscoverabilityMetrics {
    /**
     * UMA statistics for PageInfoDiscoverability. Do not reorder or remove existing fields. All
     * values here should have corresponding entries in WebsiteSettingsDiscoverabilityAction area of
     * enums.xml.
     */
    @IntDef({DiscoverabilityAction.PERMISSION_ICON_SHOWN, DiscoverabilityAction.PAGE_INFO_OPENED,
            DiscoverabilityAction.PERMISSIONS_OPENED, DiscoverabilityAction.PERMISSION_CHANGED,
            DiscoverabilityAction.STORE_ICON_SHOWN,
            DiscoverabilityAction.PAGE_INFO_OPENED_FROM_STORE_ICON,
            DiscoverabilityAction.STORE_INFO_OPENED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DiscoverabilityAction {
        int PERMISSION_ICON_SHOWN = 0;
        int PAGE_INFO_OPENED = 1;
        int PERMISSIONS_OPENED = 2;
        int PERMISSION_CHANGED = 3;
        int STORE_ICON_SHOWN = 4;
        int PAGE_INFO_OPENED_FROM_STORE_ICON = 5;
        int STORE_INFO_OPENED = 6;
        int NUM_ENTRIES = 7;
    }
    private Long mPermissionIconShownTime;
    private Long mPageInfoOpenedTime;
    private Long mStoreIconShownTime;
    private Long mPageInfoOpenedFromStoreIconTime;

    public void recordDiscoverabilityAction(@DiscoverabilityAction int action) {
        RecordHistogram.recordEnumeratedHistogram("WebsiteSettings.Discoverability.Action", action,
                DiscoverabilityAction.NUM_ENTRIES);

        switch (action) {
            case DiscoverabilityAction.PERMISSION_ICON_SHOWN:
                mPermissionIconShownTime = SystemClock.elapsedRealtime();
                break;
            case DiscoverabilityAction.PAGE_INFO_OPENED:
                assert mPermissionIconShownTime != null;
                mPageInfoOpenedTime = SystemClock.elapsedRealtime();
                RecordHistogram.recordTimesHistogram("WebsiteSettings.Discoverability.TimeToOpen",
                        mPageInfoOpenedTime - mPermissionIconShownTime);
                mPermissionIconShownTime = null;
                break;
            case DiscoverabilityAction.PERMISSIONS_OPENED:
                // A user can open permissions multiple times but we only want to include the first
                // time after the page info was opened.
                if (mPageInfoOpenedTime != null) {
                    RecordHistogram.recordMediumTimesHistogram(
                            "WebsiteSettings.Discoverability.TimeToClickHighlight",
                            SystemClock.elapsedRealtime() - mPageInfoOpenedTime);
                }
                mPageInfoOpenedTime = null;
                break;
            case DiscoverabilityAction.STORE_ICON_SHOWN:
                mStoreIconShownTime = SystemClock.elapsedRealtime();
                break;
            case DiscoverabilityAction.PAGE_INFO_OPENED_FROM_STORE_ICON:
                assert mStoreIconShownTime != null;
                mPageInfoOpenedFromStoreIconTime = SystemClock.elapsedRealtime();
                RecordHistogram.recordTimesHistogram(
                        "WebsiteSettings.Discoverability.TimeToOpenFromStoreIcon",
                        mPageInfoOpenedFromStoreIconTime - mStoreIconShownTime);
                mStoreIconShownTime = null;
                break;
            case DiscoverabilityAction.STORE_INFO_OPENED:
                // A user can open store info multiple times but we only want to include the first
                // time after the page info was opened.
                if (mPageInfoOpenedFromStoreIconTime != null) {
                    RecordHistogram.recordMediumTimesHistogram(
                            "WebsiteSettings.Discoverability.TimeToClickHighlightStoreInfo",
                            SystemClock.elapsedRealtime() - mPageInfoOpenedFromStoreIconTime);
                }
                mPageInfoOpenedFromStoreIconTime = null;
                break;
            default:
                break;
        }
    }
}
