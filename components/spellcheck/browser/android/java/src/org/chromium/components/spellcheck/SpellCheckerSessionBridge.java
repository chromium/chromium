// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.spellcheck;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.text.style.SuggestionSpan;
import android.view.textservice.SentenceSuggestionsInfo;
import android.view.textservice.SpellCheckerSession;
import android.view.textservice.SpellCheckerSession.SpellCheckerSessionListener;
import android.view.textservice.SuggestionsInfo;
import android.view.textservice.TextInfo;
import android.view.textservice.TextServicesManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;

/** JNI interface for native SpellCheckerSessionBridge to use Android's spellchecker. */
@NullMarked
public class SpellCheckerSessionBridge implements SpellCheckerSessionListener {
    // LINT.IfChange(SpellCheckResultDecoration)
    /** Values from SpellCheckResult::Decoration on the C++ side * */
    private static class SpellCheckResultDecoration {
        public static final int SPELLING = 0;
        public static final int GRAMMAR = 1;
    }

    // LINT.ThenChange(/components/spellcheck/common/spellcheck_result.h:DecorationEnum)

    private long mNativeSpellCheckerSessionBridge;
    private final boolean mAllowGrammarChecks;
    private final boolean mAllowHideSuggestionMenuAttribute;
    private final @Nullable SpellCheckerSession mSpellCheckerSession;

    /**
     * Constructs a SpellCheckerSessionBridge object as well as its SpellCheckerSession object.
     *
     * @param nativeSpellCheckerSessionBridge Pointer to the native SpellCheckerSessionBridge.
     * @param allowGrammarChecks Allows grammar errors to be processed.
     * @param allowHideSuggestionMenuAttribute Allows should_hide_suggestion_menu attribute to be
     *     processed.
     */
    private SpellCheckerSessionBridge(
            long nativeSpellCheckerSessionBridge,
            boolean allowGrammarChecks,
            boolean allowHideSuggestionMenuAttribute) {
        mNativeSpellCheckerSessionBridge = nativeSpellCheckerSessionBridge;
        mAllowGrammarChecks = allowGrammarChecks;
        mAllowHideSuggestionMenuAttribute = allowHideSuggestionMenuAttribute;

        Context context = ContextUtils.getApplicationContext();
        final TextServicesManager textServicesManager =
                (TextServicesManager)
                        context.getSystemService(Context.TEXT_SERVICES_MANAGER_SERVICE);

        // This combination of parameters will cause the spellchecker to be based off of
        // the language specified at "Settings > Language & input > Spell checker > Language".
        // If that setting is set to "Use system language" and the system language is not on the
        // list of supported spellcheck languages, this call will return null.  This call will also
        // return null if the user has turned spellchecking off at "Settings > Language & input >
        // Spell checker".
        mSpellCheckerSession = textServicesManager.newSpellCheckerSession(null, null, this, true);
    }

    /**
     * Returns a new SpellCheckerSessionBridge object if the internal SpellCheckerSession object was
     * able to be created.
     *
     * @param nativeSpellCheckerSessionBridge Pointer to the native SpellCheckerSessionBridge.
     * @param allowGrammarChecks Allows grammar errors to be processed.
     * @param allowHideSuggestionMenuAttribute Allows should_hide_suggestion_menu attribute to be
     *     processed.
     */
    @CalledByNative
    private static @Nullable SpellCheckerSessionBridge create(
            long nativeSpellCheckerSessionBridge,
            boolean allowGrammarChecks,
            boolean allowHideSuggestionMenuAttribute) {
        SpellCheckerSessionBridge bridge =
                new SpellCheckerSessionBridge(
                        nativeSpellCheckerSessionBridge,
                        allowGrammarChecks,
                        allowHideSuggestionMenuAttribute);
        if (bridge.mSpellCheckerSession == null) {
            return null;
        }
        return bridge;
    }

    /** Reset the native bridge pointer, called when the native counterpart is destroyed. */
    @CalledByNative
    private void disconnect() {
        mNativeSpellCheckerSessionBridge = 0;
        assumeNonNull(mSpellCheckerSession);
        mSpellCheckerSession.cancel();
        mSpellCheckerSession.close();
    }

    /**
     * Queries the input text against the SpellCheckerSession.
     * @param text Text to be queried.
     */
    @CalledByNative
    private void requestTextCheck(String text) {
        // SpellCheckerSession thinks that any word ending with a period is a typo.
        // We trim the period off before sending the text for spellchecking in order to avoid
        // unnecessary red underlines when the user ends a sentence with a period.
        // Filed as an Android bug here: https://code.google.com/p/android/issues/detail?id=183294
        if (text.endsWith(".")) {
            text = text.substring(0, text.length() - 1);
        }
        assumeNonNull(mSpellCheckerSession)
                .getSentenceSuggestions(
                        new TextInfo[] {new TextInfo(text)}, SuggestionSpan.SUGGESTIONS_MAX_SIZE);
    }

