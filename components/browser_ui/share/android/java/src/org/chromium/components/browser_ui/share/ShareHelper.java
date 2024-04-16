// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.share;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.ClipData;
import android.content.ComponentName;
import android.content.ContentResolver;
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
import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.UnownedUserData;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.browser_ui.share.ShareParams.TargetChosenCallback;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.IntentCallback;

import java.lang.ref.WeakReference;

/** A helper class that helps to start an intent to share titles and URLs. */
public class ShareHelper {
    private static final String TAG = "AndroidShare";

    /** The task ID of the activity that triggered the share action. */
    private static final String EXTRA_TASK_ID = "org.chromium.chrome.extra.TASK_ID";

    /** The string identifier used as a key to mark the clean up intent. */
    private static final String EXTRA_CLEAN_SHARE_SHEET =
            "org.chromium.chrome.extra.CLEAN_SHARE_SHEET";

    /** The string identifier used as a key to set the extra stream's alt text */
    private static final String EXTRA_STREAM_ALT_TEXT = "android.intent.extra.STREAM_ALT_TEXT";

    private static final String ANY_SHARE_HISTOGRAM_NAME = "Sharing.AnyShareStarted";

    // These values are recorded as histogram values. Entries should not be
    // renumbered and numeric values should never be reused.
    @IntDef({
        ShareSourceAndroid.ANDROID_SHARE_SHEET,
        ShareSourceAndroid.CHROME_SHARE_SHEET,
        ShareSourceAndroid.DIRECT_SHARE
    })
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
        new TargetChosenReceiver(params.getCallback())
                .sendChooserIntent(params.getWindow(), getShareIntent(params));
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
     * Whether the intent is a send back clean up intent. This is an workaround for Chrome to clean
     * the top share sheet activity.
     * @param intent newIntent received by Chrome activity.
     * @return Whether the intent can be ignored.
     */
    public static boolean isCleanerIntent(Intent intent) {
        if (!IntentUtils.isTrustedIntentFromSelf(intent)) return false;
        return IntentUtils.safeGetBooleanExtra(intent, EXTRA_CLEAN_SHARE_SHEET, false);
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
            // TODO(crbug.com/40256344): Allow startActivity w/o result via
            // WindowAndroid.
            Activity activity = window.getActivity().get();
            activity.startActivity(intent);
        }
    }

    /** BroadcastReceiver to record the chosen component when sharing an Intent. */
    public static class TargetChosenReceiver extends BroadcastReceiver
            implements IntentCallback, UnownedUserData {
        private static final UnownedUserDataKey<TargetChosenReceiver> TARGET_CHOSEN_RECEIVER_KEY =
                new UnownedUserDataKey<>(TargetChosenReceiver.class);
        @Nullable private TargetChosenCallback mCallback;
        private WeakReference<Context> mAttachedContext;
        private WeakReference<WindowAndroid> mAttachedWindow;
        private String mReceiverAction;

        protected TargetChosenReceiver(@Nullable TargetChosenCallback callback) {
            mCallback = callback;
            mAttachedContext = new WeakReference<>(null);
            mAttachedWindow = new WeakReference<>(null);
        }

        /**
         * Create a chooser intent and send it to trigger Android share sheet.
         *
         * @param window The {@link WindowAndroid} that starts the sharing.
         * @param sharingIntent The intent with {@link Intent.ACTION_SEND}.
         */
        protected void sendChooserIntent(WindowAndroid window, Intent sharingIntent) {
            ThreadUtils.assertOnUiThread();
            Activity activity = window.getActivity().get();
            assert activity != null;
            final String packageName = activity.getPackageName();
            mReceiverAction =
                    packageName
                            + "/"
                            + TargetChosenReceiver.class.getName()
                            + activity.getTaskId()
                            + "_ACTION";

            TargetChosenReceiver prevReceiver =
                    TARGET_CHOSEN_RECEIVER_KEY.retrieveDataFromHost(
                            window.getUnownedUserDataHost());
            if (prevReceiver != null) {
                Log.e(TAG, "Another BroadcastReceiver already exists in the window.");
                // In case where the receiver is not unregistered correctly, cancel the callback
                // (to satisfy guarantee that exactly one method of TargetChosenCallback is called).
                prevReceiver.cancel();
            }
            TARGET_CHOSEN_RECEIVER_KEY.attachToHost(window.getUnownedUserDataHost(), this);
            mAttachedWindow = new WeakReference<>(window);

            ContextUtils.registerNonExportedBroadcastReceiver(
                    activity, this, new IntentFilter(mReceiverAction));
            mAttachedContext = new WeakReference<>(activity);

            Intent chooserIntent = getChooserIntent(window, sharingIntent);
            ShareHelper.fireIntent(window, chooserIntent, this);
        }

        /** Create the chooser intent via {@link android.content.Intent.createChooser} */
        protected Intent getChooserIntent(WindowAndroid window, Intent sharingIntent) {
            Intent intent = createSendBackIntentWithFilteredAction();
            Activity activity = window.getActivity().get();
            final PendingIntent pendingIntent =
                    PendingIntent.getBroadcast(
                            activity,
                            activity.getTaskId(),
                            intent,
                            PendingIntent.FLAG_CANCEL_CURRENT
                                    | PendingIntent.FLAG_ONE_SHOT
                                    | IntentUtils.getPendingIntentMutabilityFlag(true));
            return Intent.createChooser(
                    sharingIntent,
                    activity.getString(R.string.share_link_chooser_title),
                    pendingIntent.getIntentSender());
        }

        /**
         * Create an intent to be carried by {@link PendingIntent.getBroadcast}, and will be
         * received after the PendingIntent is sent. The input action is used to match
         * the {@link IntentFilter} that this broadcast receiver is interested with.
         */
        protected Intent createSendBackIntentWithFilteredAction() {
            final Context context = ContextUtils.getApplicationContext();
            Intent intent = new Intent(mReceiverAction);
            intent.setPackage(context.getPackageName());
            // Adding intent extras since non-exported broadcast listener does not exist pre-T.
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
                IntentUtils.addTrustedIntentExtras(intent);
            }
            return intent;
        }

        protected void onReceiveInternal(Context context, Intent intent) {}

        @Override
        public void onReceive(Context context, Intent intent) {
            ThreadUtils.assertOnUiThread();
            // Ignore intents that's not initiated from Chrome.
            if (isUntrustedIntent(intent)) {
                return;
            }

            onReceiveInternal(context, intent);
            ComponentName target = intent.getParcelableExtra(Intent.EXTRA_CHOSEN_COMPONENT);
            if (mCallback != null) {
                mCallback.onTargetChosen(target);
                mCallback = null;
            }
            detach();
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

        @Override
        public void onDetachedFromHost(UnownedUserDataHost host) {
            // Remove the weak reference to the context and window when it is removed from the
            // attaching window.
            if (mAttachedContext.get() != null) {
                Log.i(TAG, "Dispatch cleaning intent to close the share sheet.");
                // Issue a cleaner intent so the share sheet is cleared. This is a workaround to
                // close the top ChooserActivity when share isn't completed.
                Intent cleanerIntent = createCleanupIntent();
                mAttachedContext.get().startActivity(cleanerIntent);
            }
            cancel();
        }

        protected Intent createCleanupIntent() {
            Intent cleanerIntent = new Intent();
            cleanerIntent.setClass(mAttachedContext.get(), mAttachedContext.get().getClass());
            cleanerIntent.putExtra(EXTRA_CLEAN_SHARE_SHEET, true);
            cleanerIntent.setFlags(
                    Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
            IntentUtils.addTrustedIntentExtras(cleanerIntent);
            return cleanerIntent;
        }

        private boolean isUntrustedIntent(Intent intent) {
            return Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU
                    && !IntentUtils.isTrustedIntentFromSelf(intent);
        }

        private void detach() {
            assert mCallback == null : "Callback is never called before this receiver is detached.";

            if (mAttachedContext.get() != null) {
                mAttachedContext.get().unregisterReceiver(this);
                mAttachedContext.clear();
            }
            if (mAttachedWindow.get() != null) {
                TARGET_CHOSEN_RECEIVER_KEY.detachFromHost(
                        mAttachedWindow.get().getUnownedUserDataHost());
                mAttachedWindow.clear();
            }
        }

        private void cancel() {
            if (mCallback != null) {
                mCallback.onCancel();
                mCallback = null;
            }
            detach();
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public static Intent getShareIntent(ShareParams params) {
        final boolean isFileShare = (params.getFileUris() != null);
        final boolean isMultipleFileShare = isFileShare && (params.getFileUris().size() > 1);
        final String action =
                isMultipleFileShare ? Intent.ACTION_SEND_MULTIPLE : Intent.ACTION_SEND;
        Intent intent = new Intent(action);
        intent.addFlags(
                Intent.FLAG_ACTIVITY_NEW_DOCUMENT
                        | Intent.FLAG_ACTIVITY_FORWARD_RESULT
                        | Intent.FLAG_ACTIVITY_PREVIOUS_IS_TOP
                        | Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(EXTRA_TASK_ID, params.getWindow().getActivity().get().getTaskId());

        Uri imageUri = params.getImageUriToShare();
        if (imageUri != null && !isMultipleFileShare) {
            intent.putExtra(Intent.EXTRA_STREAM, imageUri);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);

            // Add text, title and clip data preview for the image being shared.
            ContentResolver resolver = ContextUtils.getApplicationContext().getContentResolver();
            intent.setType(resolver.getType(imageUri));
            intent.setClipData(ClipData.newUri(resolver, null, imageUri));
            if (!TextUtils.isEmpty(params.getUrl())) {
                intent.putExtra(Intent.EXTRA_TEXT, params.getUrl());
            }
            if (!TextUtils.isEmpty(params.getImageAltText())) {
                intent.putExtra(EXTRA_STREAM_ALT_TEXT, params.getImageAltText());
            }

            return intent;
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

                if (isMultipleFileShare) {
                    intent.putParcelableArrayListExtra(Intent.EXTRA_STREAM, params.getFileUris());
                } else {
                    intent.putExtra(Intent.EXTRA_STREAM, params.getFileUris().get(0));
                }
            } else {
                intent.setType("text/plain");
                intent.putExtra(Intent.EXTRA_TITLE, params.getTitle());
                // For text sharing, only set the preview title when preview image is provided. This
                // is meant to avoid confusion about the content being shared.
                if (params.getPreviewImageUri() != null) {
                    intent.setClipData(ClipData.newRawUri("", params.getPreviewImageUri()));
                    intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                }
            }
        }

        return intent;
    }
}
