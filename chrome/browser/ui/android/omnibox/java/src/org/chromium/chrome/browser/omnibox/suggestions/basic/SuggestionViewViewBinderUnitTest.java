// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertSame;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.text.style.StyleSpan;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Tests for {@link SuggestionViewViewBinder}.
 *
 * <p>TextView.setText always stores a fresh copy of the supplied CharSequence, so the identity of
 * getText() tells whether the last bind reached the TextView.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SuggestionViewViewBinderUnitTest {
    private PropertyModel mModel;
    private TwoLineSuggestionView mView;

    @Before
    public void setUp() {
        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        var resourceProvider = new OmniboxResourceProvider(context, BrandedColorScheme.APP_DEFAULT);
        mView =
                (TwoLineSuggestionView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.omnibox_basic_suggestion, null);
        mModel =
                new PropertyModel.Builder(SuggestionViewProperties.ALL_KEYS)
                        .with(SuggestionCommonProperties.RESOURCE_PROVIDER, resourceProvider)
                        .build();
        PropertyModelChangeProcessor.create(
                mModel, new BaseSuggestionView<>(mView), new SuggestionViewViewBinder());
    }

    private static SuggestionSpannable colored(String text, int color) {
        var span = new SuggestionSpannable(text);
        span.setSpan(
                new ForegroundColorSpan(color), 0, text.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        return span;
    }

    @Test
    public void line1_skipsSetText_whenReboundWithEqualSpannable() {
        mModel.set(SuggestionViewProperties.TEXT_LINE_1_TEXT, colored("query", Color.RED));
        CharSequence applied = mView.getLine1().getText();
        assertEquals("query", applied.toString());

        // A fresh-but-equal instance (what a rebuilt model list produces) is a no-op.
        mModel.set(SuggestionViewProperties.TEXT_LINE_1_TEXT, colored("query", Color.RED));
        assertSame(applied, mView.getLine1().getText());
    }

    @Test
    public void line1_setsText_whenTextOrSpansDiffer() {
        mModel.set(SuggestionViewProperties.TEXT_LINE_1_TEXT, colored("query", Color.RED));
        CharSequence first = mView.getLine1().getText();

        mModel.set(SuggestionViewProperties.TEXT_LINE_1_TEXT, colored("query2", Color.RED));
        CharSequence second = mView.getLine1().getText();
        assertNotSame(first, second);
        assertEquals("query2", second.toString());

        // Same text, different color span (e.g. theme change) must be re-applied.
        mModel.set(SuggestionViewProperties.TEXT_LINE_1_TEXT, colored("query2", Color.BLUE));
        assertNotSame(second, mView.getLine1().getText());
    }

    @Test
    public void line2_skipsSetText_whenReboundWithEqualSpannable() {
        mModel.set(SuggestionViewProperties.TEXT_LINE_2_TEXT, colored("example.com", Color.RED));
        CharSequence applied = mView.getLine2().getText();
        assertEquals("example.com", applied.toString());
        assertEquals(View.VISIBLE, mView.getLine2().getVisibility());

        mModel.set(SuggestionViewProperties.TEXT_LINE_2_TEXT, colored("example.com", Color.RED));
        assertSame(applied, mView.getLine2().getText());
        assertEquals(View.VISIBLE, mView.getLine2().getVisibility());
    }

    @Test
    public void line2_hiddenAndShownAgain_doesNotResetUnchangedText() {
        mModel.set(SuggestionViewProperties.TEXT_LINE_2_TEXT, colored("example.com", Color.RED));
        CharSequence applied = mView.getLine2().getText();

        mModel.set(SuggestionViewProperties.TEXT_LINE_2_TEXT, null);
        assertEquals(View.GONE, mView.getLine2().getVisibility());
        assertSame(applied, mView.getLine2().getText());

        // The view still holds the same text, so showing it again is free.
        mModel.set(SuggestionViewProperties.TEXT_LINE_2_TEXT, colored("example.com", Color.RED));
        assertEquals(View.VISIBLE, mView.getLine2().getVisibility());
        assertSame(applied, mView.getLine2().getText());
    }

    @Test
    public void line2_setsText_whenTextOrSpansDiffer() {
        mModel.set(SuggestionViewProperties.TEXT_LINE_2_TEXT, colored("example.com", Color.RED));
        CharSequence first = mView.getLine2().getText();

        mModel.set(SuggestionViewProperties.TEXT_LINE_2_TEXT, colored("example.org", Color.RED));
        CharSequence second = mView.getLine2().getText();
        assertNotSame(first, second);
        assertEquals("example.org", second.toString());
        assertEquals(View.VISIBLE, mView.getLine2().getVisibility());

        // Same text, different color span must be re-applied.
        mModel.set(SuggestionViewProperties.TEXT_LINE_2_TEXT, colored("example.org", Color.BLUE));
        assertNotSame(second, mView.getLine2().getText());
        assertEquals(View.VISIBLE, mView.getLine2().getVisibility());
    }

    @Test
    public void setTextIfChanged_comparesAgainstWhatTheViewActuallyShows() {
        // There is no cached copy of the bound text: if something else writes to the TextView,
        // the next equal bind is applied again rather than being skipped against stale state.
        TextView tv = mView.getLine1();
        SuggestionViewViewBinder.setTextIfChanged(tv, colored("query", Color.RED));
        tv.setText("something else");

        SuggestionViewViewBinder.setTextIfChanged(tv, colored("query", Color.RED));
        assertEquals("query", tv.getText().toString());
    }

    @Test
    public void setTextIfChanged_nullClearsText() {
        TextView tv = mView.getLine1();
        SuggestionViewViewBinder.setTextIfChanged(tv, colored("query", Color.RED));
        assertEquals("query", tv.getText().toString());

        SuggestionViewViewBinder.setTextIfChanged(tv, null);
        assertEquals("", tv.getText().toString());
    }

    @Test
    public void setTextIfChanged_mutatedSpannableIsNotMistakenForCurrentText() {
        // Mutating a spannable after binding cannot cause a skipped update, because the comparison
        // is against the TextView's own copy rather than the caller's instance.
        TextView tv = mView.getLine1();
        SuggestionSpannable bound = colored("query", Color.RED);
        SuggestionViewViewBinder.setTextIfChanged(tv, bound);
        CharSequence shown = tv.getText();
        bound.setSpan(new StyleSpan(Typeface.BOLD), 0, 2, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);

        SuggestionViewViewBinder.setTextIfChanged(tv, bound);
        assertNotSame(shown, tv.getText());
        assertEquals(1, ((Spanned) tv.getText()).getSpans(0, 5, StyleSpan.class).length);
    }
}
