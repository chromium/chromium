// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.share;

import android.content.ComponentName;
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;

/**
 * A container object for passing share parameters to {@link ShareHelper}.
 */
public class ShareParams {
    /** The window that triggered the share action. */
    private final WindowAndroid mWindow;

    /** The title of the page to be shared. */
    private final String mTitle;

    /** The text to be shared. */
    private final String mText;

    /** The URL of the page to be shared. */
    private final String mUrl;

    /** The common MIME type of the files to be shared. A wildcard if they have differing types. */
    private final String mFileContentType;

    /** The list of Uris of the files to be shared. */
    private final ArrayList<Uri> mFileUris;

    /** The Uri to the offline MHTML file to be shared. */
    private final Uri mOfflineUri;

    /** The Uri of the screenshot of the page to be shared. */
    private final Uri mScreenshotUri;

    /**
     * Optional callback to be called when user makes a choice. Will not be called if receiving a
     * response when the user makes a choice is not supported (on older Android versions).
     */
    private TargetChosenCallback mCallback;

    private ShareParams(WindowAndroid window, String title, String text, String url,
            @Nullable String fileContentType, @Nullable ArrayList<Uri> fileUris,
            @Nullable Uri offlineUri, @Nullable Uri screenshotUri,
            @Nullable TargetChosenCallback callback) {
        mWindow = window;
        mTitle = title;
        mText = text;
        mUrl = url;
        mFileContentType = fileContentType;
        mFileUris = fileUris;
        mOfflineUri = offlineUri;
        mScreenshotUri = screenshotUri;
        mCallback = callback;
    }

    /**
     * @return The window that triggered share.
     */
    public WindowAndroid getWindow() {
        return mWindow;
    }

    /**
     * @return The title of the page to be shared.
     */
    public String getTitle() {
        return mTitle;
    }

    /**
     * @return The text concatenated with the url.
     */
    public String getTextAndUrl() {
        if (TextUtils.isEmpty(mUrl)) {
            return mText;
        }
        if (TextUtils.isEmpty(mText)) {
            return mUrl;
        }
        // Concatenate text and URL with a space.
        return mText + " " + mUrl;
    }

    /**
     * @return The text to be shared.
     */
    public String getText() {
        return mText;
    }

    /**
     * @return The URL of the page to be shared.
     */
    public String getUrl() {
        return mUrl;
    }

    /**
     * @return The MIME type to the arbitrary files to be shared.
     */
    @Nullable
    public String getFileContentType() {
        return mFileContentType;
    }

    /**
     * @return The Uri to the arbitrary files to be shared.
     */
    @Nullable
    public ArrayList<Uri> getFileUris() {
        return mFileUris;
    }

    /**
     * @return The Uri to the offline MHTML file to be shared.
     */
    @Nullable
    public Uri getOfflineUri() {
        return mOfflineUri;
    }

    /**
     * @return The Uri of the screenshot of the page to be shared.
     */
    @Nullable
    public Uri getScreenshotUri() {
        return mScreenshotUri;
    }

    /**
     * @return The callback to be called when user makes a choice.
     */
    @Nullable
    public TargetChosenCallback getCallback() {
        return mCallback;
    }

    /**
     * @param callback To be called when user makes a choice.
     */
    public void setCallback(@Nullable TargetChosenCallback callback) {
        mCallback = callback;
    }

    /** The builder for {@link ShareParams} objects. */
    public static class Builder {
        private WindowAndroid mWindow;
        private String mTitle;
        private String mText;
        private String mUrl;
        private String mFileContentType;
        private ArrayList<Uri> mFileUris;
        private Uri mOfflineUri;
        private Uri mScreenshotUri;
        private TargetChosenCallback mCallback;

        public Builder(@NonNull WindowAndroid window, @NonNull String title, @NonNull String url) {
            mWindow = window;
            mUrl = url;
            mTitle = title;
        }

        /**
         * Sets the text to be shared.
         */
        public Builder setText(@NonNull String text) {
            mText = text;
            return this;
        }

        /**
         * Sets the MIME type of the arbitrary files to be shared.
         */
        public Builder setFileContentType(@NonNull String fileContentType) {
            mFileContentType = fileContentType;
            return this;
        }

        /**
         * Sets the Uri of the arbitrary files to be shared.
         */
        public Builder setFileUris(@Nullable ArrayList<Uri> fileUris) {
            mFileUris = fileUris;
            return this;
        }

        /**
         * Sets the Uri of the offline MHTML file to be shared.
         */
        public Builder setOfflineUri(@Nullable Uri offlineUri) {
            mOfflineUri = offlineUri;
            return this;
        }

        /**
         * Sets the Uri of the screenshot of the page to be shared.
         */
        public Builder setScreenshotUri(@Nullable Uri screenshotUri) {
            mScreenshotUri = screenshotUri;
            return this;
        }

        /**
         * Sets the callback to be called when user makes a choice.
         */
        public Builder setCallback(@Nullable TargetChosenCallback callback) {
            mCallback = callback;
            return this;
        }

        /** @return A fully constructed {@link ShareParams} object. */
        public ShareParams build() {
            if (!TextUtils.isEmpty(mUrl)) {
                mUrl = DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(mUrl);
            }
            return new ShareParams(mWindow, mTitle, mText, mUrl, mFileContentType, mFileUris,
                    mOfflineUri, mScreenshotUri, mCallback);
        }
    }

    /**
     * Callback interface for when a target is chosen.
     */
    public static interface TargetChosenCallback {
        /**
         * Called when the user chooses a target in the share dialog.
         *
         * Note that if the user cancels the share dialog, this callback is never called.
         */
        public void onTargetChosen(ComponentName chosenComponent);

        /**
         * Called when the user cancels the share dialog.
         *
         * Guaranteed that either this, or onTargetChosen (but not both) will be called, eventually.
         */
        public void onCancel();
    }
}
