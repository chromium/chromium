// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.translate;

import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * A class that keeps the state of the different translation options and
 * languages.
 */
public class TranslateOptions {
    /**
     * A container for Language Code and it's translated representation and it's native UMA
     * specific hashcode.
     * For example for Spanish when viewed from a French locale, this will contain es, Espagnol,
     *Español, 114573335
     **/
    public static class TranslateLanguageData {
        public final String mLanguageCode;
        public final String mLanguageRepresentation;
        // TODO(crbug.com/40266152): Remove |mLanguageUMAHashCode| as these hashes
        // are no longer used.
        public final Integer mLanguageUMAHashCode;

        public TranslateLanguageData(
                String languageCode, String languageRepresentation, Integer uMAhashCode) {
            assert languageCode != null;
            assert languageRepresentation != null;
            mLanguageCode = languageCode;
            mLanguageRepresentation = languageRepresentation;
            mLanguageUMAHashCode = uMAhashCode;
        }

        @Override
        public boolean equals(Object obj) {
            if (!(obj instanceof TranslateLanguageData)) return false;
            TranslateLanguageData other = (TranslateLanguageData) obj;
            return this.mLanguageCode.equals(other.mLanguageCode)
                    && this.mLanguageRepresentation.equals(other.mLanguageRepresentation)
                    && this.mLanguageUMAHashCode.equals(other.mLanguageUMAHashCode);
        }

        @Override
        public int hashCode() {
            return (mLanguageCode + mLanguageRepresentation).hashCode();
        }

        @Override
        public String toString() {
            return "mLanguageCode:"
                    + mLanguageCode
                    + " - mLanguageRepresentation "
                    + mLanguageRepresentation
                    + " - mLanguageUMAHashCode "
                    + mLanguageUMAHashCode;
        }
    }

