// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.RemoteViews;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

/**
 * Performs inflation of {@link RemoteViews} taking into account the local night mode.
 * {@link RemoteViews#apply} always uses resource configuration corresponding to system
 *  settings, see https://buganizer.corp.google.com/issues/133424086, http://crbug.com/1626864.
 */
public class RemoteViewsWithNightModeInflater {
    private static final String TAG = "RemoteViewsInflater";

    /**
     * Inflates the RemoteViews.
     *
     * @param remoteViews {@link RemoteViews} to inflate.
     * @param parent Parent {@link ViewGroup} to use for inflation
     * @param isInLocalNightMode Whether night mode is enabled for the current Activity.
     * @param isInSystemNightMode Whether night mode is enabled in system settings.
     * @return Inflated View or null in case of failure.
     */
    public static @Nullable View inflate(
            RemoteViews remoteViews,
            @Nullable ViewGroup parent,
            boolean isInLocalNightMode,
            boolean isInSystemNightMode) {
        if (isInLocalNightMode == isInSystemNightMode) {
            // RemoteViews#apply will use the resource configuration corresponding to system
            // settings.
            return inflateNormally(remoteViews, parent);
        }
        View view = inflateWithEnforcedDarkMode(remoteViews, parent, isInLocalNightMode);
        if (view == null) {
            view = inflateNormally(remoteViews, parent);
            assert view == null : "Failed to inflate valid RemoteViews with enforced dark mode";
        }
        return view;
    }

    private static @Nullable View inflateNormally(RemoteViews remoteViews, ViewGroup parent) {
        try {
            return remoteViews.apply(ContextUtils.getApplicationContext(), parent);
        } catch (RuntimeException e) {
            // Catching a general RuntimeException is ugly, but RemoteViews are passed in by the
            // client app, so can contain all sorts of problems, eg. b/205503898.
            Log.e(TAG, "Failed to inflate the RemoteViews", e);
            return null;
        }
    }

    private static @Nullable View inflateWithEnforcedDarkMode(
            RemoteViews remoteViews, ViewGroup parent, boolean isInLocalNightMode) {
        // This is a modified version of RemoteViews#apply. RemoteViews#apply performs two steps:
        // 1. Inflate the View using the context of the remote app.
        // 2. Apply the Actions to the inflated View (actions are requested by remote app using
        // various setters, such as RemoteViews#setTextViewText).
        //
        // The context used at step 1 does not respect the Configuration override of the Context
        // we pass as an argument of apply().
        //
        // Here we perform step 1 manually, overriding the Configuration just before inflating.
        // Then we perform step 2 using RemoteViews#reapply on an already inflated View.

        try {
            final Context contextForResources =
                    getContextForResources(remoteViews, isInLocalNightMode);
            // App context must be used instead of activity context to avoid the support library
            // bug, see https://crbug.com/783834
            Context appContext = ContextUtils.getApplicationContext();
            Context contextForRemoteViews =
                    new RemoteViewsContextWrapper(appContext, contextForResources);

            LayoutInflater inflater =
                    LayoutInflater.from(appContext).cloneInContext(contextForRemoteViews);
            View view = inflater.inflate(remoteViews.getLayoutId(), parent, false);

            remoteViews.reapply(appContext, view);
            return view;
        } catch (PackageManager.NameNotFoundException | RuntimeException e) {
            // Catching a general RuntimeException is ugly, but RemoteViews are passed in by the
            // client app, so can contain all sorts of problems, eg b/205503898.
            Log.e(TAG, "Failed to inflate the RemoteViews", e);
            return null;
        }
    }

    private static Context getContextForResources(
            RemoteViews remoteViews, boolean isInLocalNightMode)
            throws PackageManager.NameNotFoundException {
        Context appContext = ContextUtils.getApplicationContext();
        String remotePackage = remoteViews.getPackage();
        if (appContext.getPackageName().equals(remotePackage)) return appContext;

        Context remoteContext =
                appContext.createPackageContext(remotePackage, Context.CONTEXT_RESTRICTED);

        // This line is what makes the difference with RemoteViews#apply.
        Context contextWithEnforcedNightMode =
                NightModeUtils.wrapContextWithNightModeConfig(
                        remoteContext, /* themeResId= */ 0, isInLocalNightMode);

        return contextWithEnforcedNightMode;
    }

    // Copied from RemoteViews
    private static class RemoteViewsContextWrapper extends ContextWrapper {
        private final Context mContextForResources;

        RemoteViewsContextWrapper(Context context, Context contextForResources) {
            super(context);
            mContextForResources = contextForResources;
        }

        @Override
        public Resources getResources() {
            return mContextForResources.getResources();
        }

        @Override
        public Resources.Theme getTheme() {
            return mContextForResources.getTheme();
        }

        @Override
        public String getPackageName() {
            return mContextForResources.getPackageName();
        }
    }
}
