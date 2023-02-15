// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.share;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.ClipData;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.content.res.Resources.NotFoundException;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.browser_ui.share.ShareParams.TargetChosenCallback;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.IntentCallback;

/**
 * A helper class that helps to start an intent to share titles and URLs.
 */
public class ShareHelper {
    /** The task ID of the activity that triggered the share action. */
    private static final String EXTRA_TASK_ID = "org.chromium.chrome.extra.TASK_ID";

    private static final String EXTRA_SHARE_SCREENSHOT_AS_STREAM = "share_screenshot_as_stream";

    /** The string identifier used as a key to set the extra stream's alt text */
    private static final String EXTRA_STREAM_ALT_TEXT = "android.intent.extra.STREAM_ALT_TEXT";

    private static final String ANY_SHARE_HISTOGRAM_NAME = "Sharing.AnyShareStarted";

    // These values are recorded as histogram values. Entries should not be
    // renumbered and numeric values should never be reused.
    @IntDef({ShareSourceAndroid.ANDROID_SHARE_SHEET, ShareSourceAndroid.CHROME_SHARE_SHEET,
            ShareSourceAndroid.DIRECT_SHARE})
    public @interface ShareSourceAndroid {
        // This share is going via the Android share sheet.
        int ANDROID_SHARE_SHEET = 0;

        // This share is going via Chrome's share sheet.
        int CHROME_SHARE_SHEET = 1;

        // This share is happening via directly intenting to a specific share
        // target.
        int DIRECT_SHARE = 2;
        int COUNT = 3;
    }

    protected ShareHelper() {}

    /**
     * Shares the params using the system share sheet. To skip the sheet and sharing directly, use
     * {@link #shareDirectly(ShareParams, ComponentName)}.
     *
     * @param params The container holding the share parameters.
     */
    public static void shareWithSystemShareSheetUi(ShareParams params) {
        recordShareSource(ShareSourceAndroid.ANDROID_SHARE_SHEET);
        TargetChosenReceiver.sendChooserIntent(
                params.getWindow(), getShareIntent(params), params.getCallback());
    }

    /**
     * Loads the icon for the provided ResolveInfo.
     * @param info The ResolveInfo to load the icon for.
     * @param manager The package manager to use to load the icon.
     */
    public static Drawable loadIconForResolveInfo(ResolveInfo info, PackageManager manager) {
        try {
            final int iconRes = info.getIconResource();
            if (iconRes != 0) {
                Resources res = manager.getResourcesForApplication(info.activityInfo.packageName);
                Drawable icon = ApiCompatibilityUtils.getDrawable(res, iconRes);
                return icon;
            }
        } catch (NameNotFoundException | NotFoundException e) {
            // Could not find the icon. loadIcon call below will return the default app icon.
        }
        return info.loadIcon(manager);
    }

    /**
     * Log that a share happened through some means other than ShareHelper.
     * @param source The share source.
     */
    public static void recordShareSource(int source) {
        RecordHistogram.recordEnumeratedHistogram(
                ANY_SHARE_HISTOGRAM_NAME, source, ShareSourceAndroid.COUNT);
    }

    /**
     * Fire the intent to share content with the target app.
     *
     * @param window The current window.
     * @param intent The intent to fire.
     * @param callback The callback to be triggered when the calling activity has finished.  This
     *                 allows the target app to identify Chrome as the source.
     */
    protected static void fireIntent(
            WindowAndroid window, Intent intent, @Nullable IntentCallback callback) {
        if (callback != null) {
            window.showIntent(intent, callback, null);
        } else {
            // TODO(https://crbug.com/1414893): Allow startActivity w/o result via WindowAndroid.
            Activity activity = window.getActivity().get();
            activity.startActivity(intent);
        }
    }

    /**
     * Exposed for browser to send callback without exposing TargetChosenReceiver.
     */
    protected static void sendChooserIntent(
            WindowAndroid window, Intent sharingIntent, @Nullable TargetChosenCallback callback) {
        TargetChosenReceiver.sendChooserIntent(window, sharingIntent, callback);
    }

    /**
     * Receiver to record the chosen component when sharing an Intent.
     */
    @VisibleForTesting
    public static class TargetChosenReceiver extends BroadcastReceiver implements IntentCallback {
        private static final Object LOCK = new Object();

        private static String sTargetChosenReceiveAction;
        private static TargetChosenReceiver sLastRegisteredReceiver;

        @Nullable
        private TargetChosenCallback mCallback;

        private TargetChosenReceiver(@Nullable TargetChosenCallback callback) {
            mCallback = callback;
        }

