// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.spy;

import android.app.Activity;
import android.content.Context;
import android.text.Editable;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.style.ForegroundColorSpan;
import android.util.AttributeSet;
import android.view.ContextThemeWrapper;
import android.view.KeyEvent;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.EditText;
import android.widget.LinearLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowAccessibilityManager;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.accessibility.AccessibilityState;

import java.util.Optional;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A robolectric test for {@link AutocompleteEditText} class. TODO(changwan): switch to
 * ParameterizedRobolectricTest once crbug.com/733324 is fixed.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AutocompleteEditTextTest {
    private static final String TAG = "AutocompleteTest";

    private static final boolean DEBUG = false;

    private InOrder mInOrder;
    private TestAutocompleteEditText mAutocomplete;
    private LinearLayout mFocusPlaceHolder;

    private Context mContext;
    private InputConnection mInputConnection;
    private Verifier mVerifier;
    private boolean mIsShown;

    /**
     * A flag to tweak test expectations to deal with an OS bug.
     *
     * <p>{@code EditableInputConnection}, which {@link AutocompleteEditText} internally rely on,
     * has had <a href="https://issuetracker.google.com/issues/209958658">a bug</a> that it still
     * returns {@code true} from {@link InputConnection#endBatchEdit()} when its internal batch edit
     * count becomes {@code 0} as a result of invocation, which clearly conflicted with the spec.
     * There are several tests in this file that are unfortunately affected by this bug. In order to
     * abstract out such an OS issue from the actual test expectations, we will dynamically test if
     * the bug still exists or not in the test execution environment or not, and set {@code true} to
     * this flag if it is still there.
     *
     * <p>Until a new version of Android OS with <a
     * href="https://android-review.googlesource.com/c/platform/frameworks/base/+/1923058">the
     * fix</a> and a corresponding version of Robolectric become available in Chromium, this flag is
     * expected to be always {@code true}. Once they become available, you can either remove this
     * flag with assuming it's always {@code false} or test two different OS behaviors at the same
     * time by <a href="http://robolectric.org/configuring/">specifying multiple SDK versions</a> to
     * the test runner</a>.
     *
     * @see #testEditableInputConnectionEndBatchEditBug(Context)
     * @see #assertLastBatchEdit(boolean)
     */
    private boolean mHasEditableInputConnectionEndBatchEditBug;

    /**
     * Test if {@code EditableInputConnection} has a bug that it still returns {@code true} from
     * {@link InputConnection#endBatchEdit()} when its internal batch edit count becomes {@code 0}
     * as a result of invocation.
     *
     * <p>See https://issuetracker.google.com/issues/209958658 for details.
     *
     * @param context The {@link Context} to be used to initialize {@link EditText}.
     * @return {@code true} if the bug still exists. {@code false} otherwise.
     */
    private static boolean testEditableInputConnectionEndBatchEditBug(Context context) {
        EditText editText = new EditText(context);
        EditorInfo editorInfo = new EditorInfo();
        InputConnection editableInputConnection = editText.onCreateInputConnection(editorInfo);
        editableInputConnection.beginBatchEdit();
        // If this returns true, yes, the bug is still there!
        return editableInputConnection.endBatchEdit();
    }

    /**
     * A convenient helper method to assert the return value of {@link
     * InputConnection#endBatchEdit()} when its internal batch edit count becomes {@code 0}.
     *
     * @param result The return value of {@link InputConnection#endBatchEdit()}.
     * @see #mHasEditableInputConnectionEndBatchEditBug
     * @see #testEditableInputConnectionEndBatchEditBug(Context)
     */
    private void assertLastBatchEdit(boolean result) {
        if (mHasEditableInputConnectionEndBatchEditBug) {
            assertTrue(result);
        } else {
            assertFalse(result);
        }
    }

    // Limits the target of InOrder#verify.
    private static class Verifier {
        public void onAutocompleteTextStateChanged(boolean updateDisplay) {
            if (DEBUG) Log.i(TAG, "onAutocompleteTextStateChanged(%b)", updateDisplay);
        }

        public void onUpdateSelection(int selStart, int selEnd) {
            if (DEBUG) Log.i(TAG, "onUpdateSelection(%d, %d)", selStart, selEnd);
        }

        public void onPopulateAccessibilityEvent(
                int eventType,
                String text,
                String beforeText,
                int itemCount,
                int fromIndex,
                int toIndex,
                int removedCount,
                int addedCount) {
            if (DEBUG) {
                Log.i(
                        TAG,
                        "onPopulateAccessibilityEvent: TYP[%d] TXT[%s] BEF[%s] CNT[%d] "
                                + "FROM[%d] TO[%d] REM[%d] ADD[%d]",
                        eventType,
                        text,
                        beforeText,
                        itemCount,
                        fromIndex,
                        toIndex,
                        removedCount,
                        addedCount);
            }
        }
    }

    private class TestAutocompleteEditText extends AutocompleteEditText {
        private static final String JAVASCRIPT_SCHEME = "javascript:";

        private AtomicInteger mVerifierCallCount = new AtomicInteger();
        private AtomicInteger mAccessibilityVerifierCallCount = new AtomicInteger();
        private AtomicReference<String> mKeyboardPackageName =
                new AtomicReference<>("placeholder.ime");

        public TestAutocompleteEditText(Context context, AttributeSet attrs) {
            super(context, attrs);
            if (DEBUG) Log.i(TAG, "TestAutocompleteEditText constructor");
        }

        @Override
        public void onAutocompleteTextStateChanged(boolean updateDisplay) {
            mVerifier.onAutocompleteTextStateChanged(updateDisplay);
            mVerifierCallCount.incrementAndGet();
        }

        @Override
        public void onUpdateSelectionForTesting(int selStart, int selEnd) {
            mVerifier.onUpdateSelection(selStart, selEnd);
            mVerifierCallCount.incrementAndGet();
        }

        @Override
        public void onPopulateAccessibilityEvent(AccessibilityEvent event) {
            super.onPopulateAccessibilityEvent(event);
            mVerifier.onPopulateAccessibilityEvent(
                    event.getEventType(),
                    getText(event),
                    getBeforeText(event),
                    event.getItemCount(),
                    event.getFromIndex(),
                    event.getToIndex(),
                    event.getRemovedCount(),
                    event.getAddedCount());
            mAccessibilityVerifierCallCount.incrementAndGet();
        }

        private String getText(AccessibilityEvent event) {
            if (event.getText() != null && event.getText().size() > 0) {
                return event.getText().get(0).toString();
            }
            return "";
        }

        private String getBeforeText(AccessibilityEvent event) {
            return event.getBeforeText() == null ? "" : event.getBeforeText().toString();
        }

        @Override
        public boolean isShown() {
            return mIsShown;
        }

        public int getAndResetVerifierCallCount() {
            return mVerifierCallCount.getAndSet(0);
        }

        public int getAndResetAccessibilityVerifierCallCount() {
            return mAccessibilityVerifierCallCount.getAndSet(0);
        }

        @Override
        public String getKeyboardPackageName() {
            return mKeyboardPackageName.get();
        }

        public void setKeyboardPackageName(String packageName) {
            mKeyboardPackageName.set(packageName);
        }

        @Override
        public String sanitizeTextForPaste(String s) {
            if (s.startsWith(JAVASCRIPT_SCHEME)) {
                s = s.substring(JAVASCRIPT_SCHEME.length());
            }
            return s;
        }
    }

    public AutocompleteEditTextTest() {
        if (DEBUG) ShadowLog.stream = System.out;
    }

    @Before
    public void setUp() {
        if (DEBUG) Log.i(TAG, "setUp started.");
        MockitoAnnotations.initMocks(this);
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        mHasEditableInputConnectionEndBatchEditBug =
                testEditableInputConnectionEndBatchEditBug(mContext);

        mVerifier = spy(new Verifier());
        mAutocomplete = new TestAutocompleteEditText(mContext, null);
        mFocusPlaceHolder = new LinearLayout(mContext);
        mFocusPlaceHolder.setFocusable(true);
        mFocusPlaceHolder.addView(mAutocomplete);
        assertNotNull(mAutocomplete);

        // Pretend that the view is shown in the activity hierarchy, which is for accessibility
        // testing.
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        activity.setContentView(mFocusPlaceHolder);
        assertNotNull(mFocusPlaceHolder.getParent());
        mIsShown = true;
        assertTrue(mAutocomplete.isShown());

        // Enable accessibility.
        ShadowAccessibilityManager manager =
                Shadows.shadowOf(
                        (AccessibilityManager)
                                mContext.getSystemService(Context.ACCESSIBILITY_SERVICE));
        manager.setEnabled(true);
        manager.setTouchExplorationEnabled(true);
        AccessibilityState.setIsPerformGesturesEnabledForTesting(true);
        AccessibilityState.setIsTouchExplorationEnabledForTesting(true);

        mInOrder = inOrder(mVerifier);
        assertTrue(mAutocomplete.requestFocus());
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_FOCUSED, "", "", 2, -1, -1, -1, -1);
        assertNotNull(mAutocomplete.onCreateInputConnection(new EditorInfo()));
        mInputConnection = mAutocomplete.getInputConnection();
        assertNotNull(mInputConnection);
        mInOrder.verifyNoMoreInteractions();
        assertVerifierCallCounts(0, 1);

        // Feeder should call this at the beginning.
        mAutocomplete.setIgnoreTextChangesForAutocomplete(false);

        if (DEBUG) Log.i(TAG, "setUp finished.");
    }

    private void assertTexts(String userText, String autocompleteText, String additionalText) {
        assertEquals(userText, mAutocomplete.getTextWithoutAutocomplete());
        assertEquals(userText + autocompleteText, mAutocomplete.getTextWithAutocomplete());
        assertEquals(autocompleteText.length(), mAutocomplete.getAutocompleteLength());
        assertEquals(!TextUtils.isEmpty(autocompleteText), mAutocomplete.hasAutocomplete());
        assertEquals(additionalText, mAutocomplete.getAdditionalText().orElse(""));
    }

    private void assertVerifierCallCounts(
            int nonAccessibilityCallCount, int accessibilityCallCount) {
        assertEquals(nonAccessibilityCallCount, mAutocomplete.getAndResetVerifierCallCount());
        assertEquals(
                accessibilityCallCount, mAutocomplete.getAndResetAccessibilityVerifierCallCount());
    }

    private void verifyOnPopulateAccessibilityEvent(
            int eventType,
            String text,
            String beforeText,
            int itemCount,
            int fromIndex,
            int toIndex,
            int removedCount,
            int addedCount) {
        mInOrder.verify(mVerifier)
                .onPopulateAccessibilityEvent(
                        eventType,
                        text,
                        beforeText,
                        itemCount,
                        fromIndex,
                        toIndex,
                        removedCount,
                        addedCount);
    }

    @Test
    public void testAppend_CommitText() {
        // User types "h".
        assertTrue(mInputConnection.commitText("h", 1));
        mInOrder.verify(mVerifier).onUpdateSelection(1, 1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "h", "", -1, 0, -1, 0, 1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "h", "", 1, 1, 1, -1, -1);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);

        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // The controller kicks in.
        mAutocomplete.setAutocompleteText("h", "ello world", Optional.empty());
        assertFalse(mAutocomplete.isCursorVisible());
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello world", "h", -1, 1, -1, 0, 10);
        assertVerifierCallCounts(0, 1);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // User types "he".
        assertTrue(mInputConnection.commitText("e", 1));
        mInOrder.verify(mVerifier).onUpdateSelection(2, 2);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                "hello world",
                "",
                11,
                2,
                2,
                -1,
                -1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello world", "he", -1, 2, -1, 0, 9);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("he", "llo world", Optional.empty());
        assertFalse(mAutocomplete.isCursorVisible());

        mInOrder.verifyNoMoreInteractions();
        assertTexts("he", "llo world", "");
        assertTrue(mAutocomplete.shouldAutocomplete());

        // User types "hello".
        assertTrue(mInputConnection.commitText("llo", 1));
        mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                "hello world",
                "",
                11,
                5,
                5,
                -1,
                -1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello world", "hello", -1, 5, -1, 0, 6);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world", Optional.empty());
        assertFalse(mAutocomplete.isCursorVisible());
        assertVerifierCallCounts(0, 0);
        mInOrder.verifyNoMoreInteractions();
        assertTexts("hello", " world", "");
        assertTrue(mAutocomplete.shouldAutocomplete());

        // User types a space inside a batch edit.
        assertTrue(mInputConnection.beginBatchEdit());
        // We should still show the intermediate autocomplete text to the user even in the middle of
        // a batch edit. Otherwise, the user may see flickering of autocomplete text.
        assertEquals("hello world", mAutocomplete.getText().toString());
        assertTrue(mInputConnection.commitText(" ", 1));
        assertEquals("hello world", mAutocomplete.getText().toString());
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertEquals("hello world", mAutocomplete.getText().toString());

        mInOrder.verifyNoMoreInteractions();
        assertLastBatchEdit(mInputConnection.endBatchEdit());

        // Autocomplete text gets redrawn.
        assertTexts("hello ", "world", "");
        assertTrue(mAutocomplete.shouldAutocomplete());
        mInOrder.verify(mVerifier).onUpdateSelection(6, 6);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                "hello world",
                "",
                11,
                6,
                6,
                -1,
                -1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                "hello world",
                "hello ",
                -1,
                6,
                -1,
                0,
                5);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);

        mAutocomplete.setAutocompleteText("hello ", "world", Optional.of("foo.com"));
        assertFalse(mAutocomplete.isCursorVisible());
        assertTexts("hello ", "world", "foo.com");
        assertVerifierCallCounts(0, 0);
        mInOrder.verifyNoMoreInteractions();
    }

    @Test
    public void testAppendWithAdditionalText_CommitText() {
        OmniboxFeatures.sRichInlineAutocomplete.setForTesting(true);
        OmniboxFeatures.sRichInlineShowFullUrl.setForTesting(true);
        OmniboxFeatures.sRichInlineMinimumInputChars.setForTesting(1);

        // User types "h".
        assertTrue(mInputConnection.commitText("h", 1));
        mInOrder.verify(mVerifier).onUpdateSelection(1, 1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "h", "", -1, 0, -1, 0, 1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "h", "", 1, 1, 1, -1, -1);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);

        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // The controller kicks in.
        mAutocomplete.setAutocompleteText("h", "ello world", Optional.of("www.foo.com"));
        assertFalse(mAutocomplete.isCursorVisible());
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                "hello world - www.foo.com",
                "h",
                -1,
                1,
                -1,
                0,
                10);
        assertVerifierCallCounts(0, 1);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // User types "he".
        assertTrue(mInputConnection.commitText("e", 1));
        mInOrder.verify(mVerifier).onUpdateSelection(2, 2);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                "hello world - www.foo.com",
                "",
                25,
                2,
                2,
                -1,
                -1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                "hello world - www.foo.com",
                "he",
                -1,
                2,
                -1,
                0,
                9);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("he", "llo world", Optional.of("www.bar.com"));
        assertFalse(mAutocomplete.isCursorVisible());

        mInOrder.verifyNoMoreInteractions();
        assertTexts("he", "llo world", "www.bar.com");
        assertTrue(mAutocomplete.shouldAutocomplete());

        // User types "hello".
        assertTrue(mInputConnection.commitText("llo", 1));
        mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                "hello world - www.bar.com",
                "",
                25,
                5,
                5,
                -1,
                -1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                "hello world - www.bar.com",
                "hello",
                -1,
                5,
                -1,
                0,
                6);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world", Optional.of("www.foobar.com"));
        assertFalse(mAutocomplete.isCursorVisible());
        assertVerifierCallCounts(0, 0);
        mInOrder.verifyNoMoreInteractions();
        assertTexts("hello", " world", "www.foobar.com");
        assertTrue(mAutocomplete.shouldAutocomplete());

        // User types a space inside a batch edit.
        assertTrue(mInputConnection.beginBatchEdit());
        // We should still show the intermediate autocomplete text to the user even in the middle of
        // a batch edit. Otherwise, the user may see flickering of autocomplete text.
        assertEquals("hello world - www.foobar.com", mAutocomplete.getText().toString());
        assertTrue(mInputConnection.commitText(" ", 1));
        assertEquals("hello world - www.foobar.com", mAutocomplete.getText().toString());
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertEquals("hello world - www.foobar.com", mAutocomplete.getText().toString());

        mInOrder.verifyNoMoreInteractions();
        assertLastBatchEdit(mInputConnection.endBatchEdit());

        // Autocomplete text gets redrawn.
        assertTexts("hello ", "world", "www.foobar.com");
        assertTrue(mAutocomplete.shouldAutocomplete());
        mInOrder.verify(mVerifier).onUpdateSelection(6, 6);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                "hello world - www.foobar.com",
                "",
                28,
                6,
                6,
                -1,
                -1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                "hello world - www.foobar.com",
                "hello ",
                -1,
                6,
                -1,
                0,
                5);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);

        mAutocomplete.setAutocompleteText("hello ", "world", Optional.of("www.foobar.com"));
        assertFalse(mAutocomplete.isCursorVisible());
        assertTexts("hello ", "world", "www.foobar.com");
        assertVerifierCallCounts(0, 0);
        mInOrder.verifyNoMoreInteractions();
    }

    @Test
    public void testAdditionalTextColor() {
        OmniboxFeatures.sRichInlineAutocomplete.setForTesting(true);
        OmniboxFeatures.sRichInlineShowFullUrl.setForTesting(true);
        OmniboxFeatures.sRichInlineMinimumInputChars.setForTesting(1);

        // User types "h".
        assertTrue(mInputConnection.commitText("h", 1));
        mInOrder.verify(mVerifier).onUpdateSelection(1, 1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "h", "", -1, 0, -1, 0, 1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "h", "", 1, 1, 1, -1, -1);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);

        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // The controller kicks in.
        mAutocomplete.setAutocompleteText("h", "ello world", Optional.of("www.foo.com"));
        assertFalse(mAutocomplete.isCursorVisible());
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                "hello world - www.foo.com",
                "h",
                -1,
                1,
                -1,
                0,
                10);
        assertVerifierCallCounts(0, 1);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        Editable editable = mAutocomplete.getEditableText();
        ForegroundColorSpan[] spans =
                editable.getSpans(0, editable.length(), ForegroundColorSpan.class);
        assertEquals(1, spans.length);
        assertEquals(
                SemanticColorUtils.getDefaultTextColorSecondary(mContext),
                spans[0].getForegroundColor());
    }

    @Test
    public void testAppendWithAdditionalText_noFullUrl() {
        OmniboxFeatures.sRichInlineAutocomplete.setForTesting(true);
        OmniboxFeatures.sRichInlineShowFullUrl.setForTesting(false);
        OmniboxFeatures.sRichInlineMinimumInputChars.setForTesting(1);

        // User types "h".
        assertTrue(mInputConnection.commitText("h", 1));
        mInOrder.verify(mVerifier).onUpdateSelection(1, 1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "h", "", -1, 0, -1, 0, 1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "h", "", 1, 1, 1, -1, -1);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);

        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // The controller kicks in.
        mAutocomplete.setAutocompleteText("h", "ello world", Optional.of("www.foo.com"));
        assertFalse(mAutocomplete.isCursorVisible());
        // "www.foo.com" is not shown since the show full URL parameter set to false.
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello world", "h", -1, 1, -1, 0, 10);
        assertVerifierCallCounts(0, 1);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
    }

    @Test
    public void testAppendWithAdditionalText_minimumCharacters() {
        OmniboxFeatures.sRichInlineAutocomplete.setForTesting(true);
        OmniboxFeatures.sRichInlineShowFullUrl.setForTesting(true);
        OmniboxFeatures.sRichInlineMinimumInputChars.setForTesting(4);

        // User types "h".
        assertTrue(mInputConnection.commitText("h", 1));
        mInOrder.verify(mVerifier).onUpdateSelection(1, 1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "h", "", -1, 0, -1, 0, 1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "h", "", 1, 1, 1, -1, -1);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);

        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // The controller kicks in.
        mAutocomplete.setAutocompleteText("h", "ello world", Optional.of("www.foo.com"));
        assertFalse(mAutocomplete.isCursorVisible());
        // The input characters are not enough, so additional texts are not shown.
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello world", "h", -1, 1, -1, 0, 10);
        assertVerifierCallCounts(0, 1);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // User types "he".
        assertTrue(mInputConnection.commitText("e", 1));
        mInOrder.verify(mVerifier).onUpdateSelection(2, 2);
        // The input characters are not enough, so additional texts are not shown.
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                "hello world",
                "",
                11,
                2,
                2,
                -1,
                -1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello world", "he", -1, 2, -1, 0, 9);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("he", "llo world", Optional.of("www.bar.com"));
        assertFalse(mAutocomplete.isCursorVisible());

        mInOrder.verifyNoMoreInteractions();
        assertTexts("he", "llo world", "www.bar.com");
        assertTrue(mAutocomplete.shouldAutocomplete());

        // User types "hello".
        assertTrue(mInputConnection.commitText("llo", 1));
        mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        // The input characters are enough, so additional texts are shown.
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                "hello world - www.bar.com",
                "",
                25,
                5,
                5,
                -1,
                -1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                "hello world - www.bar.com",
                "hello",
                -1,
                5,
                -1,
                0,
                6);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world", Optional.of("www.foobar.com"));
        assertFalse(mAutocomplete.isCursorVisible());
        assertVerifierCallCounts(0, 0);
        mInOrder.verifyNoMoreInteractions();
        assertTexts("hello", " world", "www.foobar.com");
        assertTrue(mAutocomplete.shouldAutocomplete());
    }

    @Test
    public void testAppendWithAdditionalText_onSelectionChanged() {
        OmniboxFeatures.sRichInlineAutocomplete.setForTesting(true);
        OmniboxFeatures.sRichInlineShowFullUrl.setForTesting(true);
        OmniboxFeatures.sRichInlineMinimumInputChars.setForTesting(1);

        // User types "h".
        assertTrue(mInputConnection.commitText("h", 1));
        mInOrder.verify(mVerifier).onUpdateSelection(1, 1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "h", "", -1, 0, -1, 0, 1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "h", "", 1, 1, 1, -1, -1);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);

        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // The controller kicks in.
        mAutocomplete.setAutocompleteText("h", "ello world", Optional.of("www.foo.com"));
        assertFalse(mAutocomplete.isCursorVisible());
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                "hello world - www.foo.com",
                "h",
                -1,
                1,
                -1,
                0,
                10);
        assertVerifierCallCounts(0, 1);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // User taps on "hello world - www.fo[|]o.com".
        mAutocomplete.onSelectionChanged(20, 20);
        mInOrder.verify(mVerifier).onUpdateSelection(11, 11);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                "hello world",
                "",
                11,
                11,
                11,
                -1,
                -1);

        // User selects on "[hello world - www.fo]o.com".
        mAutocomplete.onSelectionChanged(0, 20);
        mInOrder.verify(mVerifier).onUpdateSelection(0, 11);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                "hello world",
                "",
                11,
                0,
                11,
                -1,
                -1);
    }

    @Test
    public void testAppendWithAdditionalText_removeAutocompleteAndAddtionalText() {
        OmniboxFeatures.sRichInlineAutocomplete.setForTesting(true);
        OmniboxFeatures.sRichInlineShowFullUrl.setForTesting(true);
        OmniboxFeatures.sRichInlineMinimumInputChars.setForTesting(1);

        // User types "hello".
        assertTrue(mInputConnection.commitText("hello", 1));
        mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "", -1, 0, -1, 0, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "hello", "", 5, 5, 5, -1, -1);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world", Optional.of("www.foo.com"));
        assertFalse(mAutocomplete.isCursorVisible());
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                "hello world - www.foo.com",
                "hello",
                -1,
                5,
                -1,
                0,
                6);
        assertVerifierCallCounts(0, 1);
        assertTexts("hello", " world", "www.foo.com");
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // User taps on "he[|]llo world - www.foo.com", the autocomplete and additional text will be
        // removed.
        mAutocomplete.onSelectionChanged(2, 2);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "hello", "", 5, 5, 5, -1, -1);
    }

    @Test
    public void testAppend_SetComposingText() {
        // User types "h".
        assertTrue(mInputConnection.setComposingText("h", 1));

        mInOrder.verify(mVerifier).onUpdateSelection(1, 1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "h", "", -1, 0, -1, 0, 1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "h", "", 1, 1, 1, -1, -1);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);
        mInOrder.verifyNoMoreInteractions();

        // The old model does not allow autocompletion here.
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("h", "ello world", Optional.empty());
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello world", "h", -1, 1, -1, 0, 10);
        assertFalse(mAutocomplete.isCursorVisible());
        assertTexts("h", "ello world", "");
        assertVerifierCallCounts(0, 1);
        mInOrder.verifyNoMoreInteractions();

        // User types "hello".
        assertTrue(mInputConnection.setComposingText("hello", 1));
        mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                "hello world",
                "",
                11,
                5,
                5,
                -1,
                -1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello world", "hello", -1, 5, -1, 0, 6);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);
        mInOrder.verifyNoMoreInteractions();
        assertTexts("hello", " world", "");

        // The old model does not allow autocompletion here.
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world", Optional.empty());
        assertFalse(mAutocomplete.isCursorVisible());
        assertTexts("hello", " world", "");
        assertVerifierCallCounts(0, 0);
        mInOrder.verifyNoMoreInteractions();

        // User types a space.
        assertTrue(mInputConnection.beginBatchEdit());
        // We should still show the intermediate autocomplete text to the user even in the
        // middle of a batch edit. Otherwise, the user may see flickering of autocomplete text.
        assertEquals("hello world", mAutocomplete.getText().toString());

        assertTrue(mInputConnection.finishComposingText());

        assertEquals("hello world", mAutocomplete.getText().toString());

        assertTrue(mInputConnection.commitText(" ", 1));

        assertEquals("hello world", mAutocomplete.getText().toString());
        assertVerifierCallCounts(0, 0);
        mInOrder.verifyNoMoreInteractions();

        assertLastBatchEdit(mInputConnection.endBatchEdit());

        mInOrder.verify(mVerifier).onUpdateSelection(6, 6);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                "hello world",
                "",
                11,
                6,
                6,
                -1,
                -1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED,
                "hello world",
                "hello ",
                -1,
                6,
                -1,
                0,
                5);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);
        mInOrder.verifyNoMoreInteractions();

        // Autocomplete text has been drawn at endBatchEdit().
        assertTexts("hello ", "world", "");

        // The old model can also autocomplete now.
        assertTrue(mAutocomplete.shouldAutocomplete());
        mAutocomplete.setAutocompleteText("hello ", "world", Optional.of("foo.com"));
        assertTexts("hello ", "world", "foo.com");
        assertFalse(mAutocomplete.isCursorVisible());
        assertVerifierCallCounts(0, 0);
        mInOrder.verifyNoMoreInteractions();
    }

    @Test
    public void testAppend_DispatchKeyEvent() {
        // User types "h".
        mAutocomplete.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_H));
        mAutocomplete.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_H));
        mInOrder.verify(mVerifier).onUpdateSelection(1, 1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "h", "", -1, 0, -1, 0, 1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "h", "", 1, 1, 1, -1, -1);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // The controller kicks in.
        mAutocomplete.setAutocompleteText("h", "ello world", Optional.empty());
        // The non-spannable model changes selection in two steps.
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello world", "h", -1, 1, -1, 0, 10);
        assertVerifierCallCounts(0, 1);
        assertFalse(mAutocomplete.isCursorVisible());
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());

        // User types "he".
        mAutocomplete.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_E));
        mAutocomplete.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_E));
        mInOrder.verify(mVerifier).onUpdateSelection(2, 2);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                "hello world",
                "",
                11,
                2,
                2,
                -1,
                -1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello world", "he", -1, 2, -1, 0, 9);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        // The new model tries to reuse autocomplete text.
        assertTexts("he", "llo world", "");
        assertVerifierCallCounts(2, 2);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("he", "llo world", Optional.of("foo.com"));
        assertFalse(mAutocomplete.isCursorVisible());
        assertVerifierCallCounts(0, 0);
        mInOrder.verifyNoMoreInteractions();
        assertTexts("he", "llo world", "foo.com");
        assertTrue(mAutocomplete.shouldAutocomplete());
        mInOrder.verifyNoMoreInteractions();
    }

    @Test
    public void testDelete_CommitText() {
        // User types "hello".
        assertTrue(mInputConnection.commitText("hello", 1));
        mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "", -1, 0, -1, 0, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "hello", "", 5, 5, 5, -1, -1);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world", Optional.empty());
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello world", "hello", -1, 5, -1, 0, 6);
        assertVerifierCallCounts(0, 1);
        assertFalse(mAutocomplete.isCursorVisible());
        assertTexts("hello", " world", "");
        mInOrder.verifyNoMoreInteractions();

        // User deletes autocomplete.
        assertTrue(mInputConnection.deleteSurroundingText(1, 0)); // deletes one character

        // Pretend that we have deleted 'o' first.
        mInOrder.verify(mVerifier).onUpdateSelection(4, 4);
        // We restore 'o', and clear autocomplete text instead.
        mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        assertTrue(mAutocomplete.isCursorVisible());
        // Autocomplete removed.
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "hello world", -1, 5, -1, 6, 0);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(3, 1);
        mInOrder.verifyNoMoreInteractions();
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertTexts("hello", "", "");

        // Keyboard app checks the current state.
        assertEquals("hello", mInputConnection.getTextBeforeCursor(10, 0));
        assertTrue(mAutocomplete.isCursorVisible());
        assertVerifierCallCounts(0, 0);
        mInOrder.verifyNoMoreInteractions();
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertTexts("hello", "", "");
    }

    private boolean isComposing() {
        return BaseInputConnection.getComposingSpanStart(mAutocomplete.getText())
                != BaseInputConnection.getComposingSpanEnd(mAutocomplete.getText());
    }

    @Test
    public void testDelete_SetComposingText() {
        // User types "hello".
        assertTrue(mInputConnection.setComposingText("hello", 1));
        assertTrue(isComposing());
        mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "", -1, 0, -1, 0, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "hello", "", 5, 5, 5, -1, -1);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world", Optional.empty());
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello world", "hello", -1, 5, -1, 0, 6);
        assertFalse(mAutocomplete.isCursorVisible());
        assertTexts("hello", " world", "");
        assertVerifierCallCounts(0, 1);
        mInOrder.verifyNoMoreInteractions();

        // User deletes autocomplete.
        assertTrue(mInputConnection.setComposingText("hell", 1));
        // Pretend that we have deleted 'o'.
        mInOrder.verify(mVerifier).onUpdateSelection(4, 4);
        // We restore 'o', finish composition, and clear autocomplete text instead.
        mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        assertTrue(mAutocomplete.isCursorVisible());
        assertFalse(isComposing());
        // Remove autocomplete.
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "hello world", -1, 5, -1, 6, 0);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(3, 1);
        mInOrder.verifyNoMoreInteractions();
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertTexts("hello", "", "");

        // Keyboard app checks the current state.
        assertEquals("hello", mInputConnection.getTextBeforeCursor(10, 0));
        assertTrue(mAutocomplete.isCursorVisible());
        assertVerifierCallCounts(0, 0);
        mInOrder.verifyNoMoreInteractions();
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertTexts("hello", "", "");
    }

    @Test
    public void testDelete_SamsungKeyboard() {
        mAutocomplete.setKeyboardPackageName("com.sec.android.inputmethod");
        // User types "hello".
        assertTrue(mInputConnection.setComposingText("hello", 1));
        assertTrue(isComposing());
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world", Optional.empty());
        assertTexts("hello", " world", "");

        // User deletes autocomplete.
        assertTrue(mInputConnection.setComposingText("hell", 1));
        // Remove autocomplete.
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertTexts("hello", "", "");
        // Make sure that we do not finish composing text for Samsung keyboard - it does not update
        // its internal states when we ask this. (crbug.com/766888).
        assertTrue(isComposing());
    }

    @Test
    public void testDelete_SetComposingTextInBatchEdit() {
        // User types "hello".
        assertTrue(mInputConnection.setComposingText("hello", 1));

        mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "", -1, 0, -1, 0, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "hello", "", 5, 5, 5, -1, -1);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        mInOrder.verifyNoMoreInteractions();
        assertVerifierCallCounts(2, 2);
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world", Optional.empty());
        assertFalse(mAutocomplete.isCursorVisible());
        assertTexts("hello", " world", "");
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello world", "hello", -1, 5, -1, 0, 6);
        mInOrder.verifyNoMoreInteractions();
        assertVerifierCallCounts(0, 1);

        // User deletes 'o' in a batch edit.
        assertTrue(mInputConnection.beginBatchEdit());
        assertTrue(mInputConnection.setComposingText("hell", 1));

        // We restore 'o', and clear autocomplete text instead.
        assertTrue(mAutocomplete.isCursorVisible());
        assertFalse(mAutocomplete.shouldAutocomplete());

        // The user will see "hello" even in the middle of a batch edit.
        assertEquals("hello", mAutocomplete.getText().toString());

        // Keyboard app checks the current state.
        assertEquals("hell", mInputConnection.getTextBeforeCursor(10, 0));
        assertTrue(mAutocomplete.isCursorVisible());
        assertFalse(mAutocomplete.shouldAutocomplete());

        mInOrder.verifyNoMoreInteractions();
        assertVerifierCallCounts(0, 0);

        assertLastBatchEdit(mInputConnection.endBatchEdit());
        mInOrder.verify(mVerifier).onUpdateSelection(4, 4);
        mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "hello world", -1, 5, -1, 6, 0);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertTexts("hello", "", "");
        mInOrder.verifyNoMoreInteractions();
        assertVerifierCallCounts(3, 1);
    }

    @Test
    public void testSelect_SelectAutocomplete() {
        // User types "hello".
        assertTrue(mInputConnection.commitText("hello", 1));
        mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "", -1, 0, -1, 0, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "hello", "", 5, 5, 5, -1, -1);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world", Optional.empty());
        assertTexts("hello", " world", "");
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello world", "hello", -1, 5, -1, 0, 6);
        assertFalse(mAutocomplete.isCursorVisible());
        assertVerifierCallCounts(0, 1);
        mInOrder.verifyNoMoreInteractions();
        // User touches autocomplete text.
        mAutocomplete.setSelection(7);
        mInOrder.verify(mVerifier).onUpdateSelection(7, 7);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                "hello world",
                "",
                11,
                7,
                7,
                -1,
                -1);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 1);
        mInOrder.verifyNoMoreInteractions();
        assertFalse(mAutocomplete.shouldAutocomplete());
        assertTexts("hello world", "", "");
    }

    @Test
    public void testSelect_SelectUserText() {
        // User types "hello".
        assertTrue(mInputConnection.commitText("hello", 1));
        mInOrder.verify(mVerifier).onUpdateSelection(5, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "", -1, 0, -1, 0, 5);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "hello", "", 5, 5, 5, -1, -1);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);
        mInOrder.verifyNoMoreInteractions();
        assertTrue(mAutocomplete.shouldAutocomplete());
        // The controller kicks in.
        mAutocomplete.setAutocompleteText("hello", " world", Optional.empty());
        assertTexts("hello", " world", "");
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello world", "hello", -1, 5, -1, 0, 6);
        assertFalse(mAutocomplete.isCursorVisible());
        assertVerifierCallCounts(0, 1);
        mInOrder.verifyNoMoreInteractions();
        // User touches the user text.
        mAutocomplete.setSelection(3);
        assertTrue(mAutocomplete.isCursorVisible());
        mInOrder.verify(mVerifier).onUpdateSelection(3, 3);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, "hello", "hello world", -1, 5, -1, 6, 0);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, "hello", "", 5, 3, 3, -1, -1);
        mInOrder.verify(mVerifier).onAutocompleteTextStateChanged(false);
        assertVerifierCallCounts(2, 2);
        mInOrder.verifyNoMoreInteractions();
        assertFalse(mAutocomplete.shouldAutocomplete());
        // Autocomplete text is removed.
        assertTexts("hello", "", "");
    }

    @Test
    public void testAppend_AfterSelectAll() {
        final String url = "https://www.google.com/";
        mAutocomplete.setText(url);
        mAutocomplete.setSelection(0, url.length());
        assertTrue(mAutocomplete.isCursorVisible());
        // User types "h" - note that this is also starting character of the URL. The selection gets
        // replaced by what user types.
        assertTrue(mInputConnection.commitText("h", 1));
        // We want to allow inline autocomplete when the user overrides an existing URL.
        assertTrue(mAutocomplete.shouldAutocomplete());
        assertTrue(mAutocomplete.isCursorVisible());
    }

    @Test
    public void testIgnoreAndGet() {
        final String url = "https://www.google.com/";
        mAutocomplete.setIgnoreTextChangesForAutocomplete(true);
        mAutocomplete.setText(url);
        mAutocomplete.setIgnoreTextChangesForAutocomplete(false);
        mInputConnection.getTextBeforeCursor(1, 1);
        assertTrue(mAutocomplete.isCursorVisible());
        mInOrder.verifyNoMoreInteractions();
    }

    // crbug.com/760013
    @Test
    public void testOnSaveInstanceStateDoesNotCrash() {
        mInputConnection.setComposingText("h", 1);
        mAutocomplete.setAutocompleteText("h", "ello world", Optional.of("foo.com"));
        // On Android JB, TextView#onSaveInstanceState() calls new SpannableString(mText). This
        // should not crash.
        new SpannableString(mAutocomplete.getText());
    }

    // crbug.com/759876
    @Test
    public void testFocusInAndSelectAll() {
        final String url = "https://google.com";
        final int len = url.length();
        mAutocomplete.setIgnoreTextChangesForAutocomplete(true);
        mAutocomplete.setText(url);
        mAutocomplete.setIgnoreTextChangesForAutocomplete(false);

        mInOrder.verifyNoMoreInteractions();
        assertVerifierCallCounts(0, 0);

        assertTrue(mFocusPlaceHolder.requestFocus());

        mInOrder.verifyNoMoreInteractions();
        assertVerifierCallCounts(0, 0);

        // LocationBarLayout does this.
        mAutocomplete.setSelectAllOnFocus(true);

        assertTrue(mAutocomplete.requestFocus());

        mInOrder.verify(mVerifier).onUpdateSelection(len, len);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, url, "", 18, 18, 18, -1, -1);
        mInOrder.verify(mVerifier).onUpdateSelection(0, len);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, url, "", 18, 0, 18, -1, -1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_FOCUSED, url, "", 2, -1, -1, -1, -1);

        assertVerifierCallCounts(2, 3);
        mInOrder.verifyNoMoreInteractions();
    }

    // crbug.com/764749
    @Test
    public void testNonMatchingBatchEdit() {
        // beginBatchEdit() was not matched by endBatchEdit(), for some reason.
        mInputConnection.beginBatchEdit();

        // Restart input should reset batch edit count.
        assertNotNull(mAutocomplete.onCreateInputConnection(new EditorInfo()));
        mInputConnection = mAutocomplete.getInputConnection();

        assertTrue(mInputConnection.commitText("a", 1));
        // Works again.
        assertTrue(mAutocomplete.shouldAutocomplete());
    }

    // crbug.com/768323
    @Test
    public void testFocusLossHidesCursor() {
        assertTrue(mAutocomplete.isFocused());
        assertTrue(mAutocomplete.isCursorVisible());

        // AutocompleteEditText loses focus, and this hides cursor.
        assertTrue(mFocusPlaceHolder.requestFocus());

        assertFalse(mAutocomplete.isFocused());
        assertFalse(mAutocomplete.isCursorVisible());

        // Some IME operations may arrive after focus loss, but this should never show cursor.
        mInputConnection.getTextBeforeCursor(1, 0);
        assertFalse(mAutocomplete.isCursorVisible());
    }

    @Test
    public void testUnsupportedKeyboard() {
        mAutocomplete.setKeyboardPackageName("jp.co.sharp.android.iwnn");
        // User types "h".
        assertTrue(mInputConnection.commitText("h", 1));
        assertFalse(mAutocomplete.shouldAutocomplete());
    }

    // crbug.com/783165
    @Test
    public void testSetTextAndSelect() {
        // User types "h".
        assertTrue(mInputConnection.commitText("h", 1));
        assertTrue(mAutocomplete.shouldAutocomplete());
        mAutocomplete.setAutocompleteText("h", "ello world", Optional.empty());
        mAutocomplete.setIgnoreTextChangesForAutocomplete(true);
        mAutocomplete.setText("abcde");
        mAutocomplete.setIgnoreTextChangesForAutocomplete(false);
        assertEquals("abcde", mAutocomplete.getText().toString());

        mAutocomplete.setSelection(0);

        // Check the internal states are correct.
        assertEquals("abcde", mAutocomplete.getText().toString());
        assertEquals("abcde", mAutocomplete.getTextWithAutocomplete());
        assertEquals("abcde", mAutocomplete.getTextWithoutAutocomplete());
    }

    // crbug.com/810704
    @Test
    public void testPerformEditorAction() {
        // User types "goo".
        assertTrue(mInputConnection.setComposingText("goo", 1));
        assertTrue(mAutocomplete.shouldAutocomplete());
        mAutocomplete.setAutocompleteText("goo", "gle.com", Optional.empty());
        assertEquals("google.com", mAutocomplete.getText().toString());

        // User presses 'GO' key on the keyboard.
        assertTrue(mInputConnection.commitText("goo", 1));
        assertEquals("google.com", mAutocomplete.getText().toString());

        assertTrue(mInputConnection.performEditorAction(EditorInfo.IME_ACTION_GO));
        assertEquals("google.com", mAutocomplete.getText().toString());
    }

    @Test
    public void testPerformEditorAction_withAdditionText() {
        OmniboxFeatures.sRichInlineAutocomplete.setForTesting(true);
        OmniboxFeatures.sRichInlineShowFullUrl.setForTesting(true);
        OmniboxFeatures.sRichInlineMinimumInputChars.setForTesting(3);

        // User types "goo".
        assertTrue(mInputConnection.setComposingText("goo", 1));
        assertTrue(mAutocomplete.shouldAutocomplete());
        mAutocomplete.setAutocompleteText("goo", "gle.com", Optional.of("www.google.com"));
        assertEquals("google.com - www.google.com", mAutocomplete.getText().toString());

        // User presses 'GO' key on the keyboard.
        assertTrue(mInputConnection.commitText("goo", 1));
        assertEquals("google.com - www.google.com", mAutocomplete.getText().toString());

        assertTrue(mInputConnection.performEditorAction(EditorInfo.IME_ACTION_GO));
        assertEquals("google.com", mAutocomplete.getText().toString());
    }

    // crbug.com/810704
    @Test
    public void testPerformEditorActionInBatchEdit() {
        // User types "goo".
        assertTrue(mInputConnection.setComposingText("goo", 1));
        assertTrue(mAutocomplete.shouldAutocomplete());
        mAutocomplete.setAutocompleteText("goo", "gle.com", Optional.empty());
        assertEquals("google.com", mAutocomplete.getText().toString());

        // User presses 'GO' key on the keyboard.
        mInputConnection.beginBatchEdit();

        assertTrue(mInputConnection.commitText("goo", 1));
        assertEquals("google.com", mAutocomplete.getText().toString());

        assertTrue(mInputConnection.performEditorAction(EditorInfo.IME_ACTION_GO));
        assertEquals("google.com", mAutocomplete.getText().toString());

        mInputConnection.endBatchEdit();

        assertEquals("google.com", mAutocomplete.getText().toString());
    }

    // crbug.com/759876
    @Test
    public void testTextSelectionGetsAnnouncedAgainOnFocus() {
        final String text = "hello";
        final int len = text.length();

        assertTrue(mInputConnection.commitText(text, len));
        mAutocomplete.setSelection(0, len);

        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED, text, "", -1, 0, -1, 0, len);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED,
                text,
                "",
                len,
                len,
                len,
                -1,
                -1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, text, "", len, 0, len, -1, -1);
        mInOrder.verifyNoMoreInteractions();
        assertVerifierCallCounts(3, 3);

        assertTrue(mFocusPlaceHolder.requestFocus());
        mInOrder.verifyNoMoreInteractions();
        assertVerifierCallCounts(0, 0);

        // We left EditText with selected content. We should get the same event sent again now.
        mAutocomplete.setSelectAllOnFocus(true);
        assertTrue(mAutocomplete.requestFocus());

        mInOrder.verify(mVerifier).onUpdateSelection(0, len);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED, text, "", len, 0, len, -1, -1);
        verifyOnPopulateAccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_FOCUSED, text, "", 2, -1, -1, -1, -1);

        assertVerifierCallCounts(2, 3);
        mInOrder.verifyNoMoreInteractions();
    }

    // crbug.com/759876
    @Test
    public void testEndBatchEditCanReturnFalse() {
        assertTrue(mInputConnection.beginBatchEdit());
        assertLastBatchEdit(mInputConnection.endBatchEdit());
        // Additional endBatchEdit() must continue returning false.
        assertFalse(mInputConnection.endBatchEdit());
    }

    @Test
    public void testJavascriptSchemeShouldBeRemovedWhenPaste() {
        assertTrue(mInputConnection.commitText("javascript:alert(\"Test\")", 1));
        assertEquals("alert(\"Test\")", mAutocomplete.getText().toString());
    }

    @Test
    public void testJavascriptSchemeShouldNotBeRemovedWhenPatialPaste() {
        assertTrue(mInputConnection.commitText("j", 1));
        assertTrue(mInputConnection.commitText("avascript:alert(\"Test\")", 1));
        assertEquals("javascript:alert(\"Test\")", mAutocomplete.getText().toString());
    }

    @Test
    public void testJavascriptSchemeShouldNotBeRemovedWhenPatialPaste_LastInputIsOneLetter() {
        assertTrue(mInputConnection.commitText("javascript", 1));
        assertTrue(mInputConnection.commitText(":", 1));
        assertEquals("javascript:", mAutocomplete.getText().toString());
    }
}
