// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

import org.chromium.base.annotations.CalledByNative;

/**
 * Java representation for the native TriggerContext. Contains contextual information for a trigger
 * event.
 */
public class TriggerContext {
    @CalledByNative
    private static TriggerContext createTriggerContext() {
        return new TriggerContext();
    }

    public TriggerContext() {}
}