        public static void sendChooserIntent(WindowAndroid window, Intent sharingIntent,
                @Nullable TargetChosenCallback callback) {
            final Context context = ContextUtils.getApplicationContext();
            final String packageName = context.getPackageName();
            synchronized (LOCK) {
                if (sTargetChosenReceiveAction == null) {
                    sTargetChosenReceiveAction =
                            packageName + "/" + TargetChosenReceiver.class.getName() + "_ACTION";
                }
                if (sLastRegisteredReceiver != null) {
                    context.unregisterReceiver(sLastRegisteredReceiver);
                    // Must cancel the callback (to satisfy guarantee that exactly one method of
                    // TargetChosenCallback is called).
                    sLastRegisteredReceiver.cancel();
                }
                sLastRegisteredReceiver = new TargetChosenReceiver(callback);
                ContextUtils.registerNonExportedBroadcastReceiver(context, sLastRegisteredReceiver,
                        new IntentFilter(sTargetChosenReceiveAction));
            }

            Intent intent = new Intent(sTargetChosenReceiveAction);
            intent.setPackage(packageName);
            IntentUtils.addTrustedIntentExtras(intent);
            Activity activity = window.getActivity().get();
            final PendingIntent pendingIntent = PendingIntent.getBroadcast(activity, 0, intent,
                    PendingIntent.FLAG_CANCEL_CURRENT | PendingIntent.FLAG_ONE_SHOT
                            | IntentUtils.getPendingIntentMutabilityFlag(true));
            Intent chooserIntent = Intent.createChooser(sharingIntent,
                    context.getString(R.string.share_link_chooser_title),
                    pendingIntent.getIntentSender());
            fireIntent(window, chooserIntent, sLastRegisteredReceiver);
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            synchronized (LOCK) {
                if (sLastRegisteredReceiver != this) return;
                ContextUtils.getApplicationContext().unregisterReceiver(sLastRegisteredReceiver);
                sLastRegisteredReceiver = null;
            }
            if (!IntentUtils.isTrustedIntentFromSelf(intent)) return;

            ComponentName target = intent.getParcelableExtra(Intent.EXTRA_CHOSEN_COMPONENT);
            if (mCallback != null) {
                mCallback.onTargetChosen(target);
                mCallback = null;
            }
        }

        @Override
        public void onIntentCompleted(int resultCode, Intent data) {
            // NOTE: The validity of the returned |resultCode| is somewhat unexpected. For
            // background, a sharing flow starts with a "Chooser" activity that enables the user
            // to select the app to share to, and then when the user selects that application,
            // the "Chooser" activity dispatches our "Share" intent to that chosen application.
            //
            // The |resultCode| is only valid if the user does not select an application to share
            // with (e.g. only valid if the "Chooser" activity is the only activity shown). Once
            // the user selects an app in the "Chooser", the |resultCode| received here will always
            // be RESULT_CANCELED (because the "Share" intent specifies NEW_TASK which always
            // returns CANCELED).
            //
            // Thus, this |resultCode| is only valid if we do not receive the EXTRA_CHOSEN_COMPONENT
            // intent indicating the user selected an application in the "Chooser".
            if (resultCode == Activity.RESULT_CANCELED) {
                cancel();
            }
        }

        @VisibleForTesting
        public static void resetForTesting() {
            synchronized (LOCK) {
                sTargetChosenReceiveAction = null;
                if (sLastRegisteredReceiver != null) {
                    ContextUtils.getApplicationContext().unregisterReceiver(
                            sLastRegisteredReceiver);
                    sLastRegisteredReceiver.cancel();
                }
                sLastRegisteredReceiver = null;
            }
        }

        private void cancel() {
            if (mCallback != null) {
                mCallback.onCancel();
                mCallback = null;
            }
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public static Intent getShareIntent(ShareParams params) {
        final boolean isFileShare = (params.getFileUris() != null);
        final boolean isMultipleFileShare = isFileShare && (params.getFileUris().size() > 1);
        final String action =
                isMultipleFileShare ? Intent.ACTION_SEND_MULTIPLE : Intent.ACTION_SEND;
        Intent intent = new Intent(action);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT | Intent.FLAG_ACTIVITY_FORWARD_RESULT
                | Intent.FLAG_ACTIVITY_PREVIOUS_IS_TOP | Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(EXTRA_TASK_ID, params.getWindow().getActivity().get().getTaskId());

        Uri screenshotUri = params.getScreenshotUri();
        if (screenshotUri != null) {
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            // To give read access to an Intent target, we need to put |screenshotUri| in clipData
            // because adding Intent.FLAG_GRANT_READ_URI_PERMISSION doesn't work for
            // EXTRA_SHARE_SCREENSHOT_AS_STREAM.
            intent.setClipData(ClipData.newRawUri("", screenshotUri));
            intent.putExtra(EXTRA_SHARE_SCREENSHOT_AS_STREAM, screenshotUri);
        }

        if (params.getOfflineUri() != null) {
            intent.putExtra(Intent.EXTRA_SUBJECT, params.getTitle());
            intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            intent.putExtra(Intent.EXTRA_STREAM, params.getOfflineUri());
            intent.addCategory(Intent.CATEGORY_DEFAULT);
            intent.setType("multipart/related");
        } else {
            if (!TextUtils.equals(params.getTextAndUrl(), params.getTitle())) {
                intent.putExtra(Intent.EXTRA_SUBJECT, params.getTitle());
            }
            intent.putExtra(Intent.EXTRA_TEXT, params.getTextAndUrl());

            if (isFileShare) {
                intent.setType(params.getFileContentType());
                intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                final boolean hasAltText =
                        params.getFileAltTexts() != null && !params.getFileAltTexts().isEmpty();

                if (isMultipleFileShare) {
                    intent.putParcelableArrayListExtra(Intent.EXTRA_STREAM, params.getFileUris());
                    if (hasAltText) {
                        intent.putStringArrayListExtra(
                                EXTRA_STREAM_ALT_TEXT, params.getFileAltTexts());
                    }
                } else {
                    intent.putExtra(Intent.EXTRA_STREAM, params.getFileUris().get(0));
                    if (hasAltText) {
                        intent.putExtra(EXTRA_STREAM_ALT_TEXT, params.getFileAltTexts().get(0));
                    }
                }
            } else {
                intent.setType("text/plain");
            }
        }

        return intent;
    }
}
