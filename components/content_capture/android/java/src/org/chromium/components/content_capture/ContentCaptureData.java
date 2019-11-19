// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.graphics.Rect;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;

import java.util.ArrayList;
import java.util.List;

/**
 * The class is Java's representative of components/content_capture/common/content_capture_data.h
 */
public class ContentCaptureData {
    private long mId;
    private String mValue;
    private Rect mBounds;
    private ArrayList<ContentCaptureData> mChildren;

    @CalledByNative
    @VisibleForTesting
    public static ContentCaptureData createContentCaptureData(
            Object parent, long id, String value, int x, int y, int width, int height) {
        ContentCaptureData data = new ContentCaptureData(id, value, x, y, width, height);
        if (parent != null) {
            ((ContentCaptureData) parent).addChild(data);
        }
        return data;
    }

    private ContentCaptureData(long id, String value, int x, int y, int width, int height) {
        mId = id;
        mValue = value;
        mBounds = new Rect(x, y, x + width, y + height);
    }

    public String getValue() {
        return mValue;
    }

    public Rect getBounds() {
        return mBounds;
    }

    public List<ContentCaptureData> getChildren() {
        return mChildren;
    }

    public boolean hasChildren() {
        return mChildren != null && !mChildren.isEmpty();
    }

    public long getId() {
        return mId;
    }

    private void addChild(ContentCaptureData data) {
        if (mChildren == null) mChildren = new ArrayList<ContentCaptureData>();
        mChildren.add(data);
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("id:");
        sb.append(mId);
        sb.append(" value:");
        sb.append(mValue);
        sb.append(" bounds:");
        sb.append(mBounds);
        sb.append('\n');
        if (hasChildren()) {
            sb.append("children:");
            sb.append(mChildren.size());
            for (ContentCaptureData child : mChildren) {
                sb.append(child.toString());
            }
        }
        return sb.toString();
    }
}
