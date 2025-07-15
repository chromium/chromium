// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.text;

import android.content.Context;
import android.text.TextPaint;
import android.text.TextUtils;
import android.text.TextUtils.TruncateAt;
import android.util.AttributeSet;
import android.widget.TextView;

import androidx.appcompat.widget.AppCompatTextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A {@link AppCompatTextView} that truncates content within a template, instead of truncating
 * the template text. Truncation only happens if maxLines is set to 1 and there's not enough space
 * to display the entire content.
 *
 * For example, given the following template and content
 *   Template: "%s was closed"
 *   Content: "https://www.google.com/webhp?sourceid=chrome-instant&q=potato"
 *
 * the TemplatePreservingTextView would truncate the content but not the template text:
 *   "https://www.google.com/webh... was closed"
 */
@NullMarked
public class TemplatePreservingTextView extends AppCompatTextView {
    private @Nullable String mTemplate;
    private CharSequence mContent = "";
    private @Nullable CharSequence mVisibleText;

    /**
     * Builds an instance of an {@link TemplatePreservingTextView}.
     * @param context A {@link Context} instance to build this {@link TextView} in.
     * @param attrs   An {@link AttributeSet} instance.
     */
    public TemplatePreservingTextView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Sets the template format string. setText() must be called after calling this method for the
     * new template text to take effect.
     *
     * @param template Template format string (e.g. "Closed %s"), or null. If null is passed, this
     *     view acts like a normal TextView.
     */
    public void setTemplate(@Nullable String template) {
        mTemplate = TextUtils.isEmpty(template) ? null : template;
    }

    /**
     * This will take {@code text} and apply it to the internal template, building a new
     * {@link String} to set.  This {@code text} will be automatically truncated to fit within
     * the template as best as possible, making sure the template does not get clipped.
     */
    @Override
    public void setText(CharSequence text, BufferType type) {
        mContent = text != null ? text : "";
        setContentDescription(mTemplate == null ? mContent : String.format(mTemplate, mContent));
        updateVisibleText(0, true);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        final int availWidth =
                MeasureSpec.getSize(widthMeasureSpec) - getPaddingLeft() - getPaddingRight();
        updateVisibleText(
                availWidth, MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.UNSPECIFIED);
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    private CharSequence getTruncatedText(int availWidth) {
        final TextPaint paint = getPaint();

        // Calculate the width the template takes.
        final String emptyTemplate = String.format(mTemplate, "");
        final float emptyTemplateWidth = paint.measureText(emptyTemplate);

        // Calculate the available width for the content.
        final float contentWidth = Math.max(availWidth - emptyTemplateWidth, 0.f);

        // Ellipsize the content to the available width.
        CharSequence clipped = TextUtils.ellipsize(mContent, paint, contentWidth, TruncateAt.END);

        // Build the full string, and if it does not fit within availWidth, truncate it further.
        CharSequence fullString = String.format(mTemplate, clipped);
        boolean shouldClipFullString = (int) paint.measureText((String) fullString) > availWidth;
        return shouldClipFullString
                ? TextUtils.ellipsize(fullString, paint, availWidth, TruncateAt.END)
                : fullString;
    }

    private void updateVisibleText(int availWidth, boolean unspecifiedWidth) {
        CharSequence visibleText;
        if (mTemplate == null) {
            visibleText = mContent;
        } else if (getMaxLines() != 1 || unspecifiedWidth) {
            visibleText = String.format(mTemplate, mContent);
        } else {
            visibleText = getTruncatedText(availWidth);
        }

        if (!visibleText.equals(mVisibleText)) {
            mVisibleText = visibleText;

            // BufferType.SPANNABLE is required so that TextView.getIterableTextForAccessibility()
            // doesn't call our custom setText(). See crbug.com/449311
            super.setText(mVisibleText, BufferType.SPANNABLE);
        }
    }
}
