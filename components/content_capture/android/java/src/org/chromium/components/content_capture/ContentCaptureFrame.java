// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.graphics.Rect;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

/** The class is Java's representative of components/content_capture/browser/content_capture_frame.h */
public class ContentCaptureFrame extends ContentCaptureDataBase {
    private final String mUrl;
    private final String mTitle;
    private final String mFavicon;

    @CalledByNative
    @VisibleForTesting
    public static ContentCaptureFrame createContentCaptureFrame(
            long id,
            String value,
            int x,
            int y,
            int width,
            int height,
            String title,
            String favicon) {
        return new ContentCaptureFrame(id, value, x, y, width, height, title, favicon);
    }

    private ContentCaptureFrame(
            long id,
            String value,
            int x,
            int y,
            int width,
            int height,
            String title,
            String favicon) {
        super(id, new Rect(x, y, x + width, y + height));
        mUrl = value;
        mTitle = title;
        mFavicon = favicon;
    }

    public String getUrl() {
        return mUrl;
    }

    public String getTitle() {
        return mTitle;
    }

    public String getFavicon() {
        return mFavicon;
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder(super.toString());
        sb.append(" Url:");
        sb.append(getUrl());
        sb.append(" Title:");
        sb.append(getTitle());
        return sb.toString();
    }

    @Override
    public String getText() {
        return getTitle();
    }
}
