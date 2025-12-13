// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import android.content.Context;
import android.os.Build;
import android.view.PointerIcon;

import androidx.annotation.AnyThread;
import androidx.annotation.IntDef;
import androidx.annotation.MainThread;

import org.chromium.base.TraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.StylusWritingHandler;
import org.chromium.content_public.browser.WebContents;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import javax.annotation.concurrent.GuardedBy;

/**
 * Helper class to determine whether Direct writing service is in consideration or the Android
 * platform Stylus Writing feature, and to set the appropriate handler to WebContents.
 */
@NullMarked
public class StylusWritingController {
    private final Context mContext;
    private @Nullable WebContents mCurrentWebContents;
    private @Nullable PointerIcon mHandwritingIcon;
    private @Nullable StylusApiOption mStylusHandler;
    private boolean mIconFetched;
    private final boolean mLazyFetchHandWritingIconFeatureEnabled;
    private boolean mShouldOverrideStylusHoverIcon;
    private boolean mIsWindowFocused;

    @IntDef({
        HandlerType.UNSET,
        HandlerType.DIRECT_WRITING_TRIGGER,
        HandlerType.ANDROID_HANDLER,
        HandlerType.DISABLED_STYLUS_WRITING_HANDLER,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface HandlerType {
        int UNSET = 0;
        int DIRECT_WRITING_TRIGGER = 1;
        int ANDROID_HANDLER = 2;
        int DISABLED_STYLUS_WRITING_HANDLER = 3;
    }

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private @HandlerType int mHandlerType = HandlerType.UNSET;

    private @Nullable AndroidStylusWritingHandler mAndroidHandler;
    private @Nullable DirectWritingTrigger mDirectWritingTrigger;
    private @Nullable DisabledStylusWritingHandler mDisabledStylusWritingHandler;

    static StylusWritingController createControllerForTests(Context context, PointerIcon icon) {
        StylusWritingController controller = new StylusWritingController(context);
        controller.mIconFetched = true;
        controller.mHandwritingIcon = icon;
        return controller;
    }

    /** Creates a new instance of this class. */
    @MainThread
    public StylusWritingController(Context context) {
        this(context, false);
    }

    @MainThread
    public StylusWritingController(
            Context context, boolean lazyFetchHandWritingIconFeatureEnabled) {
        mContext = context;
        mLazyFetchHandWritingIconFeatureEnabled = lazyFetchHandWritingIconFeatureEnabled;
        mIconFetched = false;
        if (!mLazyFetchHandWritingIconFeatureEnabled) {
            int iconType = getHandler().getStylusPointerIcon();
            if (iconType != PointerIcon.TYPE_NULL) {
                mHandwritingIcon =
                        PointerIcon.getSystemIcon(context, getHandler().getStylusPointerIcon());
            }
        }
    }

    @MainThread
    private @Nullable PointerIcon getHandwritingIcon() {
        if (!mIconFetched) {
            int iconType = getHandler().getStylusPointerIcon();
            if (iconType != PointerIcon.TYPE_NULL) {
                mHandwritingIcon = PointerIcon.getSystemIcon(mContext, iconType);
            }
            mIconFetched = true;
        }

        return mHandwritingIcon;
    }

    /**
     * Determine the handler type that is needed.
     *
     * <p>This can be somewhat expensive, requiring multiple binder calls.
     *
     * <p>May be run on a background thread.
     */
    @AnyThread
    private static @HandlerType int computeHandlerType(Context context) {
        try (TraceEvent e = TraceEvent.scoped("StylusWritingController.computeHandlerType")) {
            if (DirectWritingSettingsHelper.isEnabled(context)) {
                // Lazily initialize the various handlers since a lot of the time only one will be
                // used.
                return HandlerType.DIRECT_WRITING_TRIGGER;
            }

            // The check for Android T is already in isEnabled but we are adding it here too to make
            // lint happy.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
                    && AndroidStylusWritingHandler.isEnabled(context)) {
                return HandlerType.ANDROID_HANDLER;
            }

            return HandlerType.DISABLED_STYLUS_WRITING_HANDLER;
        }
    }

    /**
     * Determine and cache the handler type that is needed.
     *
     * <p>May be run on a background thread to pre-cache the value.
     */
    @AnyThread
    private @HandlerType int getHandlerType(boolean refresh) {
        if (!StylusHandwritingFeatureMap.isEnabledOrDefault(
                StylusHandwritingFeatureMap.CACHE_STYLUS_SETTINGS, false)) {
            return computeHandlerType(mContext);
        }

        synchronized (mLock) {
            if (mHandlerType == HandlerType.UNSET || refresh) {
                mHandlerType = computeHandlerType(mContext);
            }
            return mHandlerType;
        }
    }

    /**
     * Returns the appropriate StylusWritingHandler - this may change at runtime if the user
     * enables/disables the stylus writing feature in their Android settings.
     */
    @MainThread
    private StylusApiOption chooseHandler() {
        try (TraceEvent e = TraceEvent.scoped("StylusWritingController.chooseHandler")) {
            switch (getHandlerType(/* refresh= */ false)) {
                case HandlerType.DIRECT_WRITING_TRIGGER:
                    if (mDirectWritingTrigger == null) {
                        mDirectWritingTrigger = new DirectWritingTrigger();
                    }
                    return mDirectWritingTrigger;
                case HandlerType.ANDROID_HANDLER:
                    // This case isn't reachable unless we're on Android T, but we need to make the
                    // lint happy.
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                        if (mAndroidHandler == null) {
                            mAndroidHandler = new AndroidStylusWritingHandler(mContext);
                        }
                        return mAndroidHandler;
                    }
                    break;
            }

            if (mDisabledStylusWritingHandler == null) {
                mDisabledStylusWritingHandler = new DisabledStylusWritingHandler();
            }
            return mDisabledStylusWritingHandler;
        }
    }

