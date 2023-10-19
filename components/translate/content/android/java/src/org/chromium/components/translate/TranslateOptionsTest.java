// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.translate;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Test for TranslateOptions. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TranslateOptionsTest {
    private static final boolean NEVER_LANGUAGE = false;
    private static final boolean NEVER_DOMAIN = false;
    private static final boolean ALWAYS_TRANSLATE = true;
    private static final String[] LANGUAGES = {"English", "French", "Spanish"};
    private static final String[] CODES = {"en", "fr", "es"};
    private static final int[] UMA_HASH_CODES = {10, 20, 30};

    private static final String[] CONTENT_LANGUAGES_CODES = {"es", "fr"};

    @Before
    public void setUp() {}

    @Test
    @SmallTest
    @Feature({"Translate"})
    public void testNoChanges() {
        TranslateOptions options =
                TranslateOptions.create(
                        "en",
                        "es",
                        LANGUAGES,
                        CODES,
                        NEVER_LANGUAGE,
                        NEVER_DOMAIN,
                        ALWAYS_TRANSLATE,
                        /* triggeredFromMenu= */ false,
                        /* hashCodes= */ null,
                        CONTENT_LANGUAGES_CODES);
        Assert.assertEquals("English", options.sourceLanguageName());
        Assert.assertEquals("Spanish", options.targetLanguageName());
        Assert.assertEquals("en", options.sourceLanguageCode());
        Assert.assertEquals("es", options.targetLanguageCode());
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.NEVER_LANGUAGE));
        Assert.assertTrue(options.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE));
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.NEVER_DOMAIN));
        Assert.assertFalse(options.optionsChanged());
    }

    @Test
    @SmallTest
    @Feature({"Translate"})
    public void testBasicLanguageChanges() {
        TranslateOptions options =
                TranslateOptions.create(
                        "en",
                        "es",
                        LANGUAGES,
                        CODES,
                        NEVER_LANGUAGE,
                        NEVER_DOMAIN,
                        !ALWAYS_TRANSLATE,
                        /* triggeredFromMenu= */ true,
                        UMA_HASH_CODES,
                        CONTENT_LANGUAGES_CODES);
        // Charge target and source languages.
        options.setTargetLanguage("fr");
        options.setSourceLanguage("en");
        Assert.assertEquals("English", options.sourceLanguageName());
        Assert.assertEquals("French", options.targetLanguageName());
        Assert.assertEquals("en", options.sourceLanguageCode());
        Assert.assertEquals("fr", options.targetLanguageCode());
        Assert.assertTrue(options.triggeredFromMenu());
        Assert.assertEquals("English", options.getRepresentationFromCode("en"));

        Assert.assertTrue(options.optionsChanged());

        // Switch back to the original
        options.setSourceLanguage("en");
        options.setTargetLanguage("es");
        Assert.assertFalse(options.optionsChanged());

        // Same target language as source
        Assert.assertTrue(options.setTargetLanguage("en"));

        // Same source and target
        Assert.assertTrue(options.setTargetLanguage("es"));
        Assert.assertTrue(options.setSourceLanguage("es"));
    }

    @Test
    @SmallTest
    @Feature({"Translate"})
    public void testInvalidLanguageChanges() {
        TranslateOptions options =
                TranslateOptions.create(
                        "en",
                        "es",
                        LANGUAGES,
                        CODES,
                        NEVER_LANGUAGE,
                        NEVER_DOMAIN,
                        ALWAYS_TRANSLATE,
                        /* triggeredFromMenu= */ false,
                        /* hashCodes= */ null,
                        CONTENT_LANGUAGES_CODES);

        // Target language does not exist
        Assert.assertFalse(options.setTargetLanguage("aaa"));
        Assert.assertFalse(options.optionsChanged());

        // Source language does not exist
        Assert.assertFalse(options.setSourceLanguage("bbb"));
        Assert.assertFalse(options.optionsChanged());
    }

    @Test
    @SmallTest
    @Feature({"Translate"})
    public void testBasicOptionsChanges() {
        TranslateOptions options =
                TranslateOptions.create(
                        "en",
                        "es",
                        LANGUAGES,
                        CODES,
                        NEVER_LANGUAGE,
                        NEVER_DOMAIN,
                        !ALWAYS_TRANSLATE,
                        /* triggeredFromMenu= */ false,
                        /* hashCodes= */ null,
                        CONTENT_LANGUAGES_CODES);
        Assert.assertFalse(options.optionsChanged());
        options.toggleNeverTranslateDomainState(true);
        Assert.assertTrue(options.getTranslateState(TranslateOptions.Type.NEVER_DOMAIN));
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE));
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.NEVER_LANGUAGE));
        Assert.assertTrue(options.optionsChanged());
        options.toggleNeverTranslateDomainState(false);
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.NEVER_DOMAIN));
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.NEVER_LANGUAGE));
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE));

        // We are back to the original state
        Assert.assertFalse(options.optionsChanged());
        options.toggleAlwaysTranslateLanguageState(true);
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.NEVER_DOMAIN));
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.NEVER_LANGUAGE));
        Assert.assertTrue(options.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE));
        Assert.assertTrue(options.optionsChanged());

        // Toggle never translate language and check that always translate is toggled.
        options.toggleNeverTranslateLanguageState(true);
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.NEVER_DOMAIN));
        Assert.assertTrue(options.getTranslateState(TranslateOptions.Type.NEVER_LANGUAGE));
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE));

        // Toggle always translate language and check that never translate is toggled.
        options.toggleAlwaysTranslateLanguageState(true);
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.NEVER_DOMAIN));
        Assert.assertFalse(options.getTranslateState(TranslateOptions.Type.NEVER_LANGUAGE));
        Assert.assertTrue(options.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE));
    }

    @Test
    @SmallTest
    @Feature({"Translate"})
    public void testContentLanguagesAreFilledAsExpected() {
        TranslateOptions options =
                TranslateOptions.create(
                        "en",
                        "es",
                        LANGUAGES,
                        CODES,
                        NEVER_LANGUAGE,
                        NEVER_DOMAIN,
                        ALWAYS_TRANSLATE,
                        /* triggeredFromMenu= */ false,
                        /* hashCodes= */ UMA_HASH_CODES,
                        CONTENT_LANGUAGES_CODES);

        Assert.assertEquals(2, options.contentLanguages().length);

        Assert.assertEquals("es", options.contentLanguages()[0]);
        Assert.assertEquals(
                "Spanish", options.getRepresentationFromCode(options.contentLanguages()[0]));
        Assert.assertEquals(
                "español", options.getNativeRepresentationFromCode(options.contentLanguages()[0]));

        Assert.assertEquals("fr", options.contentLanguages()[1]);
        Assert.assertEquals(
                "French", options.getRepresentationFromCode(options.contentLanguages()[1]));
        Assert.assertEquals(
                "français", options.getNativeRepresentationFromCode(options.contentLanguages()[1]));
    }

    @Test
    @SmallTest
    @Feature({"Translate"})
    public void testupdateContentLanguages() {
        TranslateOptions options =
                TranslateOptions.create(
                        "en",
                        "es",
                        LANGUAGES,
                        CODES,
                        NEVER_LANGUAGE,
                        NEVER_DOMAIN,
                        ALWAYS_TRANSLATE,
                        /* triggeredFromMenu= */ false,
                        /* hashCodes= */ UMA_HASH_CODES,
                        CONTENT_LANGUAGES_CODES);

        Assert.assertEquals(2, options.contentLanguages().length);

        Assert.assertEquals("es", options.contentLanguages()[0]);
        Assert.assertEquals("fr", options.contentLanguages()[1]);

        options.updateContentLanguages(new String[] {"en"});
        Assert.assertEquals(1, options.contentLanguages().length);

        Assert.assertEquals("en", options.contentLanguages()[0]);
        Assert.assertEquals(
                "English", options.getRepresentationFromCode(options.contentLanguages()[0]));
        Assert.assertEquals(
                "English", options.getNativeRepresentationFromCode(options.contentLanguages()[0]));
    }
}
