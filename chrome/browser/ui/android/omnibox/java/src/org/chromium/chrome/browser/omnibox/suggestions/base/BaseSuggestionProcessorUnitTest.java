// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;

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
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Optional;

/** Tests for {@link BaseSuggestionViewProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowLog.class})
public class BaseSuggestionProcessorUnitTest {
    private static class TestBaseSuggestionProcessor extends BaseSuggestionViewProcessor {
        private final Context mContext;

        public TestBaseSuggestionProcessor(
                Context context,
                SuggestionHost suggestionHost,
                Optional<OmniboxImageSupplier> imageSupplier) {
            super(context, suggestionHost, imageSupplier);
            mContext = context;
        }

        @Override
        public PropertyModel createModel() {
            return new PropertyModel(BaseSuggestionViewProperties.ALL_KEYS);
        }

        @Override
        public boolean doesProcessSuggestion(AutocompleteMatch suggestion, int position) {
            return true;
        }

        @Override
        public int getViewTypeId() {
            return OmniboxSuggestionUiType.DEFAULT;
        }

        @Override
        public void populateModel(AutocompleteMatch suggestion, PropertyModel model, int position) {
            super.populateModel(suggestion, model, position);
            fetchSuggestionFavicon(model, suggestion.getUrl());
        }
    }

    private static final GURL TEST_URL = JUnitTestGURLs.URL_1;

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock SuggestionHost mSuggestionHost;
    private @Mock OmniboxImageSupplier mImageSupplier;
    private @Mock Bitmap mBitmap;

    private Context mContext;
    private TestBaseSuggestionProcessor mProcessor;
    private AutocompleteMatch mSuggestion;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mProcessor =
                new TestBaseSuggestionProcessor(
                        mContext, mSuggestionHost, Optional.of(mImageSupplier));
    }

    /** Create Suggestion for test. */
    private void createSuggestion(int type, boolean isSearch, GURL url) {
        mSuggestion = new AutocompleteMatchBuilder(type).setIsSearch(isSearch).setUrl(url).build();
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mSuggestion, mModel, 0);
    }

    @Test
    public void suggestionFavicons_showFaviconWhenAvailable() {
        final ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        createSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, false, TEST_URL);
        OmniboxDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon1);

        verify(mImageSupplier).fetchFavicon(eq(TEST_URL), callback.capture());
        callback.getValue().onResult(mBitmap);
        OmniboxDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon2);

        Assert.assertNotEquals(icon1, icon2);
        Assert.assertEquals(mBitmap, ((BitmapDrawable) icon2.drawable).getBitmap());
    }

    @Test
    public void suggestionFavicons_doNotReplaceFallbackIconWhenNoFaviconIsAvailable() {
        final ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        createSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, false, TEST_URL);
        OmniboxDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon1);

        verify(mImageSupplier).fetchFavicon(eq(TEST_URL), callback.capture());
        callback.getValue().onResult(null);
        OmniboxDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon2);

        Assert.assertEquals(icon1, icon2);
    }

    @Test
    @DisableFeatures({OmniboxFeatureList.OMNIBOX_TOUCH_DOWN_TRIGGER_FOR_PREFETCH})
    public void touchDownForPrefetch_featureDisabled() {
        createSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, true, TEST_URL);

        Runnable touchDownListener = mModel.get(BaseSuggestionViewProperties.ON_TOUCH_DOWN_EVENT);
        Assert.assertNull(touchDownListener);
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.OMNIBOX_TOUCH_DOWN_TRIGGER_FOR_PREFETCH})
    public void touchDownForPrefetch_featureEnabled() {
        createSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, true, TEST_URL);

        Runnable touchDownListener = mModel.get(BaseSuggestionViewProperties.ON_TOUCH_DOWN_EVENT);
        Assert.assertNotNull(touchDownListener);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(
                                OmniboxMetrics.HISTOGRAM_SEARCH_PREFETCH_TOUCH_DOWN_PROCESS_TIME)
                        .build();

        touchDownListener.run();

        histogramWatcher.assertExpected();
        verify(mSuggestionHost, times(1)).onSuggestionTouchDown(mSuggestion, /* matchIndex= */ 0);
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.OMNIBOX_TOUCH_DOWN_TRIGGER_FOR_PREFETCH})
    public void touchDownForPrefetch_nonSearchSuggestion() {
        // The touch down listener is only added for search suggestions.
        createSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, false, TEST_URL);

        Runnable touchDownListener = mModel.get(BaseSuggestionViewProperties.ON_TOUCH_DOWN_EVENT);
        Assert.assertNull(touchDownListener);
    }

    @Test
    public void setTabSwitchOrRefineAction_refineActionForSearch() {
        createSuggestion(OmniboxSuggestionType.SEARCH_HISTORY, /* isSearch= */ true, TEST_URL);
        mProcessor.setTabSwitchOrRefineAction(mModel, mSuggestion, 0);

        var actions = mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS);
        Assert.assertEquals(1, actions.size());

        var action = actions.get(0);

        var expectedDescription =
                mContext.getResources()
                        .getString(
                                R.string.accessibility_omnibox_btn_refine,
                                mSuggestion.getFillIntoEdit());
        Assert.assertEquals(expectedDescription, action.accessibilityDescription);
        Assert.assertEquals(
                R.drawable.btn_suggestion_refine,
                shadowOf(action.icon.drawable).getCreatedFromResId());

        var monitor = new UserActionTester();
        action.callback.run();
        Assert.assertEquals(1, monitor.getActionCount("MobileOmniboxRefineSuggestion.Search"));
        Assert.assertEquals(1, monitor.getActions().size());
        monitor.tearDown();
    }

    @Test
    public void setTabSwitchOrRefineAction_refineActionForUrl() {
        createSuggestion(OmniboxSuggestionType.HISTORY_URL, /* isSearch= */ false, TEST_URL);
        mProcessor.setTabSwitchOrRefineAction(mModel, mSuggestion, 0);

        var actions = mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS);
        Assert.assertEquals(1, actions.size());

        var action = actions.get(0);

        var expectedDescription =
                mContext.getResources()
                        .getString(
                                R.string.accessibility_omnibox_btn_refine,
                                mSuggestion.getFillIntoEdit());
        Assert.assertEquals(expectedDescription, action.accessibilityDescription);
        Assert.assertEquals(
                R.drawable.btn_suggestion_refine,
                shadowOf(action.icon.drawable).getCreatedFromResId());

        var monitor = new UserActionTester();
        action.callback.run();
        Assert.assertEquals(1, monitor.getActionCount("MobileOmniboxRefineSuggestion.Url"));
        Assert.assertEquals(1, monitor.getActions().size());
        monitor.tearDown();
    }

    @Test
    public void decorationAndActionChipSpacingDefaults() {
        createSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, false, TEST_URL);
        Assert.assertEquals(mModel.get(BaseSuggestionViewProperties.USE_LARGE_DECORATION), false);
        Assert.assertEquals(
                mModel.get(BaseSuggestionViewProperties.ACTION_CHIP_LEAD_IN_SPACING),
                OmniboxResourceProvider.getSuggestionDecorationIconSizeWidth(mContext));

        mModel.set(BaseSuggestionViewProperties.USE_LARGE_DECORATION, true);
        mModel.set(BaseSuggestionViewProperties.ACTION_CHIP_LEAD_IN_SPACING, 43);

        mProcessor.populateModel(mSuggestion, mModel, 0);
        Assert.assertEquals(mModel.get(BaseSuggestionViewProperties.USE_LARGE_DECORATION), false);
        Assert.assertEquals(
                mModel.get(BaseSuggestionViewProperties.ACTION_CHIP_LEAD_IN_SPACING),
                OmniboxResourceProvider.getSuggestionDecorationIconSizeWidth(mContext));
    }
}
