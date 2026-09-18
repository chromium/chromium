// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.styles;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
 * Adds the standard "New" badge to a string containing {@code <new>...</new>} markers.
 *
 * <p>Use this instead of building a badge yourself out of SuperscriptSpan. That raises the text
 * without reserving room for it, which clips the badge in Thai, Urdu and Devanagari. See
 * crbug.com/512323229.
 *
 * <p>The markers live in the string resource, so translators only ever see the word "New". Most
 * callers want:
 *
 * <pre>{@code
 * preference.setTitle(NewLabelUtils.withBadge(context, context.getString(R.string.my_title)));
 * }</pre>
 *
 * <p>Deciding whether the badge should show up at all is your job, not this class's. Gate it on the
 * Feature Engagement Tracker, a shared pref, or nothing. When it shouldn't show, call {@link
 * #withoutBadge} so the markers don't end up on screen.
 */
@NullMarked
public final class NewLabelUtils {
    private static final String START_MARKER = "<new>";
    private static final String END_MARKER = "</new>";

    private NewLabelUtils() {}

    /**
     * Returns {@code textWithMarkers} with the marked-up part styled as a "New" badge.
     *
     * @param context Used to look up the accent colour for the current theme.
     * @param textWithMarkers A localised string with one {@code <new>...</new>} pair in it.
     */
    public static CharSequence withBadge(Context context, String textWithMarkers) {
        return withBadge(context, textWithMarkers, NewLabelSpan.DEFAULT_RELATIVE_SIZE);
    }

    /**
     * Same as above, but with a badge size you pick.
     *
     * <p>Reach for the two-argument version first. This one is here for surfaces whose UX asked for
     * a smaller badge, so don't use it for something new without checking with UX.
     *
     * @param context Used to look up the accent colour for the current theme.
     * @param textWithMarkers A localised string with one {@code <new>...</new>} pair in it.
     * @param relativeTextSize Badge text size, relative to the text around it.
     */
    public static CharSequence withBadge(
            Context context, String textWithMarkers, float relativeTextSize) {
        return SpanApplier.applySpans(
                textWithMarkers,
                new SpanInfo(
                        START_MARKER,
                        END_MARKER,
                        new NewLabelSpan(
                                SemanticColorUtils.getDefaultTextColorAccent1(context),
                                relativeTextSize,
                                NewLabelSpan.DEFAULT_BASELINE_SHIFT_RATIO)));
    }

    /**
     * Returns {@code textWithMarkers} with the badge text taken out.
     *
     * <p>Some titles carry the markers in the string resource itself, so there's no plain version
     * to fall back on when the badge shouldn't show. This gives you one. No {@link Context} needed,
     * since nothing has to be coloured.
     *
     * @param textWithMarkers A localised string with one {@code <new>...</new>} pair in it.
     */
    public static CharSequence withoutBadge(String textWithMarkers) {
        return SpanApplier.removeSpanText(textWithMarkers, new SpanInfo(START_MARKER, END_MARKER));
    }
}
