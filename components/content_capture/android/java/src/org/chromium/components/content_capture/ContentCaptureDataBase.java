// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.graphics.Rect;

import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;

/** The base class for ContentCaptureData and ContentCaptureFrame. */
@NullMarked
public abstract class ContentCaptureDataBase {
    private final long mId;
    private final Rect mBounds;
    private @Nullable ArrayList<ContentCaptureDataBase> mChildren;

    public ContentCaptureDataBase(long id, Rect bounds) {
        mId = id;
        mBounds = bounds;
    }

    public Rect getBounds() {
        return mBounds;
    }

    public @Nullable List<ContentCaptureDataBase> getChildren() {
        return mChildren;
    }

    @EnsuresNonNullIf("mChildren")
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
            sb.append(mChildren.size());
            for (ContentCaptureDataBase child : mChildren) {
                sb.append(child.toString());
            }
        }
        return sb.toString();
    }
}
