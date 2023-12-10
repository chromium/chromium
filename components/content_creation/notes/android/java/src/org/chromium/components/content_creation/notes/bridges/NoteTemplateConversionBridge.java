// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes.bridges;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.components.content_creation.notes.models.Background;
import org.chromium.components.content_creation.notes.models.FooterStyle;
import org.chromium.components.content_creation.notes.models.HighlightStyle;
import org.chromium.components.content_creation.notes.models.ImageBackground;
import org.chromium.components.content_creation.notes.models.LinearGradientBackground;
import org.chromium.components.content_creation.notes.models.LinearGradientDirection;
import org.chromium.components.content_creation.notes.models.NoteTemplate;
import org.chromium.components.content_creation.notes.models.SolidBackground;
import org.chromium.components.content_creation.notes.models.TextAlignment;
import org.chromium.components.content_creation.notes.models.TextStyle;

import java.util.ArrayList;
import java.util.List;

/**
 * Bridge class in charge of creating Java NoteTemplate instances based on their
 * native struct counterpart.
 */
@JNINamespace("content_creation")
public class NoteTemplateConversionBridge {
    /**
     * Creates an empty Java List instance to be used in native.
     * @return a reference to an empty Java List.
     */
    @CalledByNative
    private static List<NoteTemplate> createTemplateList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static Background createBackground(@ColorInt int color) {
        return new SolidBackground(color);
    }

    @CalledByNative
    private static Background createLinearGradientBackground(
            @ColorInt int[] colors, int direction) {
        return new LinearGradientBackground(colors, LinearGradientDirection.fromInteger(direction));
    }

    @CalledByNative
    private static Background createImageBackground(String imageUrl) {
        return new ImageBackground(imageUrl);
    }

    @CalledByNative
    private static TextStyle createTextStyle(
            String fontName,
            @ColorInt int fontColor,
            int weight,
            boolean allCaps,
            int alignment,
            int minTextSizeSP,
            int maxTextSizeSP,
            @ColorInt int highlightColor,
            int highlightStyle) {
        return new TextStyle(
                fontName,
                fontColor,
                weight,
                allCaps,
                TextAlignment.fromInteger(alignment),
                minTextSizeSP,
                maxTextSizeSP,
                highlightColor,
                HighlightStyle.fromInteger(highlightStyle));
    }

    @CalledByNative
    private static FooterStyle createFooterStyle(@ColorInt int textColor, @ColorInt int logoColor) {
        return new FooterStyle(textColor, logoColor);
    }

    /**
     * Creates a {@link NoteTemplate} instance based on the given parameters,
     * and then attempts to add it to the given list.
     * @return the {@link NoteTemplate} instance.
     */
    @CalledByNative
    private static NoteTemplate createTemplateAndMaybeAddToList(
            @Nullable List<NoteTemplate> list,
            int id,
            String localizedName,
            Background mainBackground,
            Background contentBackground,
            TextStyle textStyle,
            FooterStyle footerStyle) {
        NoteTemplate template =
                new NoteTemplate(
                        id,
                        localizedName,
                        mainBackground,
                        contentBackground,
                        textStyle,
                        footerStyle);

        if (list != null) {
            list.add(template);
        }

        return template;
    }
}
