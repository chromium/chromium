// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.VISIBLE;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.TextView;

import androidx.constraintlayout.helper.widget.Flow;
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
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.browser_ui.widget.chips.ChipView;
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
    @Mock private Runnable mMockManageClickListener;
    @Mock private Callback<Integer> mMockSuggestionClickListener;

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
                                .build(),
                        new AutofillSuggestion.Builder()
                                .setLabel("Label 2")
                                .setSubLabel("")
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
    public void testNoticeVisibleProperty() {
        AtMemoryHomeView homeView = mView.getHomeView();
        View noticeContainer = homeView.findViewById(R.id.notice_container);
        assertNotNull(noticeContainer);

        PropertyModel model =
                new PropertyModel.Builder(HomeProperties.ALL_KEYS)
                        .with(HomeProperties.IS_NOTICE_VISIBLE, true)
                        .build();
        PropertyModelChangeProcessor.create(
                model, homeView, AtMemoryBottomSheetViewBinder::bindAtMemoryHomeView);

        assertEquals(View.VISIBLE, noticeContainer.getVisibility());

        model.set(HomeProperties.IS_NOTICE_VISIBLE, false);

        assertEquals(View.GONE, noticeContainer.getVisibility());
    }

    @Test
    public void testNoticeOkClickListenerProperty() {
        AtMemoryHomeView homeView = mView.getHomeView();
        View noticeOkButton = homeView.findViewById(R.id.notice_ok_button);
        assertNotNull(noticeOkButton);

        Runnable clicked = mock(Runnable.class);
        PropertyModel model =
                new PropertyModel.Builder(HomeProperties.ALL_KEYS)
                        .with(HomeProperties.NOTICE_OK_CLICK_LISTENER, clicked)
                        .build();
        PropertyModelChangeProcessor.create(
                model, homeView, AtMemoryBottomSheetViewBinder::bindAtMemoryHomeView);

        noticeOkButton.performClick();
        verify(clicked).run();
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
        PropertyModel model =
                new PropertyModel.Builder(FlyoutProperties.ALL_KEYS)
                        .with(FlyoutProperties.ON_MANAGE_CLICKED, mMockManageClickListener)
                        .build();
        PropertyModelChangeProcessor.create(
                model,
                mView.getFlyoutView(),
                AtMemoryBottomSheetViewBinder::bindAtMemoryFlyoutView);

        View manageButton = mView.getContentView().findViewById(R.id.flyout_manage_button);
        manageButton.performClick();

        verify(mMockManageClickListener).run();
    }

    @Test
    public void testFlyoutSuggestionClickNotifiesCallback() {
        AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder().setLabel("Label 1").setSubLabel("").build();

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
