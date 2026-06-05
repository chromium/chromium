// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.DrawableRes;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteUIContext;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionInSuggest;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.SuggestTemplateInfoProto.SuggestTemplateInfo;
import org.chromium.components.omnibox.action.ActionPresentationMode;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.search_engines.StarterPackId;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.MockitoHelper;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Supplier;

/** Tests for {@link BasicSuggestionProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BasicSuggestionProcessorUnitTest {
    private static final @DrawableRes int ICON_BOOKMARK = R.drawable.ic_star_24dp;
    private static final @DrawableRes int ICON_GLOBE = R.drawable.ic_globe_24dp;
    private static final @DrawableRes int ICON_HISTORY = R.drawable.ic_history_24dp;
    private static final @DrawableRes int ICON_MAGNIFIER = R.drawable.ic_suggestion_magnifier;
    private static final @DrawableRes int ICON_TRENDS = R.drawable.trending_up_black_24dp;
    private static final @DrawableRes int ICON_VOICE = R.drawable.ic_mic_white_24dp;
    private static final @DrawableRes int ICON_FAVICON = 0; // Favicons do not come from resources.

    private static final Map<Integer, String> ICON_TYPE_NAMES;

    static {
        Map<Integer, String> map = new HashMap<>();
        map.put(ICON_BOOKMARK, "BOOKMARK");
        map.put(ICON_HISTORY, "HISTORY");
        map.put(ICON_GLOBE, "GLOBE");
        map.put(ICON_MAGNIFIER, "MAGNIFIER");
        map.put(ICON_VOICE, "VOICE");
        map.put(ICON_FAVICON, "FAVICON");
        ICON_TYPE_NAMES = map;
    }

    private static final Map<Integer, String> SUGGESTION_TYPE_NAMES;

    static {
        Map<Integer, String> map = new HashMap<>();
        map.put(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, "URL_WHAT_YOU_TYPED");
        map.put(OmniboxSuggestionType.HISTORY_URL, "HISTORY_URL");
        map.put(OmniboxSuggestionType.HISTORY_TITLE, "HISTORY_TITLE");
        map.put(OmniboxSuggestionType.HISTORY_BODY, "HISTORY_BODY");
        map.put(OmniboxSuggestionType.HISTORY_KEYWORD, "HISTORY_KEYWORD");
        map.put(OmniboxSuggestionType.NAVSUGGEST, "NAVSUGGEST");
        map.put(OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED, "SEARCH_WHAT_YOU_TYPED");
        map.put(OmniboxSuggestionType.SEARCH_HISTORY, "SEARCH_HISTORY");
        map.put(OmniboxSuggestionType.SEARCH_SUGGEST, "SEARCH_SUGGEST");
        map.put(OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY, "SEARCH_SUGGEST_ENTITY");
        map.put(OmniboxSuggestionType.SEARCH_SUGGEST_TAIL, "SEARCH_SUGGEST_TAIL");
        map.put(OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED, "SEARCH_SUGGEST_PERSONALIZED");
        map.put(OmniboxSuggestionType.SEARCH_SUGGEST_PROFILE, "SEARCH_SUGGEST_PROFILE");
        map.put(OmniboxSuggestionType.SEARCH_OTHER_ENGINE, "SEARCH_OTHER_ENGINE");
        map.put(OmniboxSuggestionType.NAVSUGGEST_PERSONALIZED, "NAVSUGGEST_PERSONALIZED");
        map.put(OmniboxSuggestionType.VOICE_SUGGEST, "VOICE_SUGGEST");
        map.put(OmniboxSuggestionType.DOCUMENT_SUGGESTION, "DOCUMENT_SUGGESTION");
        // Note: CALCULATOR suggestions are not handled by basic suggestion processor.
        // These suggestions are now processed by AnswerSuggestionProcessor instead.
        SUGGESTION_TYPE_NAMES = map;
    }

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock SuggestionHost mSuggestionHost;
    private @Mock Bitmap mBitmap;
    private @Mock OmniboxImageSupplier mImageSupplier;
    private @Mock Supplier<Tab> mTabSupplier;
    private @Mock Supplier<ShareDelegate> mShareDelegateSupplier;
    private @Mock OmniboxActionDelegate mActionDelegate;

    private BasicSuggestionProcessor mProcessor;
    private AutocompleteMatch mSuggestion;
    private PropertyModel mModel;
    private AutocompleteInput mInput;

    private static class BookmarkPredicate implements BasicSuggestionProcessor.BookmarkState {
        boolean mState;

        @Override
        public boolean isBookmarked(GURL url) {
            return mState;
        }
    }

    private final BookmarkPredicate mIsBookmarked = new BookmarkPredicate();

    @Before
    public void setUp() {
        AutocompleteUIContext uiContext =
                new AutocompleteUIContext(
                        ContextUtils.getApplicationContext(),
                        mSuggestionHost,
                        null,
                        mImageSupplier,
                        mIsBookmarked,
                        mTabSupplier,
                        mShareDelegateSupplier,
                        ObservableSuppliers.createNonNull(ControlsPosition.TOP),
                        mActionDelegate);
        mProcessor = new BasicSuggestionProcessor(uiContext);
        mInput = new AutocompleteInput();
        OmniboxResourceProvider.disableCachesForTesting();
    }

    @After
    public void tearDown() {
        OmniboxResourceProvider.reenableCachesForTesting();
        mInput.reset();
    }

    /**
     * Create Suggestion for test. Do not use directly; use helper methods to create specific
     * suggestion type instead.
     */
    private AutocompleteMatchBuilder createSuggestionBuilder(int type, String title) {
        return AutocompleteMatchBuilder.searchWithType(type).setDisplayText(title);
    }

    /** Create search suggestion for test. */
    private void createSearchSuggestion(int type, String title) {
        mSuggestion = createSuggestionBuilder(type, title).setIsSearch(true).build();
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mInput, mSuggestion, mModel, 0);
    }

    /** Create URL suggestion with supplied text and target URL for test. */
    private void createUrlSuggestion(int type, String title, GURL url) {
        mSuggestion = createSuggestionBuilder(type, title).setIsSearch(false).setUrl(url).build();
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mInput, mSuggestion, mModel, 0);
    }

    /** Create URL suggestion for test. */
    private void createUrlSuggestion(int type, String title) {
        createUrlSuggestion(type, title, GURL.emptyGURL());
    }

    /** Create switch to tab suggestion for test. */
    private void createSwitchToTabSuggestion(int type) {
        mSuggestion =
                new AutocompleteMatchBuilder(type)
                        .setIsSearch(true)
                        .setHasTabMatch(true)
                        .setUrl(JUnitTestGURLs.URL_1)
                        .setDisplayText("tab switch")
                        .setActions(
                                List.of(
                                        new OmniboxActionInSuggest(
                                                0,
                                                "tab switch",
                                                "tab switch",
                                                SuggestTemplateInfo.TemplateAction.ActionType
                                                        .CHROME_TAB_SWITCH_VALUE,
                                                "https://google.com",
                                                /* tabId= */ 0,
                                                ActionPresentationMode.BUTTON)))
                        .build();
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mInput, mSuggestion, mModel, 0);
    }

    private void assertSuggestionTypeAndIcon(
            @OmniboxSuggestionType int expectedType, @DrawableRes int expectedIconRes) {
        OmniboxDrawableState sds = mModel.get(BaseSuggestionViewProperties.ICON);
        @DrawableRes int actualIconRes = shadowOf(sds.drawable).getCreatedFromResId();
        assertEquals(
                String.format(
                        "%s: Want Icon %s, Got %s",
                        SUGGESTION_TYPE_NAMES.get(expectedType),
                        ICON_TYPE_NAMES.get(expectedIconRes),
                        ICON_TYPE_NAMES.get(actualIconRes)),
                expectedIconRes,
                actualIconRes);
    }

    @Test
    @SmallTest
    public void getSuggestionIconTypeForSearch_Default() {
        int[][] testCases = {
            {OmniboxSuggestionType.URL_WHAT_YOU_TYPED, ICON_MAGNIFIER},
            {OmniboxSuggestionType.HISTORY_URL, ICON_MAGNIFIER},
            {OmniboxSuggestionType.HISTORY_TITLE, ICON_MAGNIFIER},
            {OmniboxSuggestionType.HISTORY_BODY, ICON_MAGNIFIER},
            {OmniboxSuggestionType.HISTORY_KEYWORD, ICON_MAGNIFIER},
            {OmniboxSuggestionType.NAVSUGGEST, ICON_MAGNIFIER},
            {OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED, ICON_MAGNIFIER},
            {OmniboxSuggestionType.SEARCH_HISTORY, ICON_HISTORY},
            {OmniboxSuggestionType.SEARCH_SUGGEST, ICON_MAGNIFIER},
            {OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY, ICON_MAGNIFIER},
            {OmniboxSuggestionType.SEARCH_SUGGEST_TAIL, ICON_MAGNIFIER},
            {OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED, ICON_HISTORY},
            {OmniboxSuggestionType.SEARCH_SUGGEST_PROFILE, ICON_MAGNIFIER},
            {OmniboxSuggestionType.SEARCH_OTHER_ENGINE, ICON_MAGNIFIER},
            {OmniboxSuggestionType.NAVSUGGEST_PERSONALIZED, ICON_MAGNIFIER},
            {OmniboxSuggestionType.VOICE_SUGGEST, ICON_VOICE},
            {OmniboxSuggestionType.DOCUMENT_SUGGESTION, ICON_MAGNIFIER},
        };

        mProcessor.onNativeInitialized();
        for (int[] testCase : testCases) {
            createSearchSuggestion(testCase[0], "");
            assertTrue(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
            assertSuggestionTypeAndIcon(testCase[0], testCase[1]);
        }
    }

    @Test
    @SmallTest
    public void getSuggestionIconTypeForUrl_Default() {
        int[][] testCases = {
            {OmniboxSuggestionType.URL_WHAT_YOU_TYPED, ICON_GLOBE},
            {OmniboxSuggestionType.HISTORY_URL, ICON_GLOBE},
            {OmniboxSuggestionType.HISTORY_TITLE, ICON_GLOBE},
            {OmniboxSuggestionType.HISTORY_BODY, ICON_GLOBE},
            {OmniboxSuggestionType.HISTORY_KEYWORD, ICON_GLOBE},
            {OmniboxSuggestionType.NAVSUGGEST, ICON_GLOBE},
            {OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED, ICON_GLOBE},
            {OmniboxSuggestionType.SEARCH_HISTORY, ICON_GLOBE},
            {OmniboxSuggestionType.SEARCH_SUGGEST, ICON_GLOBE},
            {OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY, ICON_GLOBE},
            {OmniboxSuggestionType.SEARCH_SUGGEST_TAIL, ICON_GLOBE},
            {OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED, ICON_GLOBE},
            {OmniboxSuggestionType.SEARCH_SUGGEST_PROFILE, ICON_GLOBE},
            {OmniboxSuggestionType.SEARCH_OTHER_ENGINE, ICON_GLOBE},
            {OmniboxSuggestionType.NAVSUGGEST_PERSONALIZED, ICON_GLOBE},
            {OmniboxSuggestionType.VOICE_SUGGEST, ICON_GLOBE},
            {OmniboxSuggestionType.DOCUMENT_SUGGESTION, ICON_GLOBE},
        };

        mProcessor.onNativeInitialized();
        for (int[] testCase : testCases) {
            createUrlSuggestion(testCase[0], "");
            assertFalse(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
            assertSuggestionTypeAndIcon(testCase[0], testCase[1]);
        }
    }

    @Test
    @SmallTest
    public void getSuggestionIconTypeForBookmarks_Default() {
        int[][] testCases = {
            {OmniboxSuggestionType.URL_WHAT_YOU_TYPED, ICON_BOOKMARK},
            {OmniboxSuggestionType.HISTORY_URL, ICON_BOOKMARK},
            {OmniboxSuggestionType.HISTORY_TITLE, ICON_BOOKMARK},
            {OmniboxSuggestionType.HISTORY_BODY, ICON_BOOKMARK},
            {OmniboxSuggestionType.HISTORY_KEYWORD, ICON_BOOKMARK},
            {OmniboxSuggestionType.NAVSUGGEST, ICON_BOOKMARK},
            {OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED, ICON_BOOKMARK},
            {OmniboxSuggestionType.SEARCH_HISTORY, ICON_BOOKMARK},
            {OmniboxSuggestionType.SEARCH_SUGGEST, ICON_BOOKMARK},
            {OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY, ICON_BOOKMARK},
            {OmniboxSuggestionType.SEARCH_SUGGEST_TAIL, ICON_BOOKMARK},
            {OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED, ICON_BOOKMARK},
            {OmniboxSuggestionType.SEARCH_SUGGEST_PROFILE, ICON_BOOKMARK},
            {OmniboxSuggestionType.SEARCH_OTHER_ENGINE, ICON_BOOKMARK},
            {OmniboxSuggestionType.NAVSUGGEST_PERSONALIZED, ICON_BOOKMARK},
            {OmniboxSuggestionType.VOICE_SUGGEST, ICON_BOOKMARK},
            {OmniboxSuggestionType.DOCUMENT_SUGGESTION, ICON_BOOKMARK},
        };

        mIsBookmarked.mState = true;

        mProcessor.onNativeInitialized();
        for (int[] testCase : testCases) {
            createUrlSuggestion(testCase[0], "");
            assertFalse(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
            assertSuggestionTypeAndIcon(testCase[0], testCase[1]);
        }
    }

    @Test
    @SmallTest
    public void getSuggestionIconTypeForTrendingQueries() {
        int[][] testCases = {
            {OmniboxSuggestionType.URL_WHAT_YOU_TYPED, ICON_TRENDS},
            {OmniboxSuggestionType.SEARCH_HISTORY, ICON_HISTORY},
            {OmniboxSuggestionType.SEARCH_SUGGEST, ICON_TRENDS},
            {OmniboxSuggestionType.SEARCH_SUGGEST_TAIL, ICON_TRENDS},
            {OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED, ICON_HISTORY},
            {OmniboxSuggestionType.VOICE_SUGGEST, ICON_VOICE},
        };

        mProcessor.onNativeInitialized();
        for (int[] testCase : testCases) {
            mSuggestion = createSuggestionBuilder(testCase[0], "").addSubtype(143).build();
            mModel = mProcessor.createModel();
            mProcessor.populateModel(mInput, mSuggestion, mModel, 0);
            assertTrue(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
            assertSuggestionTypeAndIcon(testCase[0], testCase[1]);
        }
    }

    @Test
    public void getFallbackIconFromIconType_validIconForEachType() {
        for (var iconType : SuggestTemplateInfo.IconType.values()) {
            if (iconType == SuggestTemplateInfo.IconType.ICON_TYPE_UNSPECIFIED) {
                assertEquals(0, mProcessor.getFallbackIconFromIconType(iconType.getNumber()));
            } else {
                assertNotEquals(0, mProcessor.getFallbackIconFromIconType(iconType.getNumber()));
            }
        }
    }

    @Test
    @SmallTest
    public void refineIconNotShownForWhatYouTypedSuggestions() {
        final String typed = "Typed content";
        createSearchSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, typed);
        PropertyModel model = mProcessor.createModel();
        mProcessor.populateModel(mInput, mSuggestion, model, 0);
        assertNull(mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS));

        createUrlSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, typed);
        mProcessor.populateModel(mInput, mSuggestion, model, 0);
        assertNull(mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS));
    }

    @Test
    @SmallTest
    public void refineIconShownForRefineSuggestions() {
        final String typed = "Typed content";
        createSearchSuggestion(OmniboxSuggestionType.SEARCH_SUGGEST, typed);
        PropertyModel model = mProcessor.createModel();
        mProcessor.populateModel(mInput, mSuggestion, model, 0);
        assertNotNull(mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS));

        createUrlSuggestion(OmniboxSuggestionType.HISTORY_URL, typed);
        mProcessor.populateModel(mInput, mSuggestion, model, 0);
        assertNotNull(mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS));

        final List<BaseSuggestionViewProperties.Action> actions =
                mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS);
        assertEquals(1, actions.size());
        final OmniboxDrawableState iconState = actions.get(0).icon;
        assertEquals(
                R.drawable.btn_suggestion_refine_up,
                shadowOf(iconState.drawable).getCreatedFromResId());
    }

    @Test
    @SmallTest
    public void refineIcon_notShownForQueryTiles() {
        createSearchSuggestion(OmniboxSuggestionType.TILE_SUGGESTION, "Music");
        PropertyModel model = mProcessor.createModel();
        mProcessor.populateModel(mInput, mSuggestion, model, 0);
        assertNull(mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS));
    }

    @Test
    @SmallTest
    public void switchTabIcon_shownForSwitchToTabSuggestions() {
        mInput.setPageClassification(
                PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE);

        createSwitchToTabSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED);
        PropertyModel model = mProcessor.createModel();

        mProcessor.populateModel(mInput, mSuggestion, model, 0);
        assertNotNull(mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS));

        final List<BaseSuggestionViewProperties.Action> actions =
                mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS);
        assertEquals(1, actions.size());
        final OmniboxDrawableState iconState = actions.get(0).icon;
        assertEquals(R.drawable.switch_to_tab, shadowOf(iconState.drawable).getCreatedFromResId());
    }

    @Test
    @SmallTest
    public void suggestionFavicons_showFaviconWhenAvailable() {
        final ArgumentCaptor<Callback<Drawable>> callback = MockitoHelper.callbackCaptor();
        mProcessor.onNativeInitialized();
        createUrlSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, "");
        OmniboxDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        assertNotNull(icon1);

        verify(mImageSupplier).fetchFavicon(eq(mSuggestion.getUrl()), callback.capture());
        callback.getValue()
                .onResult(
                        new BitmapDrawable(
                                ContextUtils.getApplicationContext().getResources(), mBitmap));
        OmniboxDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        assertNotNull(icon2);

        assertNotEquals(icon1, icon2);
        assertEquals(mBitmap, ((BitmapDrawable) icon2.drawable).getBitmap());
    }

    @Test
    @SmallTest
    public void suggestionFavicons_doNotFetchForSearchSuggestions() {
        mProcessor.onNativeInitialized();
        createSearchSuggestion(OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED, "");

        verify(mImageSupplier, never()).fetchFavicon(any(), any());
    }

    @Test
    @SmallTest
    public void suggestionFavicons_doNotFetchForBookmarked() {
        mProcessor.onNativeInitialized();
        mIsBookmarked.mState = true;
        createUrlSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, "");

        verify(mImageSupplier, never()).fetchFavicon(any(), any());
    }

    @Test
    @SmallTest
    public void suggestionFavicons_doNotReplaceFallbackIconWhenNoFaviconIsAvailable() {
        final ArgumentCaptor<Callback<Drawable>> callback = MockitoHelper.callbackCaptor();
        mProcessor.onNativeInitialized();
        createUrlSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, "");
        OmniboxDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        assertNotNull(icon1);

        verify(mImageSupplier).fetchFavicon(eq(mSuggestion.getUrl()), callback.capture());
        callback.getValue().onResult(null);
        OmniboxDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        assertNotNull(icon2);

        assertEquals(icon1, icon2);
    }

    @Test
    @SmallTest
    public void searchSuggestions_searchQueriesCanWrapAroundWithFeatureEnabled() {
        mProcessor.onNativeInitialized();
        createSearchSuggestion(OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED, "");
        assertEquals(true, mModel.get(SuggestionViewProperties.ALLOW_WRAP_AROUND));

        createUrlSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, "");
        assertEquals(false, mModel.get(SuggestionViewProperties.ALLOW_WRAP_AROUND));
    }

    @Test
    @SmallTest
    public void internalUrlSuggestions_doNotPresentInternalScheme() {
        mProcessor.onNativeInitialized();
        // URLs that are rejected by UrlBarData should not be presented to the User.
        UrlBarData.setShouldShowUrlForTesting(false);
        createUrlSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, "", JUnitTestGURLs.URL_1);
        assertNull(mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT));
    }

    @Test
    @SmallTest
    public void starterPackSuggestions_fallbackIcons() {
        mProcessor.onNativeInitialized();

        mSuggestion =
                createSuggestionBuilder(OmniboxSuggestionType.STARTER_PACK, "")
                        .setIsSearch(false)
                        .setStarterPackId(StarterPackId.BOOKMARKS)
                        .build();
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mInput, mSuggestion, mModel, 0);
        OmniboxDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        assertEquals(R.drawable.ic_star_24dp, shadowOf(icon1.drawable).getCreatedFromResId());

        mSuggestion =
                createSuggestionBuilder(OmniboxSuggestionType.STARTER_PACK, "")
                        .setIsSearch(false)
                        .setStarterPackId(StarterPackId.HISTORY)
                        .build();
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mInput, mSuggestion, mModel, 0);
        OmniboxDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        assertEquals(R.drawable.ic_history_24dp, shadowOf(icon2.drawable).getCreatedFromResId());

        mSuggestion =
                createSuggestionBuilder(OmniboxSuggestionType.STARTER_PACK, "")
                        .setIsSearch(false)
                        .setStarterPackId(StarterPackId.TABS)
                        .build();
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mInput, mSuggestion, mModel, 0);
        OmniboxDrawableState icon3 = mModel.get(BaseSuggestionViewProperties.ICON);
        assertEquals(R.drawable.switch_to_tab, shadowOf(icon3.drawable).getCreatedFromResId());

        mSuggestion =
                createSuggestionBuilder(OmniboxSuggestionType.STARTER_PACK, "")
                        .setIsSearch(false)
                        .setStarterPackId(StarterPackId.GEMINI)
                        .build();
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mInput, mSuggestion, mModel, 0);
        OmniboxDrawableState icon4 = mModel.get(BaseSuggestionViewProperties.ICON);
        assertEquals(R.drawable.ic_spark_4c_16dp, shadowOf(icon4.drawable).getCreatedFromResId());
    }

    @Test
    @SmallTest
    public void topPaddingDefaultZero() {
        createUrlSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, "");
        assertEquals(0, mModel.get(BaseSuggestionViewProperties.TOP_PADDING));
    }

    @Test
    @SmallTest
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_ITEM_DECORATION)
    public void accessibilityAnnouncements_groupedSearchSuggestions() {
        mProcessor.onNativeInitialized();
        mSuggestion =
                createSuggestionBuilder(OmniboxSuggestionType.SEARCH_SUGGEST, "Google")
                        .setIsSearch(true)
                        .setDescription("Technology corporation")
                        .build();
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mInput, mSuggestion, mModel, 0);

        mModel.set(SuggestionCommonProperties.HEADER_TITLE, "Trending Searches");
        mModel.set(SuggestionCommonProperties.INDEX_IN_GROUP, 1);
        mModel.set(SuggestionCommonProperties.TOTAL_IN_GROUP, 5);

        mProcessor.populateModel(mInput, mSuggestion, mModel, 0);

        String expectedAnnouncement =
                "Google, Technology corporation. Search. 2 of 5 in the group Trending"
                        + " Searches.";
        assertEquals(
                expectedAnnouncement, mModel.get(SuggestionViewProperties.CONTENT_DESCRIPTION));
    }

    @Test
    @SmallTest
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_ITEM_DECORATION)
    public void accessibilityAnnouncements_groupedAiModeSuggestions() {
        mProcessor.onNativeInitialized();
        mSuggestion =
                createSuggestionBuilder(OmniboxSuggestionType.SEARCH_SUGGEST, "Gemini")
                        .setSuggestionKind(
                                org.chromium.components.omnibox.OmniboxSuggestionKind.CONVERSATION)
                        .setDescription("AI Mode")
                        .build();
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mInput, mSuggestion, mModel, 0);

        mModel.set(SuggestionCommonProperties.HEADER_TITLE, "AI Suggestions");
        mModel.set(SuggestionCommonProperties.INDEX_IN_GROUP, 2);
        mModel.set(SuggestionCommonProperties.TOTAL_IN_GROUP, 4);

        mProcessor.populateModel(mInput, mSuggestion, mModel, 0);

        String expectedAnnouncement =
                "Gemini, AI Mode. Conversation. 3 of 4 in the group AI Suggestions.";
        assertEquals(
                expectedAnnouncement, mModel.get(SuggestionViewProperties.CONTENT_DESCRIPTION));
    }
}
