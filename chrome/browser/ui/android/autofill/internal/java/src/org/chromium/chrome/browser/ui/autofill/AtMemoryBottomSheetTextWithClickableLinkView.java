// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** View for text items with clickable link in the AtMemory bottom sheet (e.g. AI disclosure). */
@NullMarked
public class AtMemoryBottomSheetTextWithClickableLinkView extends FrameLayout {
    private TextView mTextView;

    public AtMemoryBottomSheetTextWithClickableLinkView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTextView = findViewById(R.id.text);
    }

    public void setText(String text, Runnable linkCallback) {
        assert text.contains("<link>") && text.contains("</link>")
                : "Text must contain <link> and </link> tags.";
        ChromeClickableSpan linkSpan =
                new ChromeClickableSpan(getContext(), (widget) -> linkCallback.run());
        SpannableString formattedText =
                SpanApplier.applySpans(
                        text, new SpanApplier.SpanInfo("<link>", "</link>", linkSpan));
        mTextView.setText(formattedText);
        mTextView.setMovementMethod(LinkMovementMethod.getInstance());
    }
}
