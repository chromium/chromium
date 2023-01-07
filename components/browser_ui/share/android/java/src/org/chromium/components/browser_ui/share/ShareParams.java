// Copyright 2017 The Chromium Authors
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

    /** A format to be used when sharing |mText|. */
    private final String mTextFormat;

    /** The URL of the page to be shared. */
    private String mUrl;

    /** The common MIME type of the files to be shared. A wildcard if they have differing types. */
    private final String mFileContentType;

    /** The list of Uris of the files to be shared. */
    private final ArrayList<Uri> mFileUris;

    /** The alt-text for the shared files. */
    private final ArrayList<String> mFileAltTexts;

    /** The Uri to the offline MHTML file to be shared. */
    private final Uri mOfflineUri;

    /** The Uri of the screenshot of the page to be shared. */
    private final Uri mScreenshotUri;

    /** The boolean result of link to text generation. */
    private final Boolean mLinkToTextSuccessful;

    /** The sharing hub preview text. */
    private final String mPreviewText;

    /** A format to be used when sharing |mPreviewText|. */
    private final String mPreviewTextFormat;

    /**
     * Optional callback to be called when user makes a choice. Will not be called if receiving a
     * response when the user makes a choice is not supported (on older Android versions).
     */
    private TargetChosenCallback mCallback;

    private ShareParams(WindowAndroid window, String title, String text, String textFormat,
            String url, @Nullable String fileContentType, @Nullable ArrayList<Uri> fileUris,
            @Nullable ArrayList<String> fileAltTexts, @Nullable Uri offlineUri,
            @Nullable Uri screenshotUri, @Nullable TargetChosenCallback callback,
            @Nullable Boolean linkToTextSuccessful, @Nullable String previewText,
            String previewTextFormat) {
        mWindow = window;
        mTitle = title;
        mText = text;
        mTextFormat = textFormat;
        mUrl = url;
        mFileContentType = fileContentType;
        mFileUris = fileUris;
        mFileAltTexts = fileAltTexts;
        mOfflineUri = offlineUri;
        mScreenshotUri = screenshotUri;
        mCallback = callback;
        mLinkToTextSuccessful = linkToTextSuccessful;
        mPreviewText = previewText;
        mPreviewTextFormat = previewTextFormat;
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
        String text = getText();
        String url = getUrl();

        if (TextUtils.isEmpty(url)) {
            return text;
        }
        if (TextUtils.isEmpty(text)) {
            return url;
        }
        // Concatenate text and URL with a space.
        return text + " " + url;
    }

    /**
     * @return The text to be shared in the format it is meant to be shared.
     */
    public String getText() {
        return mTextFormat == null ? mText : String.format(mTextFormat, mText);
    }

    /**
     * @return The text to be shared, but without additional formatting.
     */
    public String getRawText() {
        return mText;
    }

    /**
     * @return The URL of the page to be shared.
     */
    public String getUrl() {
        return mUrl;
    }

    /**
     * @param url set URL to be shared.
     */
    public void setUrl(String url) {
        mUrl = url;
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
     * @return The alt-texts related to the files to be shared.
     */
    @Nullable
    public ArrayList<String> getFileAltTexts() {
        return mFileAltTexts;
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

    /**
     * @return The boolean result of link to text generation.
     */
    @Nullable
    public Boolean getLinkToTextSuccessful() {
        return mLinkToTextSuccessful;
    }

    /**
     * @return The text to be shared in the format it is meant to be shared.
     */
    @Nullable
    public String getPreviewText() {
        return mPreviewTextFormat == null ? mPreviewText
                                          : String.format(mPreviewTextFormat, mPreviewText);
    }

    /** The builder for {@link ShareParams} objects. */
    public static class Builder {
        private WindowAndroid mWindow;
        private String mTitle;
        private String mText;
        private String mTextFormat;
        private String mUrl;
        private String mFileContentType;
        private ArrayList<Uri> mFileUris;
        private ArrayList<String> mFileAltTexts;
        private Uri mOfflineUri;
        private Uri mScreenshotUri;
        private TargetChosenCallback mCallback;
        private Boolean mLinkToTextSuccessful;
        private String mPreviewText;
        private String mPreviewTextFormat;

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
         * Sets the text to be shared and its format to be used before sharing it.
         */
        public Builder setText(@NonNull String text, @NonNull String format) {
            mTextFormat = format;
            return setText(text);
        }

        /**
         * Sets the sharing hub preview text.
         */
        public Builder setPreviewText(@NonNull String previewText, @NonNull String format) {
            mPreviewTextFormat = format;
            mPreviewText = previewText;
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
         * Sets the alt-texts associated with the files to be shared.
         */
        public Builder setFileAltTexts(@Nullable ArrayList<String> fileAltTexts) {
            mFileAltTexts = fileAltTexts;
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

        /**
         * Sets the boolean result of link to text generation.
         */
        public Builder setLinkToTextSuccessful(@Nullable Boolean linkToTextSuccessful) {
            mLinkToTextSuccessful = linkToTextSuccessful;
            return this;
        }

        /** @return A fully constructed {@link ShareParams} object. */
        public ShareParams build() {
            if (!TextUtils.isEmpty(mUrl)) {
                mUrl = DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(mUrl);
            }
            return new ShareParams(mWindow, mTitle, mText, mTextFormat, mUrl, mFileContentType,
                    mFileUris, mFileAltTexts, mOfflineUri, mScreenshotUri, mCallback,
                    mLinkToTextSuccessful, mPreviewText, mPreviewTextFormat);
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
