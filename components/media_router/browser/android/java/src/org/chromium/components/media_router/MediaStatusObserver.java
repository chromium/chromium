// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

/** Interface to subscribe to MediaStatus updates. */
public interface MediaStatusObserver {
    /**
     * Called when there is a MediaStatus update.
     * NOTE: At the moment, FlingingControllerBridge is the only implementer. Sending a
     * MediaStatusBridge directly reduces boilerplate code. If a second implementer
     * were added, adding a ChromeMediaStatus interface and using here instead might
     * make more sense.
     */
    public void onMediaStatusUpdate(MediaStatusBridge status);
}
