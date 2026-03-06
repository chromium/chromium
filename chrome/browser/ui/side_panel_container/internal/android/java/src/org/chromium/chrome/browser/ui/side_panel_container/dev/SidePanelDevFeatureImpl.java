// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container.dev;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.Color;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.side_panel_container.SidePanelContainerCoordinator;
import org.chromium.chrome.browser.ui.side_panel_container.SidePanelContent;

/** Implements {@link SidePanelDevFeature}. */
@NullMarked
public final class SidePanelDevFeatureImpl implements SidePanelDevFeature {

    private final SidePanelContainerCoordinator mSidePanelContainerCoordinator;
    private final SidePanelContent mDevContent;

    public SidePanelDevFeatureImpl(
            Activity parentActivity, SidePanelContainerCoordinator sidePanelContainerCoordinator) {
        assert ChromeFeatureList.sEnableAndroidSidePanelDevFeature.isEnabled()
                : "SidePanelDevFeature can only be used when its feature flag is enabled";

        mSidePanelContainerCoordinator = sidePanelContainerCoordinator;
        mDevContent = createSidePanelContent(parentActivity);
    }

    @Override
    public void toggle() {
        ThreadUtils.assertOnUiThread();

        if (!mSidePanelContainerCoordinator.isShowing(mDevContent)) {
            mSidePanelContainerCoordinator.populateContent(mDevContent);
        } else {
            mSidePanelContainerCoordinator.removeContent();
        }
    }

    @Override
    public void destroy() {
        // TODO(crbug.com/489184906): Implement destroy() when we use a ThinWebView.
        // The ThinWebView and its underlying WebContents should be destroyed here.
    }

    @SuppressLint("SetTextI18n")
    private static SidePanelContent createSidePanelContent(Activity parentActivity) {
        TextView contentView = new TextView(parentActivity);
        contentView.setText("Dev Side Panel");
        contentView.setBackgroundColor(Color.GREEN);
        contentView.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        contentView.setGravity(Gravity.CENTER);
        return new SidePanelContent(contentView);
    }
}
