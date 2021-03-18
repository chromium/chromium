// Copyright 2021 The Chromium Authors. All rights reserved.
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
            DiscoverabilityAction.PERMISSIONS_OPENED, DiscoverabilityAction.PERMISSION_CHANGED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DiscoverabilityAction {
        int PERMISSION_ICON_SHOWN = 0;
        int PAGE_INFO_OPENED = 1;
        int PERMISSIONS_OPENED = 2;
        int PERMISSION_CHANGED = 3;
        int NUM_ENTRIES = 4;
    }
    private Long mPermissionIconShownTime;

    public void recordDiscoverabilityAction(@DiscoverabilityAction int action) {
        RecordHistogram.recordEnumeratedHistogram("WebsiteSettings.Discoverability.Action", action,
                DiscoverabilityAction.NUM_ENTRIES);

        switch (action) {
            case DiscoverabilityAction.PERMISSION_ICON_SHOWN:
                mPermissionIconShownTime = SystemClock.elapsedRealtime();
                break;
            case DiscoverabilityAction.PAGE_INFO_OPENED:
                assert mPermissionIconShownTime != null;
                RecordHistogram.recordTimesHistogram("WebsiteSettings.Discoverability.TimeToOpen",
                        SystemClock.elapsedRealtime() - mPermissionIconShownTime);
                mPermissionIconShownTime = null;
                break;
            default:
                break;
        }
    }
}
