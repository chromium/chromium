// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

/** An interface providing callbacks for {@link BaseMediaRouteDialogManager}. */
public interface MediaRouteDialogDelegate {
    /**
     * Notifies the delegate if the user has chosen a {@link MediaSink} to connect to.
     * onDialogDismissed() is not called in this case.
     * @param sourceUrn The source the sink was selected for.
     * @param sink The sink selected by the user.
     */
    void onSinkSelected(String sourceUrn, MediaSink sink);

    /**
     * Notifies the delegate if the user has closed the existing route.
     * @param mediaRouteId id of the media route that was closed.
     */
    void onRouteClosed(String mediaRouteId);

    /** Notifies the delegate if the dialog was dismissed without any user action. */
    void onDialogCancelled();
}
