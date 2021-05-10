// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes.models;

/**
 * Model class for note templates.
 */
public class NoteTemplate {
    /** The localized name of this template. */
    public final int id;
    public final String localizedName;
    public final Background mainBackground;
    public final TextStyle textStyle;
    public final FooterStyle footerStyle;

    /** Constructor. */
    public NoteTemplate(int id, String localizedName, Background mainBackground,
            TextStyle textStyle, FooterStyle footerStyle) {
        this.id = id;
        this.localizedName = localizedName;
        this.mainBackground = mainBackground;
        this.textStyle = textStyle;
        this.footerStyle = footerStyle;
    }
}
