// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.embedder_support.util.UrlConstants;

import java.util.ArrayList;
import java.util.List;
import java.util.PriorityQueue;
import java.util.Set;

/** Collection of utility methods that operates on Tab for Fusebox. */
@NullMarked
public class FuseboxTabUtils {
    private static final int MAX_RECENT_TABS = 3;

    /**
     * Returns the whether a tab is eligible for attaching it's web content. This does not exclude
     * tabs based on specific tab model - including incognito tab model.
     *
     * @param tab The tab to be checked.
     */
    public static boolean isTabEligibleForAttachment(@Nullable Tab tab) {
        // TODO: This also has to check the eligibility here:
        // components/optimization_guide/content/browser/page_context_eligibility.h
        return tab != null
                && (tab.getUrl().getScheme().equals(UrlConstants.HTTP_SCHEME)
                        || tab.getUrl().getScheme().equals(UrlConstants.HTTPS_SCHEME));
    }

    /**
     * Returns the whether a tab is active.
     *
     * @param tab The tab to be checked.
     */
    public static boolean isTabActive(@Nullable Tab tab) {
        // Note: this intentionally accepts tabs that haven't finished loading.
        // Fully loaded state should not be required and doesn't imply the content isn't available.
        // Certain pages may not fully complete loading if part of the content takes longer than
        // usual.
        return tab != null
                && tab.isInitialized()
                && !tab.isFrozen()
                && tab.getWebContents() != null
                && tab.getWebContents().getRenderWidgetHostView() != null;
    }

    /**
     * Returns the drawable given the favicon of the tab.
     *
     * @param context An Android context.
     * @param favicon The favicon of the tab.
     * @param iconSizePx The size (both width and height) to scale to.
     */
    public static Drawable getDrawableForTabFavicon(
            Context context, @Nullable Bitmap favicon, @Px int iconSizePx) {
        Drawable drawable;
        if (favicon != null) {
            Bitmap bitmap =
                    Bitmap.createScaledBitmap(favicon, iconSizePx, iconSizePx, /* filter= */ true);
            drawable = new BitmapDrawable(context.getResources(), bitmap);
            drawable.setBounds(
                    /* left= */ 0, /* top= */ 0, /* right= */ iconSizePx, /* bottom= */ iconSizePx);
        } else {
            drawable = assumeNonNull(context.getDrawable(R.drawable.ic_globe_24dp));
        }
        return drawable;
    }

    /**
     * Collect, filters, and caps eligible recent tabs from the given TabModelSelector.
     *
     * @param selector The TabModelSelector to harvest from.
     * @param attachedIds Set of tab IDs currently attached to the Fusebox.
     */
    public static List<Tab> getEligibleRecentTabs(
            TabModelSelector selector, Set<Integer> attachedIds) {
        if (selector == null) return List.of();
        boolean isOffTheRecord = selector.isOffTheRecordModelSelected();
        boolean allowBackgroundCapture =
                ChromeFeatureList.sOnDemandBackgroundTabContextCapture.isEnabled()
                        && !isOffTheRecord;

        TabModel tabModel = selector.getModel(isOffTheRecord);
        return collectEligibleTabs(tabModel, attachedIds, allowBackgroundCapture);
    }

    private static List<Tab> collectEligibleTabs(
            @Nullable TabModel tabModel, Set<Integer> attachedIds, boolean allowBackgroundCapture) {
        if (tabModel == null) return List.of();

        // Min-heap of size MAX_RECENT_TABS + 1 to temporarily hold the extra item before eviction.
        PriorityQueue<Tab> minHeap =
                new PriorityQueue<>(
                        MAX_RECENT_TABS + 1,
                        (t1, t2) -> Long.compare(t1.getTimestampMillis(), t2.getTimestampMillis()));

        for (int i = 0; i < tabModel.getCount(); i++) {
            Tab tab = tabModel.getTabAt(i);
            if (tab == null || attachedIds.contains(tab.getId())) {
                continue;
            }
            if (!isTabEligibleForAttachment(tab)) continue;
            if (!isTabActive(tab) && !allowBackgroundCapture) continue;

            minHeap.add(tab);
            if (minHeap.size() > MAX_RECENT_TABS) {
                minHeap.poll();
            }
        }

        List<Tab> recentTabs = new ArrayList<>(minHeap.size());
        while (!minHeap.isEmpty()) {
            recentTabs.add(0, minHeap.poll());
        }
        return recentTabs;
    }
}
