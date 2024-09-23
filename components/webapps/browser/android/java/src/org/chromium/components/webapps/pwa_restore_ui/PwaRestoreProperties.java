// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import android.graphics.Bitmap;
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

/** Contains the properties that a pwa-restore {@link PropertyModel} can have. */
public class PwaRestoreProperties {
    /** Encapsulates the information about an app to show in the PWA Restore dialog. */
    public static class AppInfo {
        private final String mAppId;
        private final String mAppName;
        private final Bitmap mAppIcon;
        private int mLastUsedDaysAgo;

        // Whether the app is selected or not.
        private boolean mSelected;

        /**
         * @param appId the ID of the app.
         * @param appName the name of the app.
         * @param appIcon the app icon.
         * @param lastUsedDaysAgo when the app was last used (days ago).
         */
        public AppInfo(String appId, String appName, Bitmap appIcon, int lastUsedDaysAgo) {
            mAppId = appId;
            mAppName = appName;
            mAppIcon = appIcon;
            mLastUsedDaysAgo = lastUsedDaysAgo;

            mSelected = true;
        }

        public String getId() {
            return mAppId;
        }

        public String getName() {
            return mAppName;
        }

        public Bitmap getIcon() {
            return mAppIcon;
        }

        public long getLastUsedDaysAgo() {
            return mLastUsedDaysAgo;
        }

        public boolean isSelected() {
            return mSelected;
        }

        public void toggleSelection() {
            mSelected = !mSelected;
        }
    }

    /** View states of the PWA Restore Bottom Sheet. */
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
    static final WritableObjectPropertyKey<List<AppInfo>> APPS =
            new WritableObjectPropertyKey<>(/* skipEquality= */ true);

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
    static final WritableObjectPropertyKey<Boolean> DESELECT_BUTTON_ENABLED =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<String> EXPANDED_BUTTON_LABEL =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<Boolean> EXPANDED_BUTTON_ENABLED =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<String> APPS_TITLE = new WritableObjectPropertyKey<>();

    // Button handling:
    static final ReadableObjectPropertyKey<OnClickListener> BACK_BUTTON_ON_CLICK_CALLBACK =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<OnClickListener> REVIEW_BUTTON_ON_CLICK_CALLBACK =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<OnClickListener> DESELECT_BUTTON_ON_CLICK_CALLBACK =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<OnClickListener> RESTORE_BUTTON_ON_CLICK_CALLBACK =
            new ReadableObjectPropertyKey<>();

    // Checkbox handling:
    static final ReadableObjectPropertyKey<OnClickListener> SELECTION_TOGGLE_CLICK_CALLBACK =
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
        DESELECT_BUTTON_ENABLED,
        EXPANDED_BUTTON_LABEL,
        EXPANDED_BUTTON_ENABLED,
        APPS_TITLE,
        BACK_BUTTON_ON_CLICK_CALLBACK,
        REVIEW_BUTTON_ON_CLICK_CALLBACK,
        DESELECT_BUTTON_ON_CLICK_CALLBACK,
        RESTORE_BUTTON_ON_CLICK_CALLBACK,
        SELECTION_TOGGLE_CLICK_CALLBACK,
    };

    static PropertyModel createModel(
            Runnable onReviewClicked,
            Runnable onBackClicked,
            Runnable onDeselectClicked,
            Runnable onRestoreClicked,
            OnClickListener onAppToggled) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(BACK_BUTTON_ON_CLICK_CALLBACK, v -> onBackClicked.run())
                .with(REVIEW_BUTTON_ON_CLICK_CALLBACK, v -> onReviewClicked.run())
                .with(DESELECT_BUTTON_ON_CLICK_CALLBACK, v -> onDeselectClicked.run())
                .with(RESTORE_BUTTON_ON_CLICK_CALLBACK, v -> onRestoreClicked.run())
                .with(SELECTION_TOGGLE_CLICK_CALLBACK, onAppToggled)
                .build();
    }
}
