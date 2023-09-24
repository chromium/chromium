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

/**
 * Contains the properties that a pwa-restore {@link PropertyModel} can have.
 */
public class PwaRestoreProperties {
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
    static final WritableObjectPropertyKey<String> EXPANDED_BUTTON_LABEL =
            new WritableObjectPropertyKey<>();

    // Button handling:
    static final ReadableObjectPropertyKey<OnClickListener> BACK_BUTTON_ON_CLICK_CALLBACK =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<OnClickListener> REVIEW_BUTTON_ON_CLICK_CALLBACK =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<OnClickListener> RESTORE_BUTTON_ON_CLICK_CALLBACK =
            new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {
            VIEW_STATE,
            PEEK_DESCRIPTION,
            PEEK_TITLE,
            PEEK_BUTTON_LABEL,
            EXPANDED_DESCRIPTION,
            EXPANDED_TITLE,
            EXPANDED_BUTTON_LABEL,
            BACK_BUTTON_ON_CLICK_CALLBACK,
            REVIEW_BUTTON_ON_CLICK_CALLBACK,
            RESTORE_BUTTON_ON_CLICK_CALLBACK,
    };

    static PropertyModel createModel(
            Runnable onReviewClicked, Runnable onBackClicked, Runnable onRestoreClicked) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(BACK_BUTTON_ON_CLICK_CALLBACK, v -> onBackClicked.run())
                .with(REVIEW_BUTTON_ON_CLICK_CALLBACK, v -> onReviewClicked.run())
                .with(RESTORE_BUTTON_ON_CLICK_CALLBACK, v -> onRestoreClicked.run())
                .build();
    }
}
