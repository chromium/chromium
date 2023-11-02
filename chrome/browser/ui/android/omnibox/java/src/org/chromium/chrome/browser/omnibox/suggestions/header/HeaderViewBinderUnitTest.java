// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.header;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.notNull;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.os.Bundle;
import android.view.ContextThemeWrapper;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Tests for {@link HeaderViewBinder}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class HeaderViewBinderUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    Activity mActivity;
    PropertyModel mModel;
    Context mContext;
    Resources mResources;

    HeaderView mHeaderView;
    @Mock
    TextView mHeaderText;
    @Mock
    ImageView mHeaderIcon;

    @Before
    public void setUp() {
        mContext = new ContextThemeWrapper(
                ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mResources = mContext.getResources();

        MockitoAnnotations.initMocks(this);
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        mHeaderView = mock(HeaderView.class,
                Mockito.withSettings().useConstructor(mActivity).defaultAnswer(
                        Mockito.CALLS_REAL_METHODS));

        when(mHeaderView.getTextView()).thenReturn(mHeaderText);
        when(mHeaderView.getIconView()).thenReturn(mHeaderIcon);

        mModel = new PropertyModel(HeaderViewProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mHeaderView, HeaderViewBinder::bind);
    }

    @Test
    public void actionIcon_iconReflectsExpandedState() {
        // Expand.
        mModel.set(HeaderViewProperties.IS_COLLAPSED, false);
        verify(mHeaderIcon, times(1)).setImageResource(R.drawable.ic_expand_less_black_24dp);

        // Collapse.
        mModel.set(HeaderViewProperties.IS_COLLAPSED, true);
        verify(mHeaderIcon, times(1)).setImageResource(R.drawable.ic_expand_more_black_24dp);
    }

    @Test
    public void headerView_accessibilityStringReflectsExpandedState() {
        // Expand without title.
        mModel.set(HeaderViewProperties.IS_COLLAPSED, false);
        verify(mHeaderView, times(1)).setCollapsedStateForAccessibility(false);

        mModel.set(HeaderViewProperties.IS_COLLAPSED, true);
        verify(mHeaderView, times(1)).setCollapsedStateForAccessibility(true);
    }

    @Test
    public void headerView_listenerInstalledWhenDelegateIsSet() {
        final HeaderViewProperties.Delegate delegate = mock(HeaderViewProperties.Delegate.class);

        // Install.
        mModel.set(HeaderViewProperties.DELEGATE, delegate);
        verify(mHeaderView, times(1)).setOnClickListener(notNull());
        verify(mHeaderView, times(1)).setOnSelectListener(notNull());

        // Call.
        Assert.assertTrue(mHeaderView.performClick());
        verify(delegate, times(1)).onHeaderClicked();

        // Select.
        mHeaderView.setSelected(false);
        verify(delegate, never()).onHeaderSelected();
        mHeaderView.setSelected(true);
        verify(delegate, times(1)).onHeaderSelected();

        // Remove.
        mModel.set(HeaderViewProperties.DELEGATE, null);
        verify(mHeaderView, times(1)).setOnClickListener(null);
        verify(mHeaderView, times(1)).setOnSelectListener(null);

        reset(delegate);

        // Call; check that click calls are no longer propagated.
        // Note: performClick returns value indicating whether onClickListener was invoked.
        Assert.assertFalse(mHeaderView.performClick());
        verify(delegate, never()).onHeaderClicked();

        // Select.
        mHeaderView.setSelected(true);
        verify(delegate, never()).onHeaderSelected();
    }

    @Test
    public void actionIcon_accessibilityAnnouncementsReflectExpandedState() {
        mModel.set(HeaderViewProperties.SHOULD_REMOVE_CHEVRON, false);

        final AccessibilityNodeInfo info = mock(AccessibilityNodeInfo.class);
        Bundle infoExtras = new Bundle();
        when(info.getExtras()).thenReturn(infoExtras);

        // Expand.
        mModel.set(HeaderViewProperties.IS_COLLAPSED, false);
        mHeaderView.onInitializeAccessibilityNodeInfo(info);
        verify(info, times(1)).addAction(AccessibilityAction.ACTION_COLLAPSE);
        verify(info, never()).addAction(AccessibilityAction.ACTION_EXPAND);

        reset(info);
        when(info.getExtras()).thenReturn(infoExtras);

        // Collapse.
        mModel.set(HeaderViewProperties.IS_COLLAPSED, true);
        mHeaderView.onInitializeAccessibilityNodeInfo(info);
        verify(info, never()).addAction(AccessibilityAction.ACTION_COLLAPSE);
        verify(info, times(1)).addAction(AccessibilityAction.ACTION_EXPAND);
    }

    @Test
    public void headerView_removeSuggestionHeaderChevron() {
        // Remove Chevron.
        mModel.set(HeaderViewProperties.SHOULD_REMOVE_CHEVRON, true);
        verify(mHeaderView, times(1)).setShouldRemoveSuggestionHeaderChevron(true);

        // Restore Chevron.
        mModel.set(HeaderViewProperties.SHOULD_REMOVE_CHEVRON, false);
        verify(mHeaderView, times(1)).setShouldRemoveSuggestionHeaderChevron(false);
    }

    @Test
    public void headerView_removeSuggestionHeaderChevronRemovesAccessibilityControl() {
        mModel.set(HeaderViewProperties.SHOULD_REMOVE_CHEVRON, true);

        final AccessibilityNodeInfo info = mock(AccessibilityNodeInfo.class);
        Bundle infoExtras = new Bundle();
        when(info.getExtras()).thenReturn(infoExtras);

        // Expand.
        mModel.set(HeaderViewProperties.IS_COLLAPSED, false);
        mHeaderView.onInitializeAccessibilityNodeInfo(info);
        verify(info, never()).addAction(AccessibilityAction.ACTION_COLLAPSE);
        verify(info, never()).addAction(AccessibilityAction.ACTION_EXPAND);

        // Collapse.
        mModel.set(HeaderViewProperties.IS_COLLAPSED, true);
        mHeaderView.onInitializeAccessibilityNodeInfo(info);
        verify(info, never()).addAction(AccessibilityAction.ACTION_COLLAPSE);
        verify(info, never()).addAction(AccessibilityAction.ACTION_EXPAND);
    }

    @Test
    public void headerView_removeSuggestionHeaderCapitalizationTrue() {
        // Remove Capitalization.
        mModel.set(HeaderViewProperties.SHOULD_REMOVE_CAPITALIZATION, true);
        verify(mHeaderView, times(1)).setShouldRemoveSuggestionHeaderCapitalization(true);
    }

    @Test
    public void headerView_removeSuggestionHeaderCapitalizationFalse() {
        // Restore Capitalization.
        mModel.set(HeaderViewProperties.SHOULD_REMOVE_CAPITALIZATION, false);
        verify(mHeaderView, times(1)).setShouldRemoveSuggestionHeaderCapitalization(false);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.OMNIBOX_HEADER_PADDING_UPDATE,
            ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE})
    public void
    headerView_updateHeaderPaddingFalse() {
        // Update Header Padding.
        mModel.set(HeaderViewProperties.USE_UPDATED_HEADER_PADDING, false);

        int minHeight = mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_header_height);
        int paddingStart =
                mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_header_padding_start);
        int paddingTop = 0;
        int paddingBottom = 0;
        verify(mHeaderView, times(1))
                .setUpdateHeaderPadding(minHeight, paddingStart, paddingTop, paddingBottom);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_HEADER_PADDING_UPDATE)
    @DisableFeatures(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE)
    public void headerView_updateHeaderPaddingTrue() {
        // Update Header Padding.
        mModel.set(HeaderViewProperties.USE_UPDATED_HEADER_PADDING, true);

        int minHeight =
                mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_header_height_modern);
        int paddingStart = mResources.getDimensionPixelSize(
                R.dimen.omnibox_suggestion_header_padding_start_modern);
        int paddingTop =
                mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_header_padding_top);
        int paddingBottom =
                mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_header_padding_bottom);
        verify(mHeaderView, times(1))
                .setUpdateHeaderPadding(minHeight, paddingStart, paddingTop, paddingBottom);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.OMNIBOX_HEADER_PADDING_UPDATE,
            ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE})
    public void
    headerView_updateHeaderPaddingTrueModernizeFeatureEnabled() {
        // Update Header Padding.
        mModel.set(HeaderViewProperties.USE_UPDATED_HEADER_PADDING, true);

        int minHeight = mResources.getDimensionPixelSize(
                R.dimen.omnibox_suggestion_header_height_modern_phase2);
        int paddingStart = mResources.getDimensionPixelSize(
                                   R.dimen.omnibox_suggestion_header_padding_start_modern)
                + mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_side_spacing);
        int paddingTop =
                mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_header_padding_top);
        int paddingBottom = 0;
        verify(mHeaderView, times(1))
                .setUpdateHeaderPadding(minHeight, paddingStart, paddingTop, paddingBottom);
    }
}
