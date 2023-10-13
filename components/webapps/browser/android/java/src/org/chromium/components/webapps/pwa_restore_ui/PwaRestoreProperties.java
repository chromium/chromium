// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import android.view.View.OnClickListener;

import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Contains the properties that a pwa-restore {@link PropertyModel} can have.
 */
public class PwaRestoreProperties {
    /** Encapsulates the information about an app to show in the PWA Restore dialog. */
    public static class AppInfo {
        private final String mAppId;
        private final String mAppName;

        /**
         * @param appId the ID of the app.
         * @param appName the name of the app.
         */
        public AppInfo(String appId, String appName) {
            mAppId = appId;
            mAppName = appName;
        }

        public String appName() {
            return mAppName;
        }
    }

    /**
     * View states of the PWA Restore Bottom Sheet.
     */
    @IntDef({
            ViewState.PREVIEW,
            ViewState.VIEW_PWA_LIST,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ViewState {
        int PREVIEW = 0; // The introductory message.
        int VIEW_PWA_LIST = 1; // The page listing all the PWAs.
    }

    // PropertyKey indicating the view state of the bottom sheet:
    static final WritableIntPropertyKey VIEW_STATE = new WritableIntPropertyKey();

    // App list:
    static final WritableObjectPropertyKey<List<AppInfo>> APPS = new WritableObjectPropertyKey<>();

    // Simple labels:
    static final WritableObjectPropertyKey<String> PEEK_DESCRIPTION =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<String> PEEK_TITLE = new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<String> PEEK_BUTTON_LABEL =
            new WritableObjectPropertyKey<>();

    static final WritableObjectPropertyKey<String> EXPANDED_DESCRIPTION =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<String> EXPANDED_TITLE =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<String> DESELECT_BUTTON_LABEL =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<String> EXPANDED_BUTTON_LABEL =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<String> RECENT_APPS_TITLE =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<String> OLDER_APPS_TITLE =
            new WritableObjectPropertyKey<>();

    // Button handling:
    static final ReadableObjectPropertyKey<OnClickListener> BACK_BUTTON_ON_CLICK_CALLBACK =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<OnClickListener> REVIEW_BUTTON_ON_CLICK_CALLBACK =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<OnClickListener> DESELECT_BUTTON_ON_CLICK_CALLBACK =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<OnClickListener> RESTORE_BUTTON_ON_CLICK_CALLBACK =
            new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {
        VIEW_STATE,
        APPS,
        PEEK_DESCRIPTION,
        PEEK_TITLE,
        PEEK_BUTTON_LABEL,
        EXPANDED_DESCRIPTION,
        EXPANDED_TITLE,
        DESELECT_BUTTON_LABEL,
        EXPANDED_BUTTON_LABEL,
        RECENT_APPS_TITLE,
        OLDER_APPS_TITLE,
        BACK_BUTTON_ON_CLICK_CALLBACK,
        REVIEW_BUTTON_ON_CLICK_CALLBACK,
        DESELECT_BUTTON_ON_CLICK_CALLBACK,
        RESTORE_BUTTON_ON_CLICK_CALLBACK,
    };

    static PropertyModel createModel(Runnable onReviewClicked, Runnable onBackClicked,
            Runnable onDeselectClicked, Runnable onRestoreClicked) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(BACK_BUTTON_ON_CLICK_CALLBACK, v -> onBackClicked.run())
                .with(REVIEW_BUTTON_ON_CLICK_CALLBACK, v -> onReviewClicked.run())
                .with(DESELECT_BUTTON_ON_CLICK_CALLBACK, v -> onDeselectClicked.run())
                .with(RESTORE_BUTTON_ON_CLICK_CALLBACK, v -> onRestoreClicked.run())
                .build();
    }
}
