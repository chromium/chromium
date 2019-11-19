// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.os.IBinder;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.accessibility.AccessibilityNodeProvider;
import android.widget.FrameLayout;

import org.chromium.base.Log;
import org.chromium.chromecast.base.CastSwitches;

/**
 * View for displaying a WebContents in CastShell.
 *
 * <p>Intended to be used with {@link android.app.Presentation}.
 *
 * <p>
 * Typically, this class is controlled by CastContentWindowAndroid through
 * CastWebContentsSurfaceHelper. If the CastContentWindowAndroid is destroyed,
 * CastWebContentsView should be removed from the activity holding it.
 * Similarily, if the view is removed from a activity or the activity holding
 * it is destroyed, CastContentWindowAndroid should be notified by intent.
 */
public class CastWebContentsView extends FrameLayout {
    private static final String TAG = "CastWebContentV";

    private CastWebContentsSurfaceHelper mSurfaceHelper;

    public CastWebContentsView(Context context) {
        super(context);
        initView();
    }

    private void initView() {
        FrameLayout.LayoutParams matchParent = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT);
        addView(LayoutInflater.from(getContext())
                        .inflate(R.layout.cast_web_contents_activity, null),
                matchParent);
    }

    public void onStart(Bundle startArgumentsBundle) {
        Log.d(TAG, "onStart");

        if (mSurfaceHelper != null) {
            return;
        }

        mSurfaceHelper = new CastWebContentsSurfaceHelper(
                CastWebContentsScopes.onLayoutView(getContext(),
                        findViewById(R.id.web_contents_container),
                        CastSwitches.getSwitchValueColor(
                                CastSwitches.CAST_APP_BACKGROUND_COLOR, Color.BLACK),
                        this ::getHostWindowToken),
                (Uri uri) -> sendIntentSync(CastWebContentsIntentUtils.onWebContentStopped(uri)));

        CastWebContentsSurfaceHelper.StartParams params =
                CastWebContentsSurfaceHelper.StartParams.fromBundle(startArgumentsBundle);
        if (params == null) return;

        mSurfaceHelper.onNewStartParams(params);
    }

    public void onResume() {
        Log.d(TAG, "onResume");
    }

    public void onPause() {
        Log.d(TAG, "onPause");
    }

    public void onStop() {
        Log.d(TAG, "onStop");
        if (mSurfaceHelper != null) {
            mSurfaceHelper.onDestroy();
        }
    }

    @Nullable
    protected IBinder getHostWindowToken() {
        return getWindowToken();
    }

    private void sendIntentSync(Intent in) {
        CastWebContentsIntentUtils.getLocalBroadcastManager().sendBroadcastSync(in);
    }

    @Override
    public void setAccessibilityDelegate(AccessibilityDelegate delegate) {
        View contentView = getContentView();
        if (contentView != null) {
            contentView.setAccessibilityDelegate(delegate);
        } else {
            Log.w(TAG, "Content view is null!");
        }
    }

    @Override
    public boolean onHoverEvent(MotionEvent event) {
        View contentView = getContentView();
        if (contentView != null) {
            return contentView.onHoverEvent(event);
        } else {
            Log.w(TAG, "Content view is null!");
            return false;
        }
    }

    public AccessibilityNodeProvider getWebContentsAccessibilityNodeProvider() {
        View contentView = getContentView();
        if (contentView != null) {
            return contentView.getAccessibilityNodeProvider();
        } else {
            Log.w(TAG, "Content view is null! Returns a null AccessibilityNodeProvider.");
            return null;
        }
    }

    private View getContentView() {
        return findViewWithTag(CastWebContentsScopes.VIEW_TAG_CONTENT_VIEW);
    }
}
