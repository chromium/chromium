// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes.models;

import android.view.View;

/** Model class for a template's background. */
public interface Background {
    // Draws the background onto |view|'s background with |cornerRadius|.
    void apply(View view, float cornerRadius);
}
