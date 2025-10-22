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
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteUIContext;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionInSuggest;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties.Action;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor.BookmarkState;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.SuggestTemplateInfoProto.SuggestTemplateInfo;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.base.DeviceInput;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;
import java.util.function.Supplier;

/** Tests for {@link BaseSuggestionViewProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowLog.class})
public class BaseSuggestionProcessorUnitTest {
    private static class TestBaseSuggestionProcessor extends BaseSuggestionViewProcessor {
        @SuppressWarnings("HidingField")
        private final Context mContext;

        public TestBaseSuggestionProcessor(AutocompleteUIContext uiContext) {
            super(uiContext);
            mContext = uiContext.context;
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
        public void populateModel(
                AutocompleteInput input,
                AutocompleteMatch suggestion,
                PropertyModel model,
                int position) {
            super.populateModel(input, suggestion, model, position);
            fetchSuggestionFavicon(model, suggestion.getUrl());
        }
    }

    private static final GURL TEST_URL = JUnitTestGURLs.URL_1;

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock SuggestionHost mSuggestionHost;
    private @Mock OmniboxImageSupplier mImageSupplier;
    private @Mock UrlBarEditingTextStateProvider mTextProvider;
    private @Mock Supplier<Tab> mTabSupplier;
    private @Mock Supplier<ShareDelegate> mShareDelegateSupplier;
    private @Mock BookmarkState mBookmarkState;
    private @Mock Bitmap mBitmap;

    private Context mContext;
    private AutocompleteUIContext mUiContext;
    private TestBaseSuggestionProcessor mProcessor;
    private AutocompleteMatch mSuggestion;
    private PropertyModel mModel;
    private AutocompleteInput mInput;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mUiContext =
                new AutocompleteUIContext(
                        mContext,
                        mSuggestionHost,
                        mTextProvider,
                        mImageSupplier,
                        mBookmarkState,
                        mTabSupplier,
                        mShareDelegateSupplier,
                        new ObservableSupplierImpl<>(ControlsPosition.TOP));
        mProcessor = new TestBaseSuggestionProcessor(mUiContext);
        mInput = new AutocompleteInput();
        mInput.setPageClassification(
                PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE);
    }

    /** Create Suggestion for test. */
    private void createSuggestion(int type, boolean isSearch, boolean hasTabMatch, GURL url) {
        mSuggestion =
                new AutocompleteMatchBuilder(type)
                        .setIsSearch(isSearch)
                        .setUrl(url)
                        .setHasTabMatch(hasTabMatch)
                        .build();
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mInput, mSuggestion, mModel, 0);
    }

    private void createSuggestionWithActions(
            int type, boolean isSearch, GURL url, List<OmniboxAction> actions) {
        mSuggestion =
                new AutocompleteMatchBuilder(type)
                        .setIsSearch(isSearch)
                        .setUrl(url)
                        .setActions(actions)
                        .build();
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mInput, mSuggestion, mModel, 0);
    }

    private void createDeletableSuggestion(int type, boolean isSearch, GURL url) {
        mSuggestion =
                new AutocompleteMatchBuilder(type)
                        .setIsSearch(isSearch)
                        .setUrl(url)
                        .setDeletable(true)
                        .build();
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mInput, mSuggestion, mModel, 0);
    }

    private Action setUpDeleteScenarioForRemoveActionTesting() {
        // Recreate the suggestion processor to respect any overridden features and params.
        mProcessor = new TestBaseSuggestionProcessor(mUiContext);

        createDeletableSuggestion(
                OmniboxSuggestionType.SEARCH_HISTORY, /* isSearch= */ true, TEST_URL);
        mProcessor.setRemoveOrRefineAction(mModel, mInput, mSuggestion, 0);

        var actions = mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS);
        Assert.assertEquals(1, actions.size());

        return actions.get(0);
    }

    @Test
    public void suggestionFavicons_showFaviconWhenAvailable() {
        final ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        createSuggestion(
                OmniboxSuggestionType.URL_WHAT_YOU_TYPED,
                /* isSearch= */ false,
                /* hasTabMatch= */ false,
                TEST_URL);
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
        createSuggestion(
                OmniboxSuggestionType.URL_WHAT_YOU_TYPED,
                /* isSearch= */ false,
                /* hasTabMatch= */ false,
                TEST_URL);
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
        createSuggestion(
                OmniboxSuggestionType.URL_WHAT_YOU_TYPED,
                /* isSearch= */ true,
                /* hasTabMatch= */ false,
                TEST_URL);

        Runnable touchDownListener = mModel.get(BaseSuggestionViewProperties.ON_TOUCH_DOWN_EVENT);
        Assert.assertNull(touchDownListener);
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.OMNIBOX_TOUCH_DOWN_TRIGGER_FOR_PREFETCH})
    public void touchDownForPrefetch_featureEnabled() {
        createSuggestion(
                OmniboxSuggestionType.URL_WHAT_YOU_TYPED,
                /* isSearch= */ true,
                /* hasTabMatch= */ false,
                TEST_URL);

        Runnable touchDownListener = mModel.get(BaseSuggestionViewProperties.ON_TOUCH_DOWN_EVENT);
        Assert.assertNotNull(touchDownListener);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(
                                OmniboxMetrics.HISTOGRAM_SEARCH_PREFETCH_TOUCH_DOWN_PROCESS_TIME)
                        .build();

        touchDownListener.run();

        histogramWatcher.assertExpected();
        verify(mSuggestionHost, times(1)).onSuggestionTouchDown(mSuggestion, /* position= */ 0);
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.OMNIBOX_TOUCH_DOWN_TRIGGER_FOR_PREFETCH})
    public void touchDownForPrefetch_nonSearchSuggestion() {
        // The touch down listener is only added for search suggestions.
        createSuggestion(
                OmniboxSuggestionType.URL_WHAT_YOU_TYPED,
                /* isSearch= */ false,
                /* hasTabMatch= */ false,
                TEST_URL);

        Runnable touchDownListener = mModel.get(BaseSuggestionViewProperties.ON_TOUCH_DOWN_EVENT);
        Assert.assertNull(touchDownListener);
    }

    @Test
    public void setRemoveOrRefineAction_refineActionForSearch() {
        createSuggestion(
                OmniboxSuggestionType.SEARCH_HISTORY,
                /* isSearch= */ true,
                /* hasTabMatch= */ false,
                TEST_URL);
        mProcessor.setRemoveOrRefineAction(mModel, mInput, mSuggestion, 0);

        var actions = mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS);
        Assert.assertEquals(1, actions.size());

        var action = actions.get(0);

        var expectedDescription =
                mContext.getString(
                        R.string.accessibility_omnibox_btn_refine, mSuggestion.getFillIntoEdit());
        Assert.assertEquals(expectedDescription, action.accessibilityDescription);
        Assert.assertEquals(
                R.drawable.btn_suggestion_refine_up,
                shadowOf(action.icon.drawable).getCreatedFromResId());

        var monitor = new UserActionTester();
        action.callback.run();
        Assert.assertEquals(1, monitor.getActionCount("MobileOmniboxRefineSuggestion.Search"));
        Assert.assertEquals(1, monitor.getActions().size());
        monitor.tearDown();
    }

    @Test
    public void setRemoveOrRefineAction_refineActionForUrl() {
        createSuggestion(
                OmniboxSuggestionType.HISTORY_URL,
                /* isSearch= */ false,
                /* hasTabMatch= */ false,
                TEST_URL);
        mProcessor.setRemoveOrRefineAction(mModel, mInput, mSuggestion, 0);

        var actions = mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS);
        Assert.assertEquals(1, actions.size());

        var action = actions.get(0);

        var expectedDescription =
                mContext.getString(
                        R.string.accessibility_omnibox_btn_refine, mSuggestion.getFillIntoEdit());
        Assert.assertEquals(expectedDescription, action.accessibilityDescription);
        // Note: shadows don't work with vector drawables.

        var monitor = new UserActionTester();
        action.callback.run();
        Assert.assertEquals(1, monitor.getActionCount("MobileOmniboxRefineSuggestion.Url"));
        Assert.assertEquals(1, monitor.getActions().size());
        monitor.tearDown();
    }

    @Test
    @Config(qualifiers = "w400dp")
    @EnableFeatures({OmniboxFeatureList.OMNIBOX_IMPROVEMENT_FOR_LFF})
    public void setRemoveOrRefineAction_noRmoveActionOnPhone() {
        DeviceInput.setSupportsPrecisionPointerForTesting(true);
        OmniboxFeatures.sOmniboxImprovementForLFFRemoveSuggestionViaButton.setForTesting(true);

        var action = setUpDeleteScenarioForRemoveActionTesting();

        // Refine action is shown instead.
        var expectedDescription =
                mContext.getString(
                        R.string.accessibility_omnibox_btn_refine, mSuggestion.getFillIntoEdit());
        Assert.assertEquals(expectedDescription, action.accessibilityDescription);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures({OmniboxFeatureList.OMNIBOX_IMPROVEMENT_FOR_LFF})
    public void setRemoveOrRefineAction_noRemoveActionOnTabletWithoutPeripherals() {
        DeviceInput.setSupportsAlphabeticKeyboardForTesting(false);
        DeviceInput.setSupportsPrecisionPointerForTesting(false);
        OmniboxFeatures.sOmniboxImprovementForLFFRemoveSuggestionViaButton.setForTesting(true);

        var action = setUpDeleteScenarioForRemoveActionTesting();

        // Refine action is shown instead.
        var expectedDescription =
                mContext.getString(
                        R.string.accessibility_omnibox_btn_refine, mSuggestion.getFillIntoEdit());
        Assert.assertEquals(expectedDescription, action.accessibilityDescription);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures({OmniboxFeatureList.OMNIBOX_IMPROVEMENT_FOR_LFF})
    public void setRemoveOrRefineAction_removeActionOnTabletWithAlphabeticKeyboard() {
        DeviceInput.setSupportsAlphabeticKeyboardForTesting(true);
        OmniboxFeatures.sOmniboxImprovementForLFFRemoveSuggestionViaButton.setForTesting(true);

        var action = setUpDeleteScenarioForRemoveActionTesting();

        var expectedDescription =
                mContext.getString(
                        R.string.accessibility_omnibox_remove_suggestion,
                        mSuggestion.getFillIntoEdit());
        Assert.assertEquals(expectedDescription, action.accessibilityDescription);
        Assert.assertEquals(
                R.drawable.btn_close, shadowOf(action.icon.drawable).getCreatedFromResId());

        var monitor = new UserActionTester();
        action.callback.run();
        Assert.assertEquals(1, monitor.getActionCount("MobileOmniboxRemoveSuggestion.Button"));
        Assert.assertEquals(1, monitor.getActions().size());
        monitor.tearDown();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures({OmniboxFeatureList.OMNIBOX_IMPROVEMENT_FOR_LFF})
    public void setRemoveOrRefineAction_removeActionOnTabletWithPrecisionPointer() {
        DeviceInput.setSupportsPrecisionPointerForTesting(true);
        OmniboxFeatures.sOmniboxImprovementForLFFRemoveSuggestionViaButton.setForTesting(true);

        var action = setUpDeleteScenarioForRemoveActionTesting();

        var expectedDescription =
                mContext.getString(
                        R.string.accessibility_omnibox_remove_suggestion,
                        mSuggestion.getFillIntoEdit());
        Assert.assertEquals(expectedDescription, action.accessibilityDescription);
        Assert.assertEquals(
                R.drawable.btn_close, shadowOf(action.icon.drawable).getCreatedFromResId());

        var monitor = new UserActionTester();
        action.callback.run();
        Assert.assertEquals(1, monitor.getActionCount("MobileOmniboxRemoveSuggestion.Button"));
        Assert.assertEquals(1, monitor.getActions().size());
        monitor.tearDown();
    }

    @Test
    public void decorationAndActionChipSpacingDefaults() {
        createSuggestion(
                OmniboxSuggestionType.URL_WHAT_YOU_TYPED,
                /* isSearch= */ false,
                /* hasTabMatch= */ false,
                TEST_URL);
        Assert.assertEquals(false, mModel.get(BaseSuggestionViewProperties.USE_LARGE_DECORATION));
        Assert.assertEquals(
                mModel.get(BaseSuggestionViewProperties.ACTION_CHIP_LEAD_IN_SPACING),
                OmniboxResourceProvider.getSuggestionDecorationIconSizeWidth(mContext));

        mModel.set(BaseSuggestionViewProperties.USE_LARGE_DECORATION, true);
        mModel.set(BaseSuggestionViewProperties.ACTION_CHIP_LEAD_IN_SPACING, 43);

        mProcessor.populateModel(mInput, mSuggestion, mModel, 0);
        Assert.assertEquals(false, mModel.get(BaseSuggestionViewProperties.USE_LARGE_DECORATION));
        Assert.assertEquals(
                mModel.get(BaseSuggestionViewProperties.ACTION_CHIP_LEAD_IN_SPACING),
                OmniboxResourceProvider.getSuggestionDecorationIconSizeWidth(mContext));
    }

    @Test
    public void addActionButtonIfAvailable() {
        // No action button.
        {
            createSuggestion(
                    OmniboxSuggestionType.OPEN_TAB,
                    /* isSearch= */ false,
                    /* hasTabMatch= */ false,
                    TEST_URL);
            var actions = mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS);
            Assert.assertEquals(null, actions);
        }

        // No action button.
        {
            createSuggestionWithActions(
                    OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED,
                    /* isSearch= */ true,
                    TEST_URL,
                    List.of(
                            new OmniboxActionInSuggest(
                                    0,
                                    "hint",
                                    "accessibility",
                                    SuggestTemplateInfo.TemplateAction.ActionType.REVIEWS_VALUE,
                                    "https://google.com",
                                    /* tabId= */ 0,
                                    /* showAsActionButton= */ false)));

            var actions = mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS);
            Assert.assertEquals(null, actions);
        }

        // One action button is added.
        {
            createSuggestionWithActions(
                    OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED,
                    /* isSearch= */ true,
                    TEST_URL,
                    List.of(
                            new OmniboxActionInSuggest(
                                    0,
                                    "hint",
                                    "accessibility",
                                    SuggestTemplateInfo.TemplateAction.ActionType.REVIEWS_VALUE,
                                    "https://google.com",
                                    /* tabId= */ 0,
                                    /* showAsActionButton= */ false),
                            new OmniboxActionInSuggest(
                                    0,
                                    "hint2",
                                    "accessibility2",
                                    SuggestTemplateInfo.TemplateAction.ActionType.CHROME_AIM_VALUE,
                                    "https://google.com",
                                    /* tabId= */ 0,
                                    /* showAsActionButton= */ true),
                            new OmniboxActionInSuggest(
                                    0,
                                    "hint3",
                                    "accessibility3",
                                    SuggestTemplateInfo.TemplateAction.ActionType.CHROME_AIM_VALUE,
                                    "https://google.com",
                                    /* tabId= */ 0,
                                    /* showAsActionButton= */ true)));

            var actions = mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS);
            Assert.assertEquals(1, actions.size());

            var action = actions.get(0);

            Assert.assertEquals("accessibility2", action.accessibilityDescription);
            Assert.assertEquals(
                    R.drawable.search_spark_rainbow,
                    shadowOf(action.icon.drawable).getCreatedFromResId());
        }
    }

    @Test
    public void addActionButtonIfAvailable_HubPageClassificationSkipsButton() {
        // When the ANDROID_HUB PageClassification is seen, the action button is intentionally
        // skipped.
        mInput.setPageClassification(PageClassification.ANDROID_HUB_VALUE);

        createSuggestionWithActions(
                OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED,
                /* isSearch= */ true,
                TEST_URL,
                List.of(
                        new OmniboxActionInSuggest(
                                0,
                                "hint",
                                "accessibility",
                                SuggestTemplateInfo.TemplateAction.ActionType.REVIEWS_VALUE,
                                "https://google.com",
                                /* tabId= */ 0,
                                /* showAsActionButton= */ true)));

        var actions = mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS);
        Assert.assertEquals(null, actions);
    }

    @Test
    public void addTabSwitchActionButton() {
        createSuggestionWithActions(
                OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED,
                /* isSearch= */ true,
                TEST_URL,
                List.of(
                        new OmniboxActionInSuggest(
                                0,
                                "hint",
                                "accessibility",
                                SuggestTemplateInfo.TemplateAction.ActionType
                                        .CHROME_TAB_SWITCH_VALUE,
                                "https://google.com",
                                /* tabId= */ 0,
                                /* showAsActionButton= */ true)));

        var actions = mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS);
        Assert.assertEquals(1, actions.size());

        var action = actions.get(0);
        Assert.assertEquals(
                R.drawable.switch_to_tab, shadowOf(action.icon.drawable).getCreatedFromResId());
    }
}
