// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes.models;

/**
 * Model class for note templates.
 */
public class NoteTemplate {
    /** The localized name of this template. */
    public final String localizedName;

    /** Constructor. */
    public NoteTemplate(String localizedName) {
        this.localizedName = localizedName;
    }
}
