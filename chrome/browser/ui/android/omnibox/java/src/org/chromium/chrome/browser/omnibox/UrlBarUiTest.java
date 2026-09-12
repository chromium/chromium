// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.core.IsEqual.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.ui.test.util.MockitoHelper.clearInvocations;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;

import androidx.annotation.Nullable;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.KeyUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.omnibox.TextSelection;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.Collections;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests that rely on UI rendering for UrlBar. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class UrlBarUiTest {
    @ClassRule
    public static final BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;
    private static FrameLayout sContentView;

    private UrlBar mUrlBar;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity = sActivityTestRule.getActivity();
                    sContentView = new FrameLayout(sActivity);
                    sContentView.setFocusable(true);
                    sContentView.setFocusableInTouchMode(true);
                    sContentView.setLayoutParams(
                            new ViewGroup.MarginLayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    sActivity
                                            .getResources()
                                            .getDimensionPixelSize(
                                                    R.dimen.control_container_height)));
                    sActivity.setContentView(sContentView);
                });
    }

    @Before
    public void setupTest() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sContentView.removeAllViews();
                    sActivity.getLayoutInflater().inflate(R.layout.url_bar, sContentView);
                    mUrlBar = (UrlBar) sContentView.getChildAt(0);
                    FrameLayout.LayoutParams layoutParams =
                            (LayoutParams) mUrlBar.getLayoutParams();
                    layoutParams.width = LayoutParams.MATCH_PARENT;
                    mUrlBar.setLayoutParams(layoutParams);
                    mUrlBar.onCreateInputConnection(new EditorInfo());
                });
    }

    private static void assertTextEquals(CharSequence a, CharSequence b) {
        assertTrue(a + " should match: " + b, TextUtils.equals(a, b));
    }

    private void waitForUrlBarLayout() {
        Runnable check =
                () -> {
                    Criteria.checkThat(mUrlBar.isLayoutRequested(), Matchers.is(false));
                    Criteria.checkThat(mUrlBar.isInLayout(), Matchers.is(false));
                };
        if (ThreadUtils.runningOnUiThread()) {
            CriteriaHelper.pollUiThreadNested(check);
        } else {
            CriteriaHelper.pollUiThread(check);
        }
    }

    private void updateUrlBarText(
            CharSequence text, @UrlBar.ScrollType int scrollType, int scrollIndex) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.setText(text);
                    mUrlBar.setScrollState(scrollType, scrollIndex, /* originChanged= */ false);
                });
        waitForUrlBarLayout();
    }

    private CharSequence getUrlText() {
        return ThreadUtils.runOnUiThreadBlocking(() -> mUrlBar.getText());
    }

    private CharSequence getVisibleTextPrefixHint() {
        return ThreadUtils.runOnUiThreadBlocking(() -> mUrlBar.getVisibleTextPrefixHint());
    }

    private float getEndOfUrlHorizontal() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mUrlBar.getLayout().getPrimaryHorizontal(mUrlBar.getText().length()));
    }

    private int getMeasuredWidth() {
        return ThreadUtils.runOnUiThreadBlocking(mUrlBar::getMeasuredWidth);
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testVisibleTextPrefixHint_ShortUrl() {
        String url = "www.test.com";
        updateUrlBarText(url, UrlBar.ScrollType.SCROLL_TO_TLD, url.length());

        assertThat(getEndOfUrlHorizontal(), Matchers.lessThan((float) getMeasuredWidth()));
        assertNull(getVisibleTextPrefixHint());
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testVisibleTextPrefixHint_ShortTld_LongPath() {
        final String domain = "www.test.com";
        final String path = "/" + TextUtils.join("", Collections.nCopies(500, "a"));
        updateUrlBarText(domain + path, UrlBar.ScrollType.SCROLL_TO_TLD, domain.length());

        assertThat(getEndOfUrlHorizontal(), Matchers.greaterThan((float) getMeasuredWidth()));

        CharSequence urlText = getUrlText();
        assertNull(getVisibleTextPrefixHint());

        // Append a string to the already long initial text and validate the prefix doesn't change.
        updateUrlBarText(
                getUrlText() + "bbbbbbbbbbbbbbbbbbbbbbb",
                UrlBar.ScrollType.SCROLL_TO_TLD,
                domain.length());
        final CharSequence prefixHint = getVisibleTextPrefixHint();
        assertNotNull(prefixHint);
        assertTrue(
                "Expected url text: '" + urlText + "' starts with " + prefixHint,
                TextUtils.indexOf(urlText, prefixHint) == 0);
        assertThat(prefixHint.length(), Matchers.lessThan(urlText.length()));

        // Append a character to just the hint prefix text and validate the prefix doesn't change.
        updateUrlBarText(prefixHint + "a", UrlBar.ScrollType.SCROLL_TO_TLD, domain.length());
        assertTextEquals(prefixHint, getVisibleTextPrefixHint());

        // Set the text to just the prefix text and ensure the hint remains unchanged.
        updateUrlBarText(prefixHint, UrlBar.ScrollType.SCROLL_TO_TLD, domain.length());
        assertTextEquals(prefixHint, getVisibleTextPrefixHint());

        // Set the text to be slightly shorter than the prefix, which will result in the text
        // being shorter than the visual viewport, and thus generate a null hint text.
        //
        // We subtract by 2 because an additional trailing char is added to the visible text to
        // account for rounding issues with text positioning.
        updateUrlBarText(
                TextUtils.substring(prefixHint, 0, prefixHint.length() - 2),
                UrlBar.ScrollType.SCROLL_TO_TLD,
                domain.length());
        assertNull(getVisibleTextPrefixHint());
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testVisibleTextPrefixHint_ShortTld_LongPath_WithRtl() {
        final String domain = "www.test.com";
        // Add a RTL character shortly after the TLD, so that it is visible.
        final String path = "/aت" + TextUtils.join("", Collections.nCopies(500, "a"));
        updateUrlBarText(domain + path, UrlBar.ScrollType.SCROLL_TO_TLD, domain.length());

        assertThat(getEndOfUrlHorizontal(), Matchers.greaterThan((float) getMeasuredWidth()));

        // Assert null visible hint when there is RTl text anywhere in the visible url
        final CharSequence prefixHint = getVisibleTextPrefixHint();
        assertNull(prefixHint);

        // Append a string to the already long initial text and validate the prefix doesn't change.
        updateUrlBarText(
                getUrlText() + "bbbbbbbbbbbbbbbbbbbbbbb",
                UrlBar.ScrollType.SCROLL_TO_TLD,
                domain.length());
        assertNull(prefixHint);
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testVisibleTextPrefixHint_LongTld() throws Exception {
        final String domain = "www." + TextUtils.join("", Collections.nCopies(500, "a")) + ".com";
        updateUrlBarText(domain, UrlBar.ScrollType.SCROLL_TO_TLD, domain.length());

        final CharSequence urlText = getUrlText();
        CharSequence prefixHint = getVisibleTextPrefixHint();
        assertNotNull(prefixHint);
        assertTextEquals(urlText, prefixHint);

        updateUrlBarText(
                getUrlText() + "/foooooo", UrlBar.ScrollType.SCROLL_TO_TLD, domain.length());
        assertTextEquals(urlText, getVisibleTextPrefixHint());
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testVisibleTextPrefixHint_NonUrlText() {
        updateUrlBarText("a", UrlBar.ScrollType.SCROLL_TO_BEGINNING, 0);
        assertNull(getVisibleTextPrefixHint());

        updateUrlBarText(
                TextUtils.join("", Collections.nCopies(500, "a")),
                UrlBar.ScrollType.SCROLL_TO_BEGINNING,
                0);
        assertNull(getVisibleTextPrefixHint());
    }

    private static MotionEvent createMouseEvent(int action, float x, float y, int buttonState) {
        MotionEvent.PointerProperties pp = new MotionEvent.PointerProperties();
        pp.id = 0;
        pp.toolType = MotionEvent.TOOL_TYPE_MOUSE;
        MotionEvent.PointerCoords pc = new MotionEvent.PointerCoords();
        pc.x = x;
        pc.y = y;
        return MotionEvent.obtain(
                /* downTime= */ 0,
                /* eventTime= */ 0,
                action,
                /* pointerCount= */ 1,
                new MotionEvent.PointerProperties[] {pp},
                new MotionEvent.PointerCoords[] {pc},
                /* metaState= */ 0,
                buttonState,
                /* xPrecision= */ 1f,
                /* yPrecision= */ 1f,
                /* deviceId= */ 0,
                /* edgeFlags= */ 0,
                InputDevice.SOURCE_MOUSE,
                /* flags= */ 0);
    }

    private void rightClickAtOffset(int offset) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    float startX = mUrlBar.getLayout().getPrimaryHorizontal(offset);
                    float endX = mUrlBar.getLayout().getPrimaryHorizontal(offset + 1);
                    float x = mUrlBar.getTotalPaddingLeft() + (startX + endX) / 2f;
                    float y = mUrlBar.getHeight() / 2f;
                    MotionEvent evt =
                            createMouseEvent(
                                    MotionEvent.ACTION_DOWN, x, y, MotionEvent.BUTTON_SECONDARY);
                    mUrlBar.onTouchEvent(evt);
                });
    }

    private void selectAll() {
        ThreadUtils.runOnUiThreadBlocking(mUrlBar::selectAll);
    }

    private void setSelection(int start, int end) {
        ThreadUtils.runOnUiThreadBlocking(() -> mUrlBar.setSelection(start, end));
    }

    private void assertSelection(int expectedStart, int expectedEnd) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Selection start",
                            mUrlBar.getSelectionStart(),
                            Matchers.is(expectedStart));
                    Criteria.checkThat(
                            "Selection end", mUrlBar.getSelectionEnd(), Matchers.is(expectedEnd));
                });
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testFocusedRightClick_selectsWord() {
        updateUrlBarText("search google query", UrlBar.ScrollType.SCROLL_TO_BEGINNING, 0);
        requestFocus();

        rightClickAtOffset(9);

        // Selects "google" [7, 13).
        assertSelection(/* expectedStart= */ 7, /* expectedEnd= */ 13);
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testFocusedRightClick_insideSelection_retainsSelection() {
        updateUrlBarText("search google query", UrlBar.ScrollType.SCROLL_TO_BEGINNING, 0);
        requestFocus();
        setSelection(/* start= */ 7, /* end= */ 13);

        rightClickAtOffset(9);

        // Retains selection of "google" [7, 13).
        assertSelection(/* expectedStart= */ 7, /* expectedEnd= */ 13);
    }

    private void requestFocus() {
        Runnable r =
                () -> {
                    Criteria.checkThat("UrlBar not shown.", mUrlBar.isShown(), Matchers.is(true));
                    Criteria.checkThat(
                            "UrlBar not focusable.", mUrlBar.isFocusable(), Matchers.is(true));
                    if (!mUrlBar.hasFocus()) mUrlBar.requestFocus();
                    Criteria.checkThat("UrlBar is focused.", mUrlBar.hasFocus(), Matchers.is(true));
                    if (mUrlBar.getInputConnection() == null) {
                        mUrlBar.onCreateInputConnection(new EditorInfo());
                    }
                    Criteria.checkThat(
                            "UrlBar InputConnection initialized.",
                            mUrlBar.getInputConnection(),
                            Matchers.notNullValue());
                };
        if (ThreadUtils.runningOnUiThread()) {
            r.run();
        } else {
            CriteriaHelper.pollUiThread(r);
        }
    }

    private void clearFocus() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.clearFocus();
                    sContentView.requestFocus();
                });
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(mUrlBar.hasFocus(), Matchers.is(false)));
    }

    private void setText(String userText) {
        requestFocus();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.setText(userText);
                    mUrlBar.setAutocompleteText(userText, "", null, null);
                });
        checkText(Matchers.equalTo(userText), null);
    }

    private void setAutocompleteText(String autocompleteText, @Nullable String additionalText) {
        requestFocus();
        AtomicReference<String> userText = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    userText.set(mUrlBar.getTextWithoutAutocomplete());
                    mUrlBar.setAutocompleteText(
                            userText.get(), autocompleteText, additionalText, null);
                });
        checkText(
                Matchers.equalTo(userText.get()),
                Matchers.equalTo(userText.get() + autocompleteText));
    }

    private void checkText(
            Matcher<String> textMatcher, @Nullable Matcher<String> autocompleteTextMatcher) {
        checkText(textMatcher, autocompleteTextMatcher, null);
    }

    private void checkText(
            Matcher<String> textMatcher,
            @Nullable Matcher<String> autocompleteTextMatcher,
            @Nullable Matcher<String> additionalTextMatcher) {
        checkText(
                textMatcher,
                autocompleteTextMatcher,
                additionalTextMatcher,
                /* autocompleteSelectionStart= */ null,
                /* autocompleteSelectionEnd= */ null);
    }

    private void checkText(
            Matcher<String> textMatcher,
            @Nullable Matcher<String> autocompleteTextMatcher,
            @Nullable Matcher<String> additionalTextMatcher,
            int autocompleteSelectionStart,
            int autocompleteSelectionEnd) {
        checkText(
                textMatcher,
                autocompleteTextMatcher,
                additionalTextMatcher,
                Matchers.is(autocompleteSelectionStart),
                Matchers.is(autocompleteSelectionEnd));
    }

    private void checkText(
            Matcher<String> textMatcher,
            @Nullable Matcher<String> autocompleteTextMatcher,
            @Nullable Matcher<String> additionalTextMatcher,
            @Nullable Matcher<Integer> autocompleteSelectionStart,
            @Nullable Matcher<Integer> autocompleteSelectionEnd) {
        Runnable check =
                () -> {
                    if (mUrlBar.hasFocus()) {
                        Criteria.checkThat(
                                "Text without autocomplete should match",
                                mUrlBar.getTextWithoutAutocomplete(),
                                textMatcher);

                        Criteria.checkThat(
                                "Unexpected Autocomplete state",
                                mUrlBar.hasAutocomplete(),
                                Matchers.is(autocompleteTextMatcher != null));

                        if (autocompleteTextMatcher != null) {
                            Criteria.checkThat(
                                    "Text with autocomplete should match",
                                    mUrlBar.getTextWithAutocomplete(),
                                    autocompleteTextMatcher);
                        }

                        if (additionalTextMatcher != null) {
                            String additionalText = mUrlBar.getAdditionalText();
                            Criteria.checkThat(
                                    "Additional Text should match",
                                    additionalText != null ? additionalText : "",
                                    additionalTextMatcher);
                        }

                        if (autocompleteSelectionStart != null) {
                            Criteria.checkThat(
                                    "Autocomplete Selection start",
                                    mUrlBar.getSelectionStart(),
                                    autocompleteSelectionStart);
                        }
                    } else {
                        Criteria.checkThat(mUrlBar.getText().toString(), textMatcher);
                    }
                };
        if (ThreadUtils.runningOnUiThread()) {
            check.run();
        } else {
            CriteriaHelper.pollUiThread(check);
        }
    }

    private void typeText(String text, boolean execute) {
        requestFocus();
        KeyUtils.typeTextIntoView(InstrumentationRegistry.getInstrumentation(), mUrlBar, text);
        if (execute) sendKey(KeyEvent.KEYCODE_ENTER);
    }

    private void commitText(String textToCommit, boolean commitAsAutocomplete) {
        requestFocus();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InputConnection conn = getInputConnection();
                    assertNotNull(conn);
                    if (commitAsAutocomplete) conn.finishComposingText();
                    conn.commitText(textToCommit, 1);
                });
    }

    private void setComposingText(
            String composingText, int composingRegionStart, int composingRegionEnd) {
        requestFocus();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InputConnection conn = getInputConnection();
                    assertNotNull(conn);
                    conn.setComposingRegion(composingRegionStart, composingRegionEnd);
                    conn.setComposingText(
                            composingText, /* newCursorPosition= */ composingText.length());
                });
    }

    private InputConnection getInputConnection() {
        InputConnection conn = mUrlBar.getInputConnection();
        if (conn == null) {
            conn = mUrlBar.onCreateInputConnection(new EditorInfo());
        }
        return conn;
    }

    private void sendKey(int keyCode) {
        sendKey(keyCode, /* modifiers= */ 0);
    }

    private void sendKey(int keyCode, int modifiers) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    long currentTime = SystemClock.uptimeMillis();
                    var event =
                            new KeyEvent(
                                    /* downTime= */ currentTime,
                                    /* eventTime= */ currentTime,
                                    KeyEvent.ACTION_DOWN,
                                    keyCode,
                                    /* repeat= */ 0,
                                    modifiers);
                    if (!mUrlBar.dispatchKeyEventPreIme(event)) mUrlBar.dispatchKeyEvent(event);

                    event =
                            new KeyEvent(
                                    /* downTime= */ currentTime,
                                    /* eventTime= */ currentTime,
                                    KeyEvent.ACTION_UP,
                                    keyCode,
                                    /* repeat= */ 0,
                                    modifiers);
                    if (!mUrlBar.dispatchKeyEventPreIme(event)) mUrlBar.dispatchKeyEvent(event);
                });
    }

    private void setUrlDirectionListener(@Nullable Callback<Integer> listener) {
        ThreadUtils.runOnUiThreadBlocking(() -> mUrlBar.setUrlDirectionListener(listener));
    }

    private void setTextAndVerifyTextDirection(String text, int expectedDirection)
            throws TimeoutException {
        CallbackHelper directionCallback = new CallbackHelper();
        setUrlDirectionListener(
                (direction) -> {
                    if (direction == expectedDirection) {
                        directionCallback.notifyCalled();
                    }
                });
        setText(text);
        directionCallback.waitForOnly(
                "Direction never reached expected direction: " + expectedDirection);
        assertUrlDirection(expectedDirection);
        setUrlDirectionListener(null);
    }

    private void assertUrlDirection(int expectedDirection) {
        int actualDirection = ThreadUtils.runOnUiThreadBlocking(() -> mUrlBar.getUrlDirection());
        assertEquals(expectedDirection, actualDirection);
    }

    private void replaceText(int start, int end, String replacement) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mUrlBar.setText(mUrlBar.getText().replace(start, end, replacement)));
        waitForUrlBarLayout();
    }

    private void performBatchEdit(Callback<InputConnection> action) {
        requestFocus();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InputConnection conn = getInputConnection();
                    assertNotNull(conn);
                    conn.beginBatchEdit();
                    action.onResult(conn);
                    conn.endBatchEdit();
                });
    }

    private void copySelection() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.setSelection(0, mUrlBar.getText().length());
                    mUrlBar.onTextContextMenuItem(android.R.id.copy);
                });
    }

    private void setTextContextMenuDelegate(
            @Nullable UrlBar.UrlBarTextContextMenuDelegate delegate) {
        ThreadUtils.runOnUiThreadBlocking(() -> mUrlBar.setTextContextMenuDelegate(delegate));
    }

    private void showContextMenu() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mUrlBar.showContextMenu(mUrlBar.getWidth() / 2f, mUrlBar.getHeight() / 2f));
    }

    private void waitForContextMenuShown() {
        CriteriaHelper.pollUiThread(
                () -> {
                    UrlBarContextMenuHelper helper = mUrlBar.getContextMenuHelperForTesting();
                    Criteria.checkThat(
                            "Helper should not be null", helper, Matchers.notNullValue());
                    Criteria.checkThat(
                            "ListMenu should not be empty",
                            helper.getModelListForTesting().size(),
                            Matchers.greaterThan(0));
                });
    }

    private void dismissContextMenu() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UrlBarContextMenuHelper helper = mUrlBar.getContextMenuHelperForTesting();
                    if (helper != null) {
                        helper.destroy();
                    }
                });
    }

    private String getClipboardText() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ClipboardManager clipboard =
                            (ClipboardManager)
                                    mUrlBar.getContext()
                                            .getSystemService(Context.CLIPBOARD_SERVICE);
                    ClipData clip = clipboard.getPrimaryClip();
                    if (clip != null && clip.getItemCount() > 0) {
                        return clip.getItemAt(0).getText().toString();
                    }
                    return "";
                });
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testRefocusing() {
        for (int i = 0; i < 5; i++) {
            requestFocus();
            clearFocus();
        }
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    @UiThreadTest
    public void testAutocompleteUpdatedOnSetText() {
        // Verify that setting a new string will clear the autocomplete.
        setText("test");
        setAutocompleteText("ing is fun", null);

        // Replace part of the non-autocomplete text
        setText("test");
        setAutocompleteText("ing is fun", null);
        replaceText(/* start= */ 1, /* end= */ 2, "a");
        checkText(equalTo("tast"), /* autocompleteTextMatcher= */ null);

        // Replace part of the autocomplete text.
        setText("test");
        setAutocompleteText("ing is fun", null);
        replaceText(/* start= */ 8, /* end= */ 10, "no");
        checkText(equalTo("test"), /* autocompleteTextMatcher= */ null);
    }

    /**
     * Ensure that we send cursor position with autocomplete requests.
     *
     * <p>When reading this test, it helps to remember that autocomplete requests are not sent with
     * the user simply moves the cursor. They're only sent on text modifications.
     */
    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testSendCursorPosition() throws TimeoutException {
        requestFocus();
        final CallbackHelper autocompleteHelper = new CallbackHelper();
        final AtomicInteger cursorPositionUsed = new AtomicInteger();
        mUrlBar.setTextChangeListener(
                (textWithoutAutocomplete) -> {
                    int cursorPosition =
                            mUrlBar.getSelectionEnd() == mUrlBar.getSelectionStart()
                                    ? mUrlBar.getSelectionStart()
                                    : -1;
                    cursorPositionUsed.set(cursorPosition);
                    autocompleteHelper.notifyCalled();
                });

        // User types "a".
        // Omnibox: a|
        typeText("a", /* execute= */ false);
        autocompleteHelper.waitForCallback(0);
        assertEquals(1, cursorPositionUsed.get());

        // Keyboard autocompletes "cd".
        // Omnibox: acd|
        commitText("cd", /* commitAsAutocomplete= */ true);
        autocompleteHelper.waitForCallback(1);
        assertEquals(3, cursorPositionUsed.get());

        // User moves the cursor.
        sendKey(KeyEvent.KEYCODE_DPAD_LEFT);
        sendKey(KeyEvent.KEYCODE_DPAD_LEFT);

        // Omnibox: a|cd.
        // No new events sent - cursor position movements don't count as autocomplete events.
        assertEquals(2, autocompleteHelper.getCallCount());
        assertEquals(3, cursorPositionUsed.get());

        // User appends "b"
        // Omnibox: ab|cd.
        typeText("b", /* execute= */ false);
        autocompleteHelper.waitForCallback(2);
        assertEquals(2, cursorPositionUsed.get());

        // User deletes "b"
        // Omnibox text: a|cd
        sendKey(KeyEvent.KEYCODE_DEL);
        autocompleteHelper.waitForCallback(3);
        assertEquals(1, cursorPositionUsed.get());

        // User deletes "a"
        // Omnibox text: |cd
        sendKey(KeyEvent.KEYCODE_DEL);
        autocompleteHelper.waitForCallback(4);
        assertEquals(0, cursorPositionUsed.get());

        mUrlBar.setTextChangeListener(null);
    }

    /**
     * Ensure that we allow inline autocomplete when the text gets shorter but is not an explicit
     * delete action by the user.
     *
     * <p>If you focus the omnibox and there is the selected text "[about:blank]", then typing new
     * text should clear that entirely and allow autocomplete on the newly entered text.
     *
     * <p>If we assume deletes happen any time the text gets shorter, then this would be prevented.
     */
    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testAutocompleteAllowedWhenReplacingText() throws TimeoutException {
        setText("about:blank");
        selectAll();
        final String textToBeEntered = "c";

        final CallbackHelper autocompleteHelper = new CallbackHelper();
        final AtomicBoolean didPreventInlineAutocomplete = new AtomicBoolean();
        mUrlBar.setTextChangeListener(
                (textWithoutAutocomplete) -> {
                    if (!TextUtils.equals(textToBeEntered, mUrlBar.getTextWithoutAutocomplete())) {
                        return;
                    }
                    didPreventInlineAutocomplete.set(!mUrlBar.shouldAutocomplete());
                    autocompleteHelper.notifyCalled();
                    mUrlBar.setTextChangeListener(null);
                });

        typeText(textToBeEntered, /* execute= */ false);
        autocompleteHelper.waitForCallback(0);
        assertFalse(
                "Inline autocomplete incorrectly prevented.", didPreventInlineAutocomplete.get());
    }

    /**
     * Ensure that if the user deletes just the inlined autocomplete text that the suggestions are
     * regenerated.
     */
    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testSuggestionsUpdatedWhenDeletingInlineAutocomplete() throws TimeoutException {
        requestFocus();
        setText("test");
        setAutocompleteText("ing", null);

        final CallbackHelper autocompleteHelper = new CallbackHelper();
        final AtomicBoolean didPreventInlineAutocomplete = new AtomicBoolean();
        mUrlBar.setTextChangeListener(
                (textWithoutAutocomplete) -> {
                    if (!TextUtils.equals("test", mUrlBar.getTextWithoutAutocomplete())) return;
                    didPreventInlineAutocomplete.set(!mUrlBar.shouldAutocomplete());
                    autocompleteHelper.notifyCalled();
                    mUrlBar.setTextChangeListener(null);
                });

        sendKey(KeyEvent.KEYCODE_DEL);

        checkText(equalTo("test"), /* autocompleteTextMatcher= */ null);

        autocompleteHelper.waitForCallback(0);
        assertTrue(
                "Inline autocomplete incorrectly allowed after delete.",
                didPreventInlineAutocomplete.get());
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    @DisabledTest(message = "Disabled because of crbug.com/477262537")
    public void testAutocorrectionChangesTriggerCorrectSuggestions() {
        requestFocus();
        setComposingText("test", /* composingRegionStart= */ 0, /* composingRegionEnd= */ 4);
        setAutocompleteText("ing is fun", null);
        checkText(equalTo("test"), equalTo("testing is fun"));
        commitText("rest", /* commitAsAutocomplete= */ false);
        checkText(equalTo("rest"), /* autocompleteTextMatcher= */ null);
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testAutocompletionChangesTriggerCorrectSuggestions() {
        requestFocus();
        setComposingText("test", /* composingRegionStart= */ 0, /* composingRegionEnd= */ 4);
        setAutocompleteText("ing is fun", null);
        checkText(equalTo("test"), equalTo("testing is fun"));
        commitText("y", /* commitAsAutocomplete= */ true);
        checkText(equalTo("testy"), /* autocompleteTextMatcher= */ null);
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testAutocompleteCorrectlyPerservedOnBatchMode() {
        // Valid case (cursor at the end of text, single character, matches previous autocomplete).
        setText("g");
        setAutocompleteText("oogle.com", null);
        typeText("o", /* execute= */ false);
        checkText(equalTo("go"), equalTo("google.com"));

        // Invalid case (cursor not at the end of the text).
        setText("g");
        setAutocompleteText("oogle.com", null);
        performBatchEdit(
                conn -> {
                    conn.finishComposingText();
                    conn.commitText("o", 1);
                    conn.setSelection(0, 0);
                });
        checkText(equalTo("go"), /* autocompleteTextMatcher= */ null);

        // Invalid case (next character did not match previous autocomplete)
        setText("g");
        setAutocompleteText("oogle.com", null);
        typeText("a", /* execute= */ false);
        checkText(equalTo("ga"), /* autocompleteTextMatcher= */ null);

        // Multiple characters entered instead of 1.
        setText("g");
        setAutocompleteText("oogle.com", null);
        commitText("oogl", /* commitAsAutocomplete= */ true);
        checkText(equalTo("googl"), equalTo("google.com"));
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testAutocompleteSpanClearedOnNonMatchingCommitText() {
        requestFocus();
        setText("a");
        setAutocompleteText("mazon.com", null);
        checkText(equalTo("a"), equalTo("amazon.com"));

        typeText("l", /* execute= */ false);
        checkText(equalTo("al"), /* autocompleteTextMatcher= */ null);
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testAutocompleteClearedOnComposition() {
        requestFocus();
        setText("test");
        setAutocompleteText("ing is fun", null);

        setComposingText("ing compose", /* composingRegionStart= */ 4, /* composingRegionEnd= */ 4);
        checkText(equalTo("testing compose"), /* autocompleteTextMatcher= */ null);
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testDelayedCompositionCorrectedWithAutocomplete() {
        requestFocus();
        // Test with a single IME autocomplete
        typeText("chrome://f", /* execute= */ false);
        setAutocompleteText("lags", null);
        setComposingText("l", /* composingRegionStart= */ 13, /* composingRegionEnd= */ 14);
        checkText(equalTo("chrome://fl"), equalTo("chrome://flags"));

        // Test with > 1 characters in composition.
        setText("chrome://fl");
        setAutocompleteText("ags", null);
        checkText(equalTo("chrome://fl"), equalTo("chrome://flags"));
        setComposingText("fl", /* composingRegionStart= */ 12, /* composingRegionEnd= */ 14);
        checkText(equalTo("chrome://flfl"), /* autocompleteTextMatcher= */ null);

        // Test with non-matching composition. Should just append to the URL text.
        setText("chrome://f");
        setAutocompleteText("lags", null);
        checkText(equalTo("chrome://f"), equalTo("chrome://flags"));
        setComposingText("g", /* composingRegionStart= */ 13, /* composingRegionEnd= */ 14);
        checkText(equalTo("chrome://fg"), /* autocompleteTextMatcher= */ null);

        // Test with composition text that matches the entire text w/o autocomplete.
        setText("chrome://f");
        setAutocompleteText("lags", null);
        checkText(equalTo("chrome://f"), equalTo("chrome://flags"));
        setComposingText(
                "chrome://f", /* composingRegionStart= */ 13, /* composingRegionEnd= */ 14);
        checkText(equalTo("chrome://fchrome://f"), /* autocompleteTextMatcher= */ null);

        // Test with composition text longer than the URL text.
        // Shouldn't crash and should just append text.
        setText("chrome://f");
        setAutocompleteText("lags", null);
        checkText(equalTo("chrome://f"), equalTo("chrome://flags"));
        setComposingText(
                "blahblahblah", /* composingRegionStart= */ 13, /* composingRegionEnd= */ 14);
        checkText(equalTo("chrome://fblahblahblah"), /* autocompleteTextMatcher= */ null);
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    @DisabledTest(message = "Disabled because of b/333536371")
    public void testUrlTextChangeListener() {
        @SuppressWarnings("unchecked")
        Callback<String> listener = Mockito.mock(Callback.class);
        mUrlBar.setTextChangeListener(listener);

        setText("onomatop");
        Mockito.verify(listener).onResult("onomatop");

        // Setting autocomplete does not send a change update.
        setAutocompleteText("oeia", null);

        clearInvocations(listener);
        setText("");
        Mockito.verify(listener).onResult("");
        mUrlBar.setTextChangeListener(null);
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    @UiThreadTest
    public void testSetAutocompleteText_ShrinkingText() {
        setText("test");
        setAutocompleteText("ing is awesome", null);
        setAutocompleteText("ing is hard", null);
        setAutocompleteText("ingz", null);
        checkText(
                equalTo("test"),
                equalTo("testingz"),
                null,
                /* autocompleteSelectionStart= */ 4,
                /* autocompleteSelectionEnd= */ 8);
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    @UiThreadTest
    public void testSetAutocompleteTextWithAdditionalText_ShrinkingText() {
        setText("test");
        setAutocompleteText("ing is awesome", "www.foobar.com");
        setAutocompleteText("ing is hard", "www.bar.com");
        setAutocompleteText("ingz", "www.foo.com");
        checkText(
                equalTo("test"),
                equalTo("testingz"),
                equalTo("www.foo.com"),
                /* autocompleteSelectionStart= */ 4,
                /* autocompleteSelectionEnd= */ 8);
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    @UiThreadTest
    public void testSetAutocompleteText_GrowingText() {
        setText("test");
        setAutocompleteText("ingz", null);
        setAutocompleteText("ing is hard", null);
        setAutocompleteText("ing is awesome", null);
        checkText(
                equalTo("test"),
                equalTo("testing is awesome"),
                null,
                /* autocompleteSelectionStart= */ 4,
                /* autocompleteSelectionEnd= */ 18);
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    @UiThreadTest
    public void testSetAutocompleteTextWithAdditionalText_GrowingText() {
        setText("test");
        setAutocompleteText("ingz", "www.foo.com");
        setAutocompleteText("ing is hard", "www.bar.com");
        setAutocompleteText("ing is awesome", "www.foobar.com");
        checkText(
                equalTo("test"),
                equalTo("testing is awesome"),
                equalTo("www.foobar.com"),
                /* autocompleteSelectionStart= */ 4,
                /* autocompleteSelectionEnd= */ 18);
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    @UiThreadTest
    public void testSetAutocompleteText_DuplicateText() {
        setText("test");
        setAutocompleteText("ingz", null);
        setAutocompleteText("ingz", null);
        setAutocompleteText("ingz", null);
        checkText(
                equalTo("test"),
                equalTo("testingz"),
                null,
                /* autocompleteSelectionStart= */ 4,
                /* autocompleteSelectionEnd= */ 8);
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    @UiThreadTest
    public void testSetAutocompleteTextWithAdditionalText_DuplicateText() {
        setText("test");
        setAutocompleteText("ingz", "www.foo.com");
        setAutocompleteText("ingz", "www.foo.com");
        setAutocompleteText("ingz", "www.foo.com");
        checkText(
                equalTo("test"),
                equalTo("testingz"),
                equalTo("www.foo.com"),
                /* autocompleteSelectionStart= */ 4,
                /* autocompleteSelectionEnd= */ 8);
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testUrlDirection() throws TimeoutException {
        setTextAndVerifyTextDirection("ل", View.LAYOUT_DIRECTION_RTL);
        setTextAndVerifyTextDirection("a", View.LAYOUT_DIRECTION_LTR);
        setTextAndVerifyTextDirection("للك", View.LAYOUT_DIRECTION_RTL);
        setTextAndVerifyTextDirection("f", View.LAYOUT_DIRECTION_LTR);
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testCopyUrl_SchemePreservation() {
        String url = "https://www.foo.com/index.html";
        String expectedStripped = "www.foo.com/index.html";
        setText(expectedStripped);

        UrlBar.UrlBarTextContextMenuDelegate delegate =
                new UrlBar.UrlBarTextContextMenuDelegate() {
                    @Override
                    public @Nullable String getTextToPaste() {
                        return null;
                    }

                    @Override
                    public @Nullable String getReplacementCutCopyText(
                            String currentText, TextSelection selection) {
                        if (TextUtils.equals(currentText, expectedStripped)) {
                            return url;
                        }
                        return null;
                    }
                };
        setTextContextMenuDelegate(delegate);

        copySelection();
        assertEquals(url, getClipboardText());

        setText("");
        typeText("bar", /* execute= */ false);

        copySelection();
        assertEquals("bar", getClipboardText());

        setTextContextMenuDelegate(null);
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    public void testUrlBarContextMenu() {
        setText("test context menu");
        selectAll();
        showContextMenu();

        waitForContextMenuShown();
        dismissContextMenu();
    }
}