    /*
     * Returns the currently selected handler, initializing it lazily if it has not been initialized
     * already.
     */
    @MainThread
    private StylusApiOption getHandler() {
        // If the feature is enabled, we listen to settings changes and re-run the handler selection
        // logic if stylus related settings changed. If the feature is disabled, we re-run the
        // handler selection logic every time.
        if (StylusHandwritingFeatureMap.isEnabledOrDefault(
                StylusHandwritingFeatureMap.CACHE_STYLUS_SETTINGS, false)) {
            if (mStylusHandler == null) {
                mStylusHandler = chooseHandler();
            }
            return mStylusHandler;
        }
        return chooseHandler();
    }

    /**
     * Determining the handler type for the first time can be an expensive blocking operation taking
     * multiple milliseconds. This wrapper can be used to pre-cache the handler type on a background
     * thread if needed before continuing work on the UI thread.
     *
     * <p>Tasks should prefer to use instance state rather than captured state, as posted tasks may
     * otherwise operate on stale data.
     *
     * <p>Must be called from the UI thread.
     */
    @MainThread
    private void probeSupportThenRunOrPost(boolean refresh, Runnable task) {
        boolean background =
                StylusHandwritingFeatureMap.isEnabledOrDefault(
                        StylusHandwritingFeatureMap.PROBE_STYLUS_WRITING_IN_BACKGROUND, false);
        boolean cache =
                StylusHandwritingFeatureMap.isEnabledOrDefault(
                        StylusHandwritingFeatureMap.CACHE_STYLUS_SETTINGS, false);
        if (background && cache) {
            // Note that multiple precache tasks could be posted in a race, but the underlying work
            // is guarded by locks and the caching will avoid any expensive duplicate work.
            PostTask.postTask(
                    TaskTraits.USER_VISIBLE_MAY_BLOCK,
                    () -> {
                        getHandlerType(refresh);
                        PostTask.postTask(
                                TaskTraits.UI_USER_VISIBLE,
                                () -> {
                                    if (refresh) {
                                        mStylusHandler = null;
                                    }
                                    task.run();
                                });
                    });
        } else {
            getHandlerType(refresh);
            if (refresh) {
                mStylusHandler = null;
            }
            task.run();
        }
    }

    /**
     * Notifies the applicable {@link StylusWritingHandler} so that stylus writing messages can be
     * received and handled for performing handwriting recognition.
     *
     * @param webContents current web contents
     */
    @MainThread
    public void onWebContentsChanged(WebContents webContents) {
        if (webContents.getViewAndroidDelegate() == null) return;

        mCurrentWebContents = webContents;

        probeSupportThenRunOrPost(
                /* refresh= */ false,
                () -> {
                    if (mCurrentWebContents == null) return;
                    if (mCurrentWebContents.getViewAndroidDelegate() == null) return;

                    StylusApiOption handler = getHandler();
                    handler.onWebContentsChanged(mContext, mCurrentWebContents);

                    mCurrentWebContents
                            .getViewAndroidDelegate()
                            .setShouldShowStylusHoverIconCallback(
                                    this::setShouldOverrideStylusHoverIcon);
                });
    }

    /**
     * Notify window focus has changed
     *
     * @param hasFocus whether window gained or lost focus
     */
    @MainThread
    public void onWindowFocusChanged(boolean hasFocus) {
        // This notification is used to determine if the Stylus writing feature is enabled or not
        // from System settings as it can be changed while Chrome is in background. If caching of
        // stylus settings is enabled, we need to store the current focus state and send it when
        // settings change is observed.
        mIsWindowFocused = hasFocus;
        probeSupportThenRunOrPost(
                /* refresh= */ false,
                () -> {
                    updateStylusState();
                });
    }

    /** Notify stylus related settings changed. */
    @MainThread
    public void onSettingsChange() {
        probeSupportThenRunOrPost(
                /* refresh= */ true,
                () -> {
                    updateStylusState();
                });
    }

    @MainThread
    public @Nullable PointerIcon resolvePointerIcon() {
        if (mShouldOverrideStylusHoverIcon) {
            return mLazyFetchHandWritingIconFeatureEnabled
                    ? getHandwritingIcon()
                    : mHandwritingIcon;
        }
        return null;
    }

    @MainThread
    private void setShouldOverrideStylusHoverIcon(boolean shouldOverride) {
        mShouldOverrideStylusHoverIcon = shouldOverride;
    }

    @MainThread
    private void updateStylusState() {
        StylusApiOption handler = getHandler();
        handler.updateHandlerState(mContext, mIsWindowFocused);

        if (mCurrentWebContents == null) return;
        handler.onWebContentsChanged(mContext, mCurrentWebContents);
        if (mCurrentWebContents.getViewAndroidDelegate() == null) return;
        mCurrentWebContents
                .getViewAndroidDelegate()
                .setShouldShowStylusHoverIconCallback(this::setShouldOverrideStylusHoverIcon);
    }
}
