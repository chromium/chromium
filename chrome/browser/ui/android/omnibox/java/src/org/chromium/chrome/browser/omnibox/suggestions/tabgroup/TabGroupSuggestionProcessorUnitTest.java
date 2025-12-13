// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.tabgroup;

import android.content.Context;
import android.graphics.drawable.ShapeDrawable;
import android.text.style.ImageSpan;

import androidx.core.content.ContextCompat;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteUIContext;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor.BookmarkState;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

import java.util.function.Supplier;

/** Tests for {@link TabGroupSuggestionProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupSuggestionProcessorUnitTest {
    private static final String SYNC_ID = "sync_id";
    private static final String COLOR_ID = "2";
    private static final String TITLE = "My Group";
    private static final String DESCRIPTION = JUnitTestGURLs.URL_1.getSpec();

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock SuggestionHost mSuggestionHost;
    private @Mock OmniboxImageSupplier mImageSupplier;
    private @Mock UrlBarEditingTextStateProvider mTextProvider;
    private @Mock Supplier<Tab> mTabSupplier;
    private @Mock Supplier<ShareDelegate> mShareDelegateSupplier;
    private @Mock BookmarkState mBookmarkState;

    private Context mContext;
    private TabGroupSuggestionProcessor mProcessor;
    private AutocompleteMatch mSuggestion;
    private PropertyModel mModel;
    private AutocompleteInput mInput;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);

        AutocompleteUIContext uiContext =
                new AutocompleteUIContext(
                        mContext,
                        mSuggestionHost,
                        mTextProvider,
                        mImageSupplier,
                        mBookmarkState,
                        mTabSupplier,
                        mShareDelegateSupplier,
                        new ObservableSupplierImpl<>(ControlsPosition.TOP));
        mProcessor = new TabGroupSuggestionProcessor(uiContext);
        mInput = new AutocompleteInput();
        OmniboxResourceProvider.disableCachesForTesting();
    }

    @After
    public void tearDown() {
        OmniboxResourceProvider.reenableCachesForTesting();
        mInput.reset();
    }

    /** Create tab group suggestion for test. */
    private void createTabGroupSuggestion(int type) {
        mSuggestion =
                AutocompleteMatchBuilder.searchWithType(type)
                        .setTabGroupUuid(SYNC_ID)
                        .setImageDominantColor(COLOR_ID)
                        .setDisplayText(TITLE)
                        .setDescription(DESCRIPTION)
                        .build();
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mInput, mSuggestion, mModel, 0);
    }

    @Test
    @SmallTest
    public void testPopulateModelTabGroupSuggestions() {
        mInput.setPageClassification(PageClassification.ANDROID_HUB_VALUE);

        createTabGroupSuggestion(OmniboxSuggestionType.TAB_GROUP);
        PropertyModel model = mProcessor.createModel();

        mProcessor.populateModel(mInput, mSuggestion, model, 0);
        Assert.assertTrue(
                ContextCompat.getDrawable(mContext, R.drawable.ic_features_24dp)
                        .getConstantState()
                        .equals(
                                mModel.get(BaseSuggestionViewProperties.ICON)
                                        .drawable
                                        .getConstantState()));

        SuggestionSpannable suggestion = mModel.get(SuggestionViewProperties.TEXT_LINE_1_TEXT);
        ImageSpan[] imageSpans = suggestion.getSpans(0, suggestion.length(), ImageSpan.class);
        Assert.assertEquals(1, imageSpans.length);
        Assert.assertEquals(
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                        mContext, TabGroupColorId.RED, /* isIncognito= */ false),
                ((ShapeDrawable) imageSpans[0].getDrawable()).getPaint().getColor());

        Assert.assertEquals(
                DESCRIPTION, mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT).toString());
        Assert.assertEquals(
                TITLE,
                mModel.get(SuggestionViewProperties.TEXT_LINE_1_TEXT)
                        .subSequence(
                                3, mModel.get(SuggestionViewProperties.TEXT_LINE_1_TEXT).length())
                        .toString());

        String targetString =
                mContext.getString(
                        R.string.accessibility_tab_group_suggestion_description,
                        TITLE,
                        "Red",
                        DESCRIPTION);
        Assert.assertEquals(targetString, mModel.get(SuggestionViewProperties.CONTENT_DESCRIPTION));
    }
}