    /**
     * Checks for typos and sends results back to native through a JNI call.
     * @param results Results returned by the Android spellchecker.
     */
    @Override
    public void onGetSentenceSuggestions(SentenceSuggestionsInfo[] results) {
        if (mNativeSpellCheckerSessionBridge == 0) {
            return;
        }

        ArrayList<Integer> offsets = new ArrayList<Integer>();
        ArrayList<Integer> lengths = new ArrayList<Integer>();
        ArrayList<String[]> suggestions = new ArrayList<String[]>();
        ArrayList<Integer> spellcheckResultDecorations = new ArrayList<Integer>();
        ArrayList<Boolean> hideSuggestionMenuBooleans = new ArrayList<Boolean>();

        for (SentenceSuggestionsInfo result : results) {
            if (result == null) {
                // In some cases null can be returned by the selected spellchecking service,
                // see crbug.com/651458. In this case skip to next result to avoid a
                // NullPointerException later on.
                continue;
            }
            for (int i = 0; i < result.getSuggestionsCount(); i++) {
                SuggestionsInfo info = result.getSuggestionsInfoAt(i);

                final int grammarBitMask =
                        mAllowGrammarChecks
                                ? SuggestionsInfo.RESULT_ATTR_LOOKS_LIKE_GRAMMAR_ERROR
                                : 0;

                final int dontShowUiBitMask =
                        mAllowHideSuggestionMenuAttribute
                                ? SuggestionsInfo.RESULT_ATTR_DONT_SHOW_UI_FOR_SUGGESTIONS
                                : 0;

                final int attributes = info.getSuggestionsAttributes();
                // If a word looks like a typo or grammar error, record its offset and length.
                if ((attributes & (SuggestionsInfo.RESULT_ATTR_LOOKS_LIKE_TYPO | grammarBitMask))
                        != 0) {
                    offsets.add(result.getOffsetAt(i));
                    lengths.add(result.getLengthAt(i));
                    // TODO(crbug.com/434080921): Verify which should take precedence if both are
                    // set.
                    final int decoration =
                            (attributes & grammarBitMask) != 0
                                    ? SpellCheckResultDecoration.GRAMMAR
                                    : SpellCheckResultDecoration.SPELLING;
                    spellcheckResultDecorations.add(decoration);
                    ArrayList<String> suggestionsForWord = new ArrayList<String>();
                    for (int j = 0; j < info.getSuggestionsCount(); ++j) {
                        String suggestion = info.getSuggestionAt(j);
                        // Remove zero-length space from end of suggestion, if any
                        if (suggestion.charAt(suggestion.length() - 1) == 0x200b) {
                            suggestion = suggestion.substring(0, suggestion.length() - 1);
                        }
                        suggestionsForWord.add(suggestion);
                    }
                    suggestions.add(
                            suggestionsForWord.toArray(new String[suggestionsForWord.size()]));
                    hideSuggestionMenuBooleans.add((attributes & dontShowUiBitMask) != 0);
                }
            }
        }
        SpellCheckerSessionBridgeJni.get()
                .processSpellCheckResults(
                        mNativeSpellCheckerSessionBridge,
                        convertIntListToArray(offsets),
                        convertIntListToArray(lengths),
                        suggestions.toArray(new String[suggestions.size()][]),
                        convertIntListToArray(spellcheckResultDecorations),
                        convertBoolListToArray(hideSuggestionMenuBooleans));
    }

    /**
     * Helper method to convert an ArrayList of Integer objects into an array of primitive ints for
     * easier JNI handling of these objects on the native side.
     *
     * @param list List to be converted to an array.
     */
    private int[] convertIntListToArray(ArrayList<Integer> list) {
        int[] array = new int[list.size()];
        for (int index = 0; index < array.length; index++) {
            array[index] = list.get(index).intValue();
        }
        return array;
    }

    /**
     * Helper method to convert an ArrayList of Boolean objects into an array of primitive bools for
     * easier JNI handling of these objects on the native side.
     *
     * @param list List to be converted to an array.
     */
    private boolean[] convertBoolListToArray(ArrayList<Boolean> list) {
        boolean[] array = new boolean[list.size()];
        for (int index = 0; index < array.length; index++) {
            array[index] = list.get(index).booleanValue();
        }
        return array;
    }

    @Override
    public void onGetSuggestions(SuggestionsInfo[] results) {}

    @NativeMethods
    interface Natives {
        void processSpellCheckResults(
                long nativeSpellCheckerSessionBridge,
                int[] offsets,
                int[] lengths,
                String[][] suggestions,
                int[] spellcheckResultDecorations,
                boolean[] hideSuggestionMenuBooleans);
    }
}
