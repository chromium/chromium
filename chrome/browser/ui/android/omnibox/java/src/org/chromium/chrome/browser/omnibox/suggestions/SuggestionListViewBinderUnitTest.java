// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.app.Activity;
import android.graphics.drawable.ColorDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link SuggestionListViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SuggestionListViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock DropdownItemViewInfo mDropdownItem;

    private PropertyModel mListModel;
    private ViewGroup mContainer;
    private OmniboxSuggestionsDropdown mDropdown;
    private ModelList mSuggestionModels;
    private Activity mActivity = Robolectric.buildActivity(Activity.class).setup().get();

    @Before
    public void setUp() {
        mSuggestionModels = new ModelList();
        mListModel = new PropertyModel(SuggestionListProperties.ALL_KEYS);
        mContainer =
                (ViewGroup)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.omnibox_results_container, /* root= */ null);
        mDropdown = mContainer.findViewById(R.id.omnibox_suggestions_dropdown);
        PropertyModelChangeProcessor.create(
                mListModel,
                new SuggestionListViewBinder.SuggestionListViewHolder(mContainer, mDropdown),
                SuggestionListViewBinder::bind);
        mListModel.set(SuggestionListProperties.SUGGESTION_MODELS, mSuggestionModels);
    }

    @Test
    public void suggestionsContainerVisible_zeroListItems() {
        assertEquals(0, mSuggestionModels.size());
        assertEquals(View.GONE, mContainer.getVisibility());
        assertEquals(View.GONE, mDropdown.getVisibility());

        mListModel.set(SuggestionListProperties.OMNIBOX_SESSION_ACTIVE, true);
        mListModel.set(SuggestionListProperties.CONTAINER_ALWAYS_VISIBLE, true);
        assertEquals(View.VISIBLE, mContainer.getVisibility());
        assertEquals(View.GONE, mDropdown.getVisibility());

        mListModel.set(SuggestionListProperties.OMNIBOX_SESSION_ACTIVE, false);
        assertEquals(View.GONE, mContainer.getVisibility());
        assertEquals(View.GONE, mDropdown.getVisibility());
    }

    @Test
    public void suggestionsContainerVisible_nonZeroListItems() {
        List<ListItem> suggestionsList = new ArrayList<>();
        suggestionsList.add(mDropdownItem);
        mSuggestionModels.set(suggestionsList);

        assertEquals(suggestionsList.size(), mSuggestionModels.size());
        assertEquals(View.GONE, mContainer.getVisibility());
        assertEquals(View.GONE, mDropdown.getVisibility());

        mListModel.set(SuggestionListProperties.OMNIBOX_SESSION_ACTIVE, true);
        mListModel.set(SuggestionListProperties.CONTAINER_ALWAYS_VISIBLE, false);
        assertEquals(View.VISIBLE, mContainer.getVisibility());
        assertEquals(View.VISIBLE, mDropdown.getVisibility());

        mListModel.set(SuggestionListProperties.OMNIBOX_SESSION_ACTIVE, false);
        assertEquals(View.GONE, mContainer.getVisibility());
        assertEquals(View.GONE, mDropdown.getVisibility());
    }

    @Test
    public void suggestionsContainerNotVisible_colorScheme() {
        mListModel.set(SuggestionListProperties.COLOR_SCHEME, BrandedColorScheme.APP_DEFAULT);
        mListModel.set(SuggestionListProperties.CONTAINER_ALWAYS_VISIBLE, false);
        assertNull(mContainer.getBackground());
    }

    @Test
    public void suggestionsContainerVisible_incognitoColorScheme() {
        mListModel.set(SuggestionListProperties.COLOR_SCHEME, BrandedColorScheme.INCOGNITO);
        mListModel.set(SuggestionListProperties.CONTAINER_ALWAYS_VISIBLE, true);

        assertThat(mContainer.getBackground(), instanceOf(ColorDrawable.class));
        ColorDrawable background = (ColorDrawable) mContainer.getBackground();
        assertEquals(
                mActivity.getColor(R.color.default_bg_color_dark_elev_3_baseline),
                background.getColor());
    }

    @Test
    public void suggestionsContainerVisible_nonIncognitoColorScheme() {
        mListModel.set(SuggestionListProperties.COLOR_SCHEME, BrandedColorScheme.APP_DEFAULT);
        mListModel.set(SuggestionListProperties.CONTAINER_ALWAYS_VISIBLE, true);

        assertThat(mContainer.getBackground(), instanceOf(ColorDrawable.class));
        ColorDrawable background = (ColorDrawable) mContainer.getBackground();
        assertEquals(
                ChromeColors.getSurfaceColor(
                        mActivity, R.dimen.omnibox_suggestion_dropdown_bg_elevation),
                background.getColor());
    }
}
