// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link AtMemoryBottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class AtMemoryBottomSheetMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AtMemoryBottomSheetCoordinator.Delegate mDelegate;

    private PropertyModel mModel;
    private ModelList mModelList;
    private AtMemoryBottomSheetMediator mMediator;

    @Before
    public void setUp() {
        mModel =
                new PropertyModel.Builder(AtMemoryBottomSheetProperties.ALL_KEYS)
                        .with(AtMemoryBottomSheetProperties.VISIBLE, false)
                        .build();
        mModelList = new ModelList();
        mMediator = new AtMemoryBottomSheetMediator(mDelegate, mModel, mModelList);
    }

    @Test
    public void testOnSuggestionClicked() {
        List<AutofillSuggestion> suggestions =
                List.of(
                        new AutofillSuggestion.Builder()
                                .setIconId(R.drawable.flight)
                                .setLabel("KLM204")
                                .setSubLabel("Flight ⋅ 15 May ⋅ SEA - MUC")
                                .build(),
                        new AutofillSuggestion.Builder()
                                .setIconId(R.drawable.travel_trip)
                                .setLabel("Hotel Booking")
                                .setSubLabel("Hilton ⋅ 16 May")
                                .build());

        mMediator.setSuggestions(suggestions);

        assertEquals(2, mModelList.size());

        assertEquals(
                suggestions.get(0).getLabel(),
                mModelList.get(0).model.get(AtMemoryBottomSheetSuggestionProperties.TITLE));
        assertEquals(
                suggestions.get(1).getLabel(),
                mModelList.get(1).model.get(AtMemoryBottomSheetSuggestionProperties.TITLE));

        PropertyModel itemModel1 = mModelList.get(0).model;
        itemModel1.get(AtMemoryBottomSheetSuggestionProperties.ON_SUGGESTION_CLICKED).run();

        verify(mDelegate).onSuggestionClicked(suggestions.get(0));

        PropertyModel itemModel2 = mModelList.get(1).model;
        itemModel2.get(AtMemoryBottomSheetSuggestionProperties.ON_FLYOUT_CLICKED).run();

        verify(mDelegate).onFlyoutClicked(suggestions.get(1));
    }

    @Test
    public void testOnDismissed() {
        mModel.set(AtMemoryBottomSheetProperties.VISIBLE, true);
        mMediator.onDismissed();
        assertFalse(mModel.get(AtMemoryBottomSheetProperties.VISIBLE));
        verify(mDelegate).onDismissed();
    }
}
