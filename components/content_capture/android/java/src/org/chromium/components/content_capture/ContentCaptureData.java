// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.graphics.Rect;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

/** The class is Java's representative of components/content_capture/common/content_capture_data.h */
public class ContentCaptureData extends ContentCaptureDataBase {
    private String mValue;

    @CalledByNative
    @VisibleForTesting
    public static ContentCaptureData createContentCaptureData(
            Object parent, long id, String value, int x, int y, int width, int height) {
        ContentCaptureData data = new ContentCaptureData(id, value, x, y, width, height);
        if (parent != null) {
            ((ContentCaptureDataBase) parent).addChild(data);
        }
        return data;
    }

    private ContentCaptureData(long id, String value, int x, int y, int width, int height) {
        super(id, new Rect(x, y, x + width, y + height));
        mValue = value;
    }

    public String getValue() {
        return mValue;
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder(super.toString());
        sb.append(" value:");
        sb.append(mValue);
        return sb.toString();
    }

    @Override
    public String getText() {
        return getValue();
    }
}
