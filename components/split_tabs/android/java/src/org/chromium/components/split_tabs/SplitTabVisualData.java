// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.split_tabs;

import com.google.errorprone.annotations.DoNotMock;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Objects;

/** Java counterpart to native {@code split_tabs::SplitTabVisualData}. */
@NullMarked
@JNINamespace("split_tabs")
@DoNotMock("This is a simple value object.")
public final class SplitTabVisualData {
    /** Default proportional size of the first pane in a split tab (an equal 50/50 split). */
    public static final double DEFAULT_SPLIT_RATIO = 0.5;

    private final @SplitTabLayout int mSplitLayout;
    private final double mSplitRatio;

    /**
     * Constructs a {@link SplitTabVisualData}.
     *
     * @param splitLayout See {@link SplitTabLayout}.
     * @param splitRatio Proportional size of the first pane (between 0.0 and 1.0).
     */
    @CalledByNative
    public SplitTabVisualData(
            @JniType("split_tabs::SplitTabLayout") @SplitTabLayout int splitLayout,
            double splitRatio) {
        assert 0.0 <= splitRatio && splitRatio <= 1.0 : "Invalid splitRatio: " + splitRatio;

        mSplitLayout = splitLayout;
        mSplitRatio = splitRatio;
    }

    /** Returns the {@link SplitTabLayout}. */
    @CalledByNative
    public @JniType("split_tabs::SplitTabLayout") @SplitTabLayout int getSplitLayout() {
        return mSplitLayout;
    }

    /** Returns the proportional size of the first pane (between 0.0 and 1.0). */
    @CalledByNative
    public double getSplitRatio() {
        return mSplitRatio;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) {
            return true;
        }
        if (!(obj instanceof SplitTabVisualData other)) {
            return false;
        }
        return mSplitLayout == other.mSplitLayout
                && Double.compare(mSplitRatio, other.mSplitRatio) == 0;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mSplitLayout, mSplitRatio);
    }

    @Override
    public String toString() {
        return "SplitTabVisualData{layout=" + mSplitLayout + ", ratio=" + mSplitRatio + "}";
    }
}
