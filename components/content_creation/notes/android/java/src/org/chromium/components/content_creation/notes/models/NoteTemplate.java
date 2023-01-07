// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes.models;

/**
 * Model class for note templates.
 */
public class NoteTemplate {
    /** Required. */
    public final int id;
    public final String localizedName;
    public final Background mainBackground;
    public final TextStyle textStyle;
    public final FooterStyle footerStyle;

    /** Optional. */
    public final Background contentBackground;

    /** Constructor. */
    public NoteTemplate(int id, String localizedName, Background mainBackground,
            Background contentBackground, TextStyle textStyle, FooterStyle footerStyle) {
        this.id = id;
        this.localizedName = localizedName;
        this.mainBackground = mainBackground;
        this.contentBackground = contentBackground;
        this.textStyle = textStyle;
        this.footerStyle = footerStyle;
    }
}
