// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.graphics.Rect;

import java.util.ArrayList;
import java.util.List;

/** The base class for ContentCaptureData and ContentCaptureFrame. */
public abstract class ContentCaptureDataBase {
    private final long mId;
    private Rect mBounds;
    private ArrayList<ContentCaptureDataBase> mChildren;

    public ContentCaptureDataBase(long id, Rect bounds) {
        mId = id;
        mBounds = bounds;
    }

    public Rect getBounds() {
        return mBounds;
    }

    public List<ContentCaptureDataBase> getChildren() {
        return mChildren;
    }

    public boolean hasChildren() {
        return mChildren != null && !mChildren.isEmpty();
    }

    public long getId() {
        return mId;
    }

    public void addChild(ContentCaptureDataBase data) {
        if (mChildren == null) mChildren = new ArrayList<ContentCaptureDataBase>();
        mChildren.add(data);
    }

    /** @return the text shall be set to ViewStructure.setText(). */
    public abstract String getText();

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append(" id:");
        sb.append(getId());
        sb.append(" bounds:");
        sb.append(getBounds());
        if (hasChildren()) {
            sb.append(" children:");
            sb.append(getChildren().size());
            for (ContentCaptureDataBase child : getChildren()) {
                sb.append(child.toString());
            }
        }
        return sb.toString();
    }
}