    // Values must be numerated from 0 and can't have gaps
    // (they're used for indexing mOptions).
    @IntDef({Type.NEVER_LANGUAGE, Type.NEVER_DOMAIN, Type.ALWAYS_LANGUAGE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        int NEVER_LANGUAGE = 0;
        int NEVER_DOMAIN = 1;
        int ALWAYS_LANGUAGE = 2;

        int NUM_ENTRIES = 3;
    }

    private String mSourceLanguageCode;
    private String mTargetLanguageCode;

    private final ArrayList<TranslateLanguageData> mAllLanguages;
    @Nullable private String[] mContentLanguagesCodes;

    // Language code to UI display language name map Conceptually final
    private Map<String, String> mCodeToRepresentation;

    // Will reflect the state before the object was ever modified
    private final boolean[] mOriginalOptions;

    private final String mOriginalSourceLanguageCode;
    private final String mOriginalTargetLanguageCode;
    private final boolean mTriggeredFromMenu;

    private final boolean[] mOptions;

    private TranslateOptions(
            String sourceLanguageCode,
            String targetLanguageCode,
            ArrayList<TranslateLanguageData> allLanguages,
            String[] contentLanguages,
            boolean neverLanguage,
            boolean neverDomain,
            boolean alwaysLanguage,
            boolean triggeredFromMenu,
            boolean[] originalOptions) {
        assert Type.NUM_ENTRIES == 3;
        mOptions = new boolean[Type.NUM_ENTRIES];
        mOptions[Type.NEVER_LANGUAGE] = neverLanguage;
        mOptions[Type.NEVER_DOMAIN] = neverDomain;
        mOptions[Type.ALWAYS_LANGUAGE] = alwaysLanguage;

        mOriginalOptions = originalOptions == null ? mOptions.clone() : originalOptions.clone();

        mSourceLanguageCode = sourceLanguageCode;
        mTargetLanguageCode = targetLanguageCode;
        mOriginalSourceLanguageCode = mSourceLanguageCode;
        mOriginalTargetLanguageCode = mTargetLanguageCode;
        mTriggeredFromMenu = triggeredFromMenu;

        mAllLanguages = allLanguages;
        mContentLanguagesCodes = contentLanguages;
        mCodeToRepresentation = new HashMap<String, String>();
        for (TranslateLanguageData language : allLanguages) {
            mCodeToRepresentation.put(language.mLanguageCode, language.mLanguageRepresentation);
        }
    }

    /** Creates a TranslateOptions by the given data. */
    public static TranslateOptions create(
            String sourceLanguageCode,
            String targetLanguageCode,
            String[] languages,
            String[] codes,
            boolean neverLanguage,
            boolean neverDomain,
            boolean alwaysTranslate,
            boolean triggeredFromMenu,
            int[] hashCodes,
            String[] contentLanguagesCodes) {
        assert languages.length == codes.length;

        ArrayList<TranslateLanguageData> languageList = new ArrayList<TranslateLanguageData>();
        for (int i = 0; i < languages.length; ++i) {
            Integer hashCode = hashCodes != null ? Integer.valueOf(hashCodes[i]) : null;
            languageList.add(new TranslateLanguageData(codes[i], languages[i], hashCode));
        }

        return new TranslateOptions(
                sourceLanguageCode,
                targetLanguageCode,
                languageList,
                contentLanguagesCodes,
                neverLanguage,
                neverDomain,
                alwaysTranslate,
                triggeredFromMenu,
                null);
    }

    /** Returns a copy of the current instance. */
    TranslateOptions copy() {
        return new TranslateOptions(
                mSourceLanguageCode,
                mTargetLanguageCode,
                mAllLanguages,
                mContentLanguagesCodes,
                mOptions[Type.NEVER_LANGUAGE],
                mOptions[Type.NEVER_DOMAIN],
                mOptions[Type.ALWAYS_LANGUAGE],
                mTriggeredFromMenu,
                mOriginalOptions);
    }

    /** Updates content languages. */
    public void updateContentLanguages(String[] contentLanguagesCodes) {
        this.mContentLanguagesCodes = contentLanguagesCodes;
    }

    public String sourceLanguageName() {
        return getRepresentationFromCode(mSourceLanguageCode);
    }

    public String targetLanguageName() {
        return getRepresentationFromCode(mTargetLanguageCode);
    }

    public String sourceLanguageCode() {
        return mSourceLanguageCode;
    }

    public String targetLanguageCode() {
        return mTargetLanguageCode;
    }

    public boolean triggeredFromMenu() {
        return mTriggeredFromMenu;
    }

    public boolean optionsChanged() {
        return !mSourceLanguageCode.equals(mOriginalSourceLanguageCode)
                || !mTargetLanguageCode.equals(mOriginalTargetLanguageCode)
                || (mOptions[Type.NEVER_LANGUAGE] != mOriginalOptions[Type.NEVER_LANGUAGE])
                || (mOptions[Type.NEVER_DOMAIN] != mOriginalOptions[Type.NEVER_DOMAIN])
                || (mOptions[Type.ALWAYS_LANGUAGE] != mOriginalOptions[Type.ALWAYS_LANGUAGE]);
    }

    public List<TranslateLanguageData> allLanguages() {
        return mAllLanguages;
    }

    @Nullable
    public String[] contentLanguages() {
        return mContentLanguagesCodes;
    }

    public boolean getTranslateState(@Type int type) {
        return mOptions[type];
    }

    public boolean setSourceLanguage(String languageCode) {
        boolean canSet = canSetLanguage(languageCode, mTargetLanguageCode);
        if (canSet) mSourceLanguageCode = languageCode;
        return canSet;
    }

    public boolean setTargetLanguage(String languageCode) {
        boolean canSet = canSetLanguage(mSourceLanguageCode, languageCode);
        if (canSet) mTargetLanguageCode = languageCode;
        return canSet;
    }

    /** Sets the new state of never translate domain. */
    public void toggleNeverTranslateDomainState(boolean value) {
        mOptions[Type.NEVER_DOMAIN] = value;
    }

    /** Sets the new state of never translate language. */
    public void toggleNeverTranslateLanguageState(boolean value) {
        // Ensure AlwaysTranslate is disabled if enabling NeverTranslate.
        if (mOptions[Type.ALWAYS_LANGUAGE] && value) {
            toggleAlwaysTranslateLanguageState(false);
        }
        mOptions[Type.NEVER_LANGUAGE] = value;
    }

    /** Sets the new state of never translate a language pair. */
    public void toggleAlwaysTranslateLanguageState(boolean value) {
        // Ensure NeverTranslate is disabled if enabling AlwaysTranslate.
        if (mOptions[Type.NEVER_LANGUAGE] && value) {
            toggleNeverTranslateLanguageState(false);
        }
        mOptions[Type.ALWAYS_LANGUAGE] = value;
    }

    /**
     * Gets the language's translated representation from a given language code.
     * @param languageCode ISO code for the language
     * @return The translated representation of the language, or "" if not found.
     */
    public String getRepresentationFromCode(String languageCode) {
        return isValidLanguageCode(languageCode) ? mCodeToRepresentation.get(languageCode) : "";
    }

    /**
     * Gets the language's native representation from a given language code.
     * Only for content languages.
     * @param languageCode ISO code for the language
     * @return The native representation of the language.
     */
    public String getNativeRepresentationFromCode(String languageCode) {
        if (isValidLanguageCode(languageCode)) {
            Locale locale = Locale.forLanguageTag(languageCode);
            return locale.getDisplayName(locale);
        }
        return "";
    }

    private boolean isValidLanguageCode(String languageCode) {
        return !TextUtils.isEmpty(languageCode) && mCodeToRepresentation.containsKey(languageCode);
    }

    private boolean canSetLanguage(String sourceCode, String targetCode) {
        return isValidLanguageCode(sourceCode) && isValidLanguageCode(targetCode);
    }

    @Override
    public String toString() {
        return sourceLanguageCode()
                + " -> "
                + targetLanguageCode()
                + " - "
                + "Never Language:"
                + mOptions[Type.NEVER_LANGUAGE]
                + " Always Language:"
                + mOptions[Type.ALWAYS_LANGUAGE]
                + " Never Domain:"
                + mOptions[Type.NEVER_DOMAIN];
    }
}
