// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.ComponentName;
import android.content.Context;
import android.content.LocusId;
import android.os.Build;
import android.view.contentcapture.ContentCaptureCondition;
import android.view.contentcapture.ContentCaptureManager;
import android.view.contentcapture.DataRemovalRequest;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;
import java.util.regex.Pattern;

/**
 * The class talks to the ContentCaptureManager to verify ContentCaptureService is Aiai and provide
 * the methods to check if the given urls shall be captured and delete the ContentCapture history.
 */
@RequiresApi(Build.VERSION_CODES.Q)
@NullMarked
public class PlatformContentCaptureController {
    private static final String TAG = "ContentCapture";
    private static final String AIAI_PACKAGE_NAME = "com.google.android.as";

    private static @Nullable PlatformContentCaptureController sContentCaptureController;

    private boolean mShouldStartCapture;
    private boolean mIsAiai;
    private @Nullable UrlAllowlist mAllowlist;
    private final ContentCaptureManager mContentCaptureManager;

    public static PlatformContentCaptureController lazyInit() {
        if (sContentCaptureController == null) {
            sContentCaptureController =
                new PlatformContentCaptureController(ContextUtils.getApplicationContext());
        }
        return sContentCaptureController;
    }

    public static @Nullable PlatformContentCaptureController getInstance() {
        return sContentCaptureController;
    }

    public PlatformContentCaptureController(Context context) {
        mContentCaptureManager = context.getSystemService(ContentCaptureManager.class);
        verifyService();
        pullAllowlist();
    }

    public boolean shouldStartCapture() {
        return mShouldStartCapture;
    }

    private void verifyService() {
        if (mContentCaptureManager == null) {
            log("ContentCaptureManager isn't available.");
            return;
        }

        ComponentName componentName = null;
        try {
            componentName = mContentCaptureManager.getServiceComponentName();
        } catch (RuntimeException e) {
            Log.e(TAG, "Error to get component name", e);
        }
        if (componentName == null) {
            log("Service isn't available.");
            return;
        }

        mIsAiai = AIAI_PACKAGE_NAME.equals(componentName.getPackageName());
        if (!mIsAiai) {
            log(
                    "Package doesn't match, current one is "
                            + assumeNonNull(mContentCaptureManager.getServiceComponentName())
                                    .getPackageName());
            // Disable the ContentCapture if there is no testing flag.
            if (!BuildInfo.isDebugAndroid() && !ContentCaptureFeatures.isDumpForTestingEnabled()) {
                return;
            }
        }

        mShouldStartCapture = mContentCaptureManager.isContentCaptureEnabled();
        if (!mShouldStartCapture) {
            log("ContentCapture disabled.");
        }
    }

    private void pullAllowlist() {
        if (mContentCaptureManager == null) {
            // Nothing shall be captured.
            mAllowlist = new UrlAllowlist(null, null);
            return;
        }
        Set<ContentCaptureCondition> conditions =
                mContentCaptureManager.getContentCaptureConditions();
        if (conditions == null) return;

        HashSet<String> allowedUrls = null;
        ArrayList<Pattern> allowedRe = null;
        for (ContentCaptureCondition c : conditions) {
            if ((c.getFlags() & ContentCaptureCondition.FLAG_IS_REGEX) != 0) {
                if (allowedRe == null) allowedRe = new ArrayList<Pattern>();
                allowedRe.add(Pattern.compile(c.getLocusId().getId()));
            } else {
                if (allowedUrls == null) allowedUrls = new HashSet<String>();
                allowedUrls.add(c.getLocusId().getId());
            }
        }
        mAllowlist = new UrlAllowlist(allowedUrls, allowedRe);
    }

    private void log(String msg) {
        if (!ContentCaptureFeatures.isDumpForTestingEnabled()) return;
        Log.i(TAG, msg);
    }

    public void clearAllContentCaptureData() {
        if (mContentCaptureManager == null) return;

        mContentCaptureManager.removeData(new DataRemovalRequest.Builder().forEverything().build());
    }

    public void clearContentCaptureDataForURLs(String[] urlsToDelete) {
        if (mContentCaptureManager == null) return;

        DataRemovalRequest.Builder builder = new DataRemovalRequest.Builder();
        for (String url : urlsToDelete) {
            builder =
                    builder.addLocusId(
                            new LocusId(url), /* Signals that we aren't using extra flags */ 0);
        }
        mContentCaptureManager.removeData(builder.build());
    }

    /**
     * @return if any of the given allows to be captured.
     */
    public boolean shouldCapture(String[] urls) {
        if (mAllowlist == null) return true;
        return mAllowlist.isAllowed(urls);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public boolean isAiai() {
        return mIsAiai;
    }
}
