// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;

/**
 * This class is used to specify the frame's session by a list of Frame ContentCaptureData from the
 * interested frame to root.
 */
@NullMarked
public class FrameSession extends ArrayList<ContentCaptureFrame> {
    /** @param length is reserved frame list length. */
    public FrameSession(int length) {
        super(length);
    }
}
