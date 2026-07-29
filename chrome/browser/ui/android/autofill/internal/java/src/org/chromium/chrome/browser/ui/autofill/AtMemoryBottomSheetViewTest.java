// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.VISIBLE;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.TextView;

import androidx.constraintlayout.helper.widget.Flow;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.FlyoutProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.HomeProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.ScreenId;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SuggestionItemProperties;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link AtMemoryBottomSheetView}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class AtMemoryBottomSheetViewTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mMockBackClickListener;
    @Mock private Callback<Integer> mMockSuggestionClickListener;
    @Mock private BottomSheetController mBottomSheetController;

    private Context mContext;
    private AtMemoryBottomSheetView mView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        mView = new AtMemoryBottomSheetView(mContext);
    }

    @Test
    public void testSearchAreaGainsFocusWhenVisible() {
        PropertyModel model =
                new PropertyModel.Builder(AtMemoryBottomSheetProperties.ALL_KEYS)
                        .with(VISIBLE, true)
                        .build();
        PropertyModelChangeProcessor.create(
                model, mView, AtMemoryBottomSheetViewBinder::bindAtMemoryBottomSheetView);

        View contentView = mView.getContentView();
        View searchView = contentView.findViewById(R.id.search_query_input);
        assertNotNull(searchView);
        assertTrue(searchView.hasFocus());
        assertTrue(mView.searchHasFocus());
    }

    @Test
    public void testSearchTextIsClearedWhenVisible() {
        View contentView = mView.getContentView();
        EditText searchView = contentView.findViewById(R.id.search_query_input);
        assertNotNull(searchView);
        searchView.setText("some text");

        PropertyModel model =
                new PropertyModel.Builder(AtMemoryBottomSheetProperties.ALL_KEYS)
                        .with(VISIBLE, true)
                        .build();
        PropertyModelChangeProcessor.create(
                model, mView, AtMemoryBottomSheetViewBinder::bindAtMemoryBottomSheetView);

        assertEquals("", searchView.getText().toString());
    }

    @Test
    public void testSetFlyoutSuggestionsPopulatesChips() {
        List<AutofillSuggestion> suggestions =
                List.of(
                        new AutofillSuggestion.Builder()
                                .setLabel("Label 1")
                                .setSubLabel("Sublabel 1")
                                .setSuggestionType(SuggestionType.AT_MEMORY_SEARCH_RESULT)
                                .build(),
                        new AutofillSuggestion.Builder()
                                .setLabel("Label 2")
                                .setSubLabel("")
                                .setSuggestionType(SuggestionType.AT_MEMORY_SEARCH_RESULT)
                                .build());

        PropertyModel model =
                new PropertyModel.Builder(FlyoutProperties.ALL_KEYS)
                        .with(FlyoutProperties.TITLE, "Flyout Title")
                        .with(FlyoutProperties.SUGGESTIONS, suggestions)
                        .build();
        PropertyModelChangeProcessor.create(
                model,
                mView.getFlyoutView(),
                AtMemoryBottomSheetViewBinder::bindAtMemoryFlyoutView);

        TextView titleText = mView.getContentView().findViewById(R.id.flyout_title);
        assertEquals("Flyout Title", titleText.getText().toString());

        TextView sourceText = mView.getContentView().findViewById(R.id.flyout_source_text);
        assertEquals(
                mContext.getString(R.string.autofill_at_memory_suggestion_source_text),
                sourceText.getText().toString());

        ViewGroup chipsContainer = mView.getContentView().findViewById(R.id.flyout_chips_container);
        assertNotNull(chipsContainer);

        Flow flow = mView.getContentView().findViewById(R.id.chips_flow);
        assertNotNull(flow);
        int[] ids = flow.getReferencedIds();
        assertEquals(2, ids.length);

        ChipView chip1 = mView.getContentView().findViewById(ids[0]);
        ChipView chip2 = mView.getContentView().findViewById(ids[1]);
        assertNotNull(chip1);
        assertNotNull(chip2);

        assertEquals("Label 1", chip1.getPrimaryTextView().getText().toString());
        assertEquals("Sublabel 1", chip1.getSecondaryTextView().getText().toString());
        assertEquals(View.VISIBLE, chip1.getSecondaryTextView().getVisibility());

        assertEquals("Label 2", chip2.getPrimaryTextView().getText().toString());
        assertEquals(View.GONE, chip2.getSecondaryTextView().getVisibility());

        // Adding views to a Flow posts asynchronous layout tasks to the main thread.
        // We must idle the main looper to ensure these tasks complete before verifying the view
        // hierarchy, avoiding flaky test failures.
        ShadowLooper.idleMainLooper();
    }

    @Test
    public void testNoticeItemViewBinding() {
        View noticeView =
                android.view.LayoutInflater.from(mContext)
                        .inflate(R.layout.at_memory_bottom_sheet_notice_item, null);

        Runnable okClicked = mock(Runnable.class);
        Runnable settingsClicked = mock(Runnable.class);
        PropertyModel model =
                new PropertyModel.Builder(
                                AtMemoryBottomSheetProperties.NoticeItemProperties.ALL_KEYS)
                        .with(
                                AtMemoryBottomSheetProperties.NoticeItemProperties.ON_OK_CLICKED,
                                okClicked)
                        .with(
                                AtMemoryBottomSheetProperties.NoticeItemProperties
                                        .ON_SETTINGS_CLICKED,
                                settingsClicked)
                        .build();

        PropertyModelChangeProcessor.create(
                model,
                (AtMemoryBottomSheetNoticeView) noticeView,
                AtMemoryBottomSheetViewBinder::bindNoticeItemView);

        View noticeOkButton = noticeView.findViewById(R.id.notice_ok_button);
        assertNotNull(noticeOkButton);
        noticeOkButton.performClick();
        verify(okClicked).run();
    }

    @Test
    public void testTextWithClickableLinkViewBinding() {
        View textWithClickableLinkView =
                android.view.LayoutInflater.from(mContext)
                        .inflate(
                                R.layout.at_memory_bottom_sheet_text_with_clickable_link_item,
                                null);

        Runnable linkClicked = mock(Runnable.class);
        PropertyModel model =
                new PropertyModel.Builder(
                                AtMemoryBottomSheetProperties.TextWithClickableLinkProperties
                                        .ALL_KEYS)
                        .with(
                                AtMemoryBottomSheetProperties.TextWithClickableLinkProperties.TEXT,
                                "Test string with <link>link text</link>")
                        .with(
                                AtMemoryBottomSheetProperties.TextWithClickableLinkProperties
                                        .ON_LINK_CLICKED,
                                linkClicked)
                        .build();

        PropertyModelChangeProcessor.create(
                model,
                (AtMemoryBottomSheetTextWithClickableLinkView) textWithClickableLinkView,
                AtMemoryBottomSheetViewBinder::bindTextWithClickableLinkView);

        TextView textView = textWithClickableLinkView.findViewById(R.id.text);
        assertNotNull(textView);
        assertEquals("Test string with link text", textView.getText().toString());
    }

    @Test
    public void testNoticeItemViewBinding_isLoggingDisabled() {
        View noticeView =
                android.view.LayoutInflater.from(mContext)
                        .inflate(R.layout.at_memory_bottom_sheet_notice_item, null);

        Runnable settingsClicked = mock(Runnable.class);
        PropertyModel model =
                new PropertyModel.Builder(
                                AtMemoryBottomSheetProperties.NoticeItemProperties.ALL_KEYS)
                        .with(
                                AtMemoryBottomSheetProperties.NoticeItemProperties
                                        .IS_LOGGING_ALLOWED,
                                false)
                        .with(
                                AtMemoryBottomSheetProperties.NoticeItemProperties
                                        .ON_SETTINGS_CLICKED,
                                settingsClicked)
                        .build();

        PropertyModelChangeProcessor.create(
                model,
                (AtMemoryBottomSheetNoticeView) noticeView,
                AtMemoryBottomSheetViewBinder::bindNoticeItemView);

        TextView noticeTextView = noticeView.findViewById(R.id.notice_text);
        assertNotNull(noticeTextView);
        String expectedTextWithoutSpan =
                mContext.getString(R.string.at_memory_notice_text_no_logging)
                        .replace("<link>", "")
                        .replace("</link>", "");
        assertEquals(expectedTextWithoutSpan, noticeTextView.getText().toString());
    }

    @Test
    public void testNoticeItemViewBinding_isLoggingEnabled() {
        View noticeView =
                android.view.LayoutInflater.from(mContext)
                        .inflate(R.layout.at_memory_bottom_sheet_notice_item, null);

        Runnable settingsClicked = mock(Runnable.class);
        PropertyModel model =
                new PropertyModel.Builder(
                                AtMemoryBottomSheetProperties.NoticeItemProperties.ALL_KEYS)
                        .with(
                                AtMemoryBottomSheetProperties.NoticeItemProperties
                                        .IS_LOGGING_ALLOWED,
                                true)
                        .with(
                                AtMemoryBottomSheetProperties.NoticeItemProperties
                                        .ON_SETTINGS_CLICKED,
                                settingsClicked)
                        .build();

        PropertyModelChangeProcessor.create(
                model,
                (AtMemoryBottomSheetNoticeView) noticeView,
                AtMemoryBottomSheetViewBinder::bindNoticeItemView);

        TextView noticeTextView = noticeView.findViewById(R.id.notice_text);
        assertNotNull(noticeTextView);
        String expectedTextWithoutSpan =
                mContext.getString(R.string.at_memory_notice_text)
                        .replace("<link>", "")
                        .replace("</link>", "");
        assertEquals(expectedTextWithoutSpan, noticeTextView.getText().toString());
    }

    @Test
    public void testFlyoutBackClickNotifiesCallback() {
        PropertyModel model =
                new PropertyModel.Builder(FlyoutProperties.ALL_KEYS)
                        .with(FlyoutProperties.ON_BACK_CLICKED, mMockBackClickListener)
                        .build();
        PropertyModelChangeProcessor.create(
                model,
                mView.getFlyoutView(),
                AtMemoryBottomSheetViewBinder::bindAtMemoryFlyoutView);

        View backButton = mView.getContentView().findViewById(R.id.flyout_back_button);
        backButton.performClick();

        verify(mMockBackClickListener).run();
    }

    @Test
    public void testFlyoutManageClickNotifiesCallback() {
        AutofillSuggestion manageSuggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("Manage information")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.MANAGE_AUTOFILL_AI)
                        .setIconId(R.drawable.ic_chrome)
                        .build();
        PropertyModel model =
                new PropertyModel.Builder(FlyoutProperties.ALL_KEYS)
                        .with(FlyoutProperties.SUGGESTIONS, List.of(manageSuggestion))
                        .with(FlyoutProperties.ON_SUGGESTION_CLICKED, mMockSuggestionClickListener)
                        .build();
        PropertyModelChangeProcessor.create(
                model,
                mView.getFlyoutView(),
                AtMemoryBottomSheetViewBinder::bindAtMemoryFlyoutView);

        View manageButton = mView.getContentView().findViewById(R.id.flyout_manage_button);
        assertEquals(View.VISIBLE, manageButton.getVisibility());
        manageButton.performClick();

        verify(mMockSuggestionClickListener).onResult(0);
    }

    @Test
    public void testFlyoutSuggestionClickNotifiesCallback() {
        AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("Label 1")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.AT_MEMORY_SEARCH_RESULT)
                        .build();

        PropertyModel model =
                new PropertyModel.Builder(FlyoutProperties.ALL_KEYS)
                        .with(FlyoutProperties.SUGGESTIONS, List.of(suggestion))
                        .with(FlyoutProperties.ON_SUGGESTION_CLICKED, mMockSuggestionClickListener)
                        .build();
        PropertyModelChangeProcessor.create(
                model,
                mView.getFlyoutView(),
                AtMemoryBottomSheetViewBinder::bindAtMemoryFlyoutView);

        ViewGroup chipsContainer = mView.getContentView().findViewById(R.id.flyout_chips_container);
        ChipView chip = getChipViews(chipsContainer).get(0);
        chip.performClick();

        verify(mMockSuggestionClickListener).onResult(0);
    }

    @Test
    public void testHeightRatiosWhenSearchHasFocus() {
        when(mBottomSheetController.getContainerHeight()).thenReturn(1000);

        AtMemoryBottomSheetContent content =
                new AtMemoryBottomSheetContent(mView, mBottomSheetController);

        assertTrue(content.getHalfHeightRatio() > 0.0f && content.getHalfHeightRatio() <= 0.5f);
        assertEquals(1.0f, content.getFullHeightRatio(), 0.01f);

        EditText searchView = mView.getContentView().findViewById(R.id.search_query_input);
        searchView.requestFocus();

        assertTrue(mView.searchHasFocus());
        assertEquals(1.0f, content.getFullHeightRatio(), 0.01f);
        assertEquals(HeightMode.DISABLED, content.getHalfHeightRatio(), 0.01f);
    }

    @Test
    public void testHeightRatiosOnFlyoutScreen() {
        when(mBottomSheetController.getContainerHeight()).thenReturn(1000);

        AtMemoryBottomSheetContent content =
                new AtMemoryBottomSheetContent(mView, mBottomSheetController);

        mView.setCurrentScreen(ScreenId.FLYOUT_SCREEN);

        assertTrue(content.getHalfHeightRatio() > 0.0f && content.getHalfHeightRatio() <= 0.5f);
        assertEquals(1.0f, content.getFullHeightRatio(), 0.01f);
    }

    @Test
    public void testSuggestionWithDeactivatedStyle() {
        ModelList modelList = new ModelList();
        PropertyModel suggestionModel =
                new PropertyModel.Builder(SuggestionItemProperties.ALL_KEYS)
                        .with(SuggestionItemProperties.TITLE, "Couldn't find this info")
                        .with(SuggestionItemProperties.APPLY_DEACTIVATED_STYLE, true)
                        .build();

        modelList.add(new ListItem(HomeProperties.ItemType.SUGGESTION, suggestionModel));
        AtMemoryHomeView homeView = mView.getHomeView();
        homeView.setUpSheetItems(modelList);

        ShadowLooper.idleMainLooper();

        RecyclerView recyclerView = homeView.findViewById(R.id.suggestions_view);
        recyclerView.layout(0, 0, 100, 1000);
        AtMemoryBottomSheetSuggestionView suggestionView =
                (AtMemoryBottomSheetSuggestionView) recyclerView.getChildAt(0);

        assertFalse(suggestionView.isEnabled());
    }

    private List<ChipView> getChipViews(ViewGroup viewGroup) {
        List<ChipView> chips = new ArrayList<>();
        for (int i = 0; i < viewGroup.getChildCount(); i++) {
            View child = viewGroup.getChildAt(i);
            if (child instanceof ChipView) {
                chips.add((ChipView) child);
            }
        }
        return chips;
    }
}
