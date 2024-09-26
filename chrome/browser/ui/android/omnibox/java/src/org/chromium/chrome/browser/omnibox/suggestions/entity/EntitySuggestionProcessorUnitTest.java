// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.entity;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.core.IsInstanceOf.instanceOf;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.BaseSwitches;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor.BookmarkState;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Optional;

/** Tests for {@link EntitySuggestionProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@CommandLineFlags.Add(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)
public class EntitySuggestionProcessorUnitTest {
    private static final GURL WEB_URL = JUnitTestGURLs.URL_1;
    private static final GURL WEB_URL_2 = JUnitTestGURLs.URL_2;
    private static final GURL SEARCH_URL = JUnitTestGURLs.SEARCH_URL;

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock SuggestionHost mSuggestionHost;
    private @Mock OmniboxImageSupplier mImageSupplier;
    private @Mock Bitmap mBitmap;
    private @Mock BookmarkState mBookmarkState;
    private @Mock UrlBarEditingTextStateProvider mTextProvider;

    private EntitySuggestionProcessor mProcessor;

    /**
     * Base Suggestion class that can be used for testing. Holds all mechanisms that are required to
     * processSuggestion and validate suggestions.
     */
    static class SuggestionTestHelper {
        // Stores created AutocompleteMatch
        protected final AutocompleteMatch mSuggestion;
        // Stores PropertyModel for the suggestion.
        protected final PropertyModel mModel;

        private SuggestionTestHelper(AutocompleteMatch suggestion, PropertyModel model) {
            mSuggestion = suggestion;
            mModel = model;
        }

        /** Get Drawable associated with the suggestion. */
        Drawable getIcon() {
            final OmniboxDrawableState state = mModel.get(BaseSuggestionViewProperties.ICON);
            return state == null ? null : state.drawable;
        }
    }

    /** Create fake Entity suggestion. */
    SuggestionTestHelper createSuggestion(
            String subject, String description, String color, GURL url) {
        AutocompleteMatch suggestion =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY)
                        .setDisplayText(subject)
                        .setDescription(description)
                        .setImageUrl(url)
                        .setImageDominantColor(color)
                        .build();
        PropertyModel model = mProcessor.createModel();
        return new SuggestionTestHelper(suggestion, model);
    }

    /** Populate model for associated suggestion. */
    void processSuggestion(SuggestionTestHelper helper) {
        mProcessor.populateModel(helper.mSuggestion, helper.mModel, 0);
    }

    @Before
    public void setUp() {
        mProcessor =
                new EntitySuggestionProcessor(
                        ContextUtils.getApplicationContext(),
                        mSuggestionHost,
                        mTextProvider,
                        Optional.of(mImageSupplier),
                        mBookmarkState);
        doReturn("").when(mTextProvider).getTextWithoutAutocomplete();
    }

    @Test
    @SmallTest
    public void contentTest_basicContent() {
        SuggestionTestHelper suggHelper = createSuggestion("subject", "details", null, SEARCH_URL);
        processSuggestion(suggHelper);
        Assert.assertEquals(
                "subject",
                suggHelper.mModel.get(SuggestionViewProperties.TEXT_LINE_1_TEXT).toString());
        Assert.assertEquals(
                "details",
                suggHelper.mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT).toString());
    }

    @Test
    @SmallTest
    public void decorationTest_noColorOrImage() {
        SuggestionTestHelper suggHelper = createSuggestion("", "", null, SEARCH_URL);
        processSuggestion(suggHelper);

        Assert.assertNotNull(suggHelper.getIcon());
        assertThat(suggHelper.getIcon(), instanceOf(BitmapDrawable.class));
    }

    @Test
    @SmallTest
    public void decorationTest_validHexColor_lowMemoryDevice() {
        OmniboxFeatures.setIsLowMemoryDeviceForTesting(true);
        SuggestionTestHelper suggHelper = createSuggestion("", "", "#fedcba", SEARCH_URL);
        processSuggestion(suggHelper);

        assertThat(suggHelper.getIcon(), instanceOf(BitmapDrawable.class));
    }

    @Test
    @SmallTest
    public void decorationTest_validNamedColor() {
        SuggestionTestHelper suggHelper = createSuggestion("", "", "red", SEARCH_URL);
        processSuggestion(suggHelper);

        assertThat(suggHelper.getIcon(), instanceOf(ColorDrawable.class));
        ColorDrawable icon = (ColorDrawable) suggHelper.getIcon();
        Assert.assertEquals(icon.getColor(), Color.RED);
    }

    @Test
    @SmallTest
    public void decorationTest_invalidColor() {
        // Note, fallback is the bitmap drawable representing a search loupe.
        SuggestionTestHelper suggHelper = createSuggestion("", "", "", SEARCH_URL);
        processSuggestion(suggHelper);
        assertThat(suggHelper.getIcon(), instanceOf(BitmapDrawable.class));

        suggHelper = createSuggestion("", "", "#", SEARCH_URL);
        processSuggestion(suggHelper);
        assertThat(suggHelper.getIcon(), instanceOf(BitmapDrawable.class));

        suggHelper = createSuggestion("", "", "invalid", SEARCH_URL);
        processSuggestion(suggHelper);
        assertThat(suggHelper.getIcon(), instanceOf(BitmapDrawable.class));
    }

    @Test
    @SmallTest
    public void fetchImage_withSupplier() {
        SuggestionTestHelper suggHelper = createSuggestion("", "", "red", WEB_URL);
        processSuggestion(suggHelper);

        final ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        verify(mImageSupplier).fetchImage(eq(WEB_URL), callback.capture());

        assertThat(suggHelper.getIcon(), instanceOf(ColorDrawable.class));
        callback.getValue().onResult(mBitmap);
        assertThat(suggHelper.getIcon(), instanceOf(BitmapDrawable.class));
        Assert.assertEquals(mBitmap, ((BitmapDrawable) suggHelper.getIcon()).getBitmap());
    }

    @Test
    @SmallTest
    public void fetchImage_withoutSupplier() {
        mProcessor =
                new EntitySuggestionProcessor(
                        ContextUtils.getApplicationContext(),
                        mSuggestionHost,
                        mTextProvider,
                        /* imageSupplier= */ Optional.empty(),
                        mBookmarkState);
        SuggestionTestHelper suggHelper = createSuggestion("", "", "red", WEB_URL);
        processSuggestion(suggHelper);
        verifyNoMoreInteractions(mImageSupplier);
        // Expect a fallback icon.
        Assert.assertNotNull(suggHelper.getIcon());
    }

    @Test
    public void doesProcessSuggestion_entitySuggestion() {
        SuggestionTestHelper suggHelper = createSuggestion("", "", "red", WEB_URL);
        Assert.assertTrue(mProcessor.doesProcessSuggestion(suggHelper.mSuggestion, 0));
    }

    @Test
    public void doesProcessSuggestion_nonEntitySuggestion() {
        AutocompleteMatch suggestion =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .build();
        Assert.assertFalse(mProcessor.doesProcessSuggestion(suggestion, 0));
    }

    @Test
    public void getViewTypeId_forFullTestCoverage() {
        Assert.assertEquals(OmniboxSuggestionUiType.ENTITY_SUGGESTION, mProcessor.getViewTypeId());
    }

    @Test
    public void populateModel_suggestionTextDoesNotWrap() {
        SuggestionTestHelper suggHelper = createSuggestion("subject", "details", null, SEARCH_URL);
        processSuggestion(suggHelper);
        Assert.assertFalse(suggHelper.mModel.get(SuggestionViewProperties.ALLOW_WRAP_AROUND));
    }
}
