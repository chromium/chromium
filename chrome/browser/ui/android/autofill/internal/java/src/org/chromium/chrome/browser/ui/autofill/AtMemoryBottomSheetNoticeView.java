// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/** View for rendering personal context onboarding notice item in @memory bottom sheet list. */
@NullMarked
public class AtMemoryBottomSheetNoticeView extends LinearLayout {
    private TextViewWithClickableSpans mNoticeTextView;
    private View mNoticeOkButton;

    public AtMemoryBottomSheetNoticeView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mNoticeTextView = findViewById(R.id.notice_text);
        mNoticeOkButton = findViewById(R.id.notice_ok_button);
    }

    public void setOkClickListener(Runnable listener) {
        mNoticeOkButton.setOnClickListener(v -> listener.run());
    }

    public void setSettingsClickListener(Runnable listener) {
        Context context = getContext();
        String rawText = context.getString(R.string.at_memory_notice_text);
        ChromeClickableSpan settingsSpan =
                new ChromeClickableSpan(context, (widget) -> listener.run());

        SpannableString formattedText =
                SpanApplier.applySpans(
                        rawText, new SpanApplier.SpanInfo("<link>", "</link>", settingsSpan));

        mNoticeTextView.setText(formattedText);
        mNoticeTextView.setMovementMethod(LinkMovementMethod.getInstance());
    }
}
