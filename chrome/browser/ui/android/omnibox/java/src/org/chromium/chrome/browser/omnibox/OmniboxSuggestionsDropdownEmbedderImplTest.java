// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Configuration;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownEmbedder.OmniboxAlignment;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.widget.InsetObserverView;
import org.chromium.components.browser_ui.widget.InsetObserverViewSupplier;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowDelegate;
import org.chromium.ui.display.DisplayAndroid;

import java.lang.ref.WeakReference;

/**
 * Unit tests for {@link OmniboxSuggestionsDropdownEmbedderImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxSuggestionsDropdownEmbedderImplTest {
    private static final int ANCHOR_WIDTH = 600;
    private static final int ANCHOR_HEIGHT = 80;
    private static final int ANCHOR_TOP = 31;
    private static final int ALIGNMENT_WIDTH = 400;
    // Sentinel value for mistaken use of alignment view top instead of left. If you see a 43, it's
    // probably because you used position[1] instead of position[0].
    private static final int ALIGNMENT_TOP = 43;

    public @Rule TestRule mProcessor = new Features.JUnitProcessor();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock WindowAndroid mWindowAndroid;
    private @Mock WindowDelegate mWindowDelegate;
    private @Mock ViewTreeObserver mViewTreeObserver;
    private @Mock ViewGroup mContentView;
    private @Mock ViewGroup mAnchorView;
    private @Mock View mHorizontalAlignmentView;
    private @Mock DisplayAndroid mDisplay;
    private @Mock InsetObserverView mInsetObserverView;

    private OmniboxSuggestionsDropdownEmbedderImpl mImpl;
    private WeakReference<Context> mContextWeakRef;

    @Before
    public void setUp() {
        mContextWeakRef = new WeakReference<>(ContextUtils.getApplicationContext());
        InsetObserverViewSupplier.setInstanceForTesting(mInsetObserverView);
        doReturn(mContextWeakRef).when(mWindowAndroid).getContext();
        doReturn(mContextWeakRef.get()).when(mAnchorView).getContext();
        doReturn(mViewTreeObserver).when(mAnchorView).getViewTreeObserver();
        doReturn(mContentView).when(mAnchorView).getRootView();
        doReturn(mContentView).when(mContentView).findViewById(android.R.id.content);
        doReturn(mContentView).when(mAnchorView).getParent();
        doReturn(ANCHOR_WIDTH).when(mAnchorView).getMeasuredWidth();
        doReturn(ALIGNMENT_WIDTH).when(mHorizontalAlignmentView).getMeasuredWidth();
        doReturn(ANCHOR_HEIGHT).when(mAnchorView).getMeasuredHeight();
        doReturn(ANCHOR_TOP).when(mAnchorView).getTop();
        doReturn(ALIGNMENT_TOP).when(mHorizontalAlignmentView).getTop();
        doReturn(mDisplay).when(mWindowAndroid).getDisplay();
        // Sentinel value for mistaken use of pixels. OmniboxSuggestionsDropdownEmbedderImpl should
        // operate solely in terms of dp so values that are 10x their correct size are probably
        // being inadvertently converted to px.
        doReturn(10.0f).when(mDisplay).getDipScale();
        mImpl = new OmniboxSuggestionsDropdownEmbedderImpl(
                mWindowAndroid, mWindowDelegate, mAnchorView, mHorizontalAlignmentView);
    }

    @Test
    public void testWindowAttachment() {
        verify(mAnchorView, never()).addOnLayoutChangeListener(mImpl);
        verify(mHorizontalAlignmentView, never()).addOnLayoutChangeListener(mImpl);
        verify(mAnchorView, never()).getViewTreeObserver();

        mImpl.onAttachedToWindow();

        verify(mAnchorView).addOnLayoutChangeListener(mImpl);
        verify(mHorizontalAlignmentView).addOnLayoutChangeListener(mImpl);
        verify(mViewTreeObserver).addOnGlobalLayoutListener(mImpl);

        mImpl.onDetachedFromWindow();
        verify(mAnchorView).removeOnLayoutChangeListener(mImpl);
        verify(mHorizontalAlignmentView).removeOnLayoutChangeListener(mImpl);
        verify(mViewTreeObserver).removeOnGlobalLayoutListener(mImpl);
    }

    @Test
    public void testRecalculateOmniboxAlignment_phone() {
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(40).when(mHorizontalAlignmentView).getLeft();
        doReturn(60).when(mHorizontalAlignmentView).getTop();
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        assertEquals(new OmniboxAlignment(0, ANCHOR_HEIGHT + ANCHOR_TOP, ANCHOR_WIDTH, 0, 0, 0),
                alignment);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE)
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE + "<Study",
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_modernize_visual_update_on_tablet/true"})
    public void
    testRecalculateOmniboxAlignment_phoneRevampEnabled() {
        OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.setForTesting(true);
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(40).when(mHorizontalAlignmentView).getLeft();
        doReturn(60).when(mHorizontalAlignmentView).getTop();
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        assertEquals(new OmniboxAlignment(0, ANCHOR_HEIGHT + ANCHOR_TOP, ANCHOR_WIDTH, 0, 0, 0),
                alignment);
    }

    @Test
    @Config(qualifiers = "ldltr-sw600dp")
    public void testRecalculateOmniboxAlignment_tablet_ltr() {
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(40).when(mHorizontalAlignmentView).getLeft();
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        assertEquals(new OmniboxAlignment(0, ANCHOR_HEIGHT + ANCHOR_TOP, ANCHOR_WIDTH, 0, 40,
                             ANCHOR_WIDTH - ALIGNMENT_WIDTH - 40),
                alignment);
    }

    @Test
    @Config(qualifiers = "ldrtl-sw600dp")
    public void testRecalculateOmniboxAlignment_tablet_rtl() {
        doReturn(View.LAYOUT_DIRECTION_RTL).when(mAnchorView).getLayoutDirection();
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(40).when(mHorizontalAlignmentView).getLeft();
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        assertEquals(new OmniboxAlignment(0, ANCHOR_HEIGHT + ANCHOR_TOP, ANCHOR_WIDTH, 0, 40,
                             ANCHOR_WIDTH - ALIGNMENT_WIDTH - 40),
                alignment);
    }

    @Test
    @Config(qualifiers = "ldltr-sw600dp")
    @EnableFeatures({ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE,
            ChromeFeatureList.OMNIBOX_ADAPT_NARROW_TABLET_WINDOWS})
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE + "<Study",
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_modernize_visual_update_on_tablet/true"})
    public void
    testRecalculateOmniboxAlignment_tabletToPhoneSwitch() {
        OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.setForTesting(true);
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(40).when(mHorizontalAlignmentView).getLeft();
        assertTrue(mImpl.isTablet());
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        assertEquals(new OmniboxAlignment(40, ANCHOR_HEIGHT + ANCHOR_TOP, ALIGNMENT_WIDTH, 0, 0, 0),
                alignment);

        Configuration newConfig = new Configuration();
        newConfig.screenWidthDp = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP - 1;
        mImpl.onConfigurationChanged(newConfig);
        assertFalse(mImpl.isTablet());
        OmniboxAlignment newAlignment = mImpl.getCurrentAlignment();
        assertEquals(new OmniboxAlignment(0, ANCHOR_HEIGHT + ANCHOR_TOP, ANCHOR_WIDTH, 0, 0, 0),
                newAlignment);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE,
            ChromeFeatureList.OMNIBOX_ADAPT_NARROW_TABLET_WINDOWS})
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE + "<Study",
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_modernize_visual_update_on_tablet/true"})
    @Config(qualifiers = "ldltr-sw600dp")
    public void
    testRecalculateOmniboxAlignment_phoneToTabletSwitch() {
        OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.setForTesting(true);
        Configuration newConfig = new Configuration();
        newConfig.screenWidthDp = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP - 1;
        mImpl.onConfigurationChanged(newConfig);
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(40).when(mHorizontalAlignmentView).getLeft();
        assertFalse(mImpl.isTablet());
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        assertEquals(new OmniboxAlignment(0, ANCHOR_HEIGHT + ANCHOR_TOP, ANCHOR_WIDTH, 0, 0, 0),
                alignment);

        newConfig.screenWidthDp = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP + 1;
        mImpl.onConfigurationChanged(newConfig);
        assertTrue(mImpl.isTablet());
        OmniboxAlignment newAlignment = mImpl.getCurrentAlignment();
        assertEquals(new OmniboxAlignment(40, ANCHOR_HEIGHT + ANCHOR_TOP, ALIGNMENT_WIDTH, 0, 0, 0),
                newAlignment);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_ADAPT_NARROW_TABLET_WINDOWS)
    @Config(qualifiers = "sw400dp")
    public void testAdaptToNarrowWindows_widePhoneScreen() {
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(40).when(mHorizontalAlignmentView).getLeft();
        assertFalse(mImpl.isTablet());

        Configuration newConfig = new Configuration();
        newConfig.screenWidthDp = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP + 1;
        mImpl.onConfigurationChanged(newConfig);
        assertFalse(mImpl.isTablet());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_ADAPT_NARROW_TABLET_WINDOWS)
    @Config(qualifiers = "sw600dp")
    public void testRecalculateOmniboxAlignment_tabletToPhoneSwitch_revampDisabled() {
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(40).when(mHorizontalAlignmentView).getLeft();
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        assertEquals(new OmniboxAlignment(0, ANCHOR_HEIGHT + ANCHOR_TOP, ANCHOR_WIDTH, 0, 40,
                             ANCHOR_WIDTH - ALIGNMENT_WIDTH - 40),
                alignment);

        Configuration newConfig = new Configuration();
        newConfig.screenWidthDp = DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP - 1;
        mImpl.onConfigurationChanged(newConfig);
        assertFalse(mImpl.isTablet());
        OmniboxAlignment newAlignment = mImpl.getCurrentAlignment();
        assertEquals(new OmniboxAlignment(0, ANCHOR_HEIGHT + ANCHOR_TOP, ANCHOR_WIDTH, 0, 0, 0),
                newAlignment);
    }

    @Test
    @Config(qualifiers = "ldltr-sw600dp")
    @EnableFeatures(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE)
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE + "<Study",
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_modernize_visual_update_on_tablet/true"})
    public void
    testRecalculateOmniboxAlignment_tabletRevampEnabled_ltr() {
        OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.setForTesting(true);
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(40).when(mHorizontalAlignmentView).getLeft();
        doReturn(60).when(mHorizontalAlignmentView).getTop();
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        assertEquals(new OmniboxAlignment(40, ANCHOR_HEIGHT + ANCHOR_TOP, ALIGNMENT_WIDTH, 0, 0, 0),
                alignment);
    }

    @Test
    @Config(qualifiers = "ldrtl-sw600dp")
    @EnableFeatures(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE)
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE + "<Study",
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_modernize_visual_update_on_tablet/true"})
    public void
    testRecalculateOmniboxAlignment_tabletRevampEnabled_rtl() {
        OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.setForTesting(true);
        doReturn(View.LAYOUT_DIRECTION_RTL).when(mAnchorView).getLayoutDirection();
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(40).when(mHorizontalAlignmentView).getLeft();
        doReturn(60).when(mHorizontalAlignmentView).getTop();
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        assertEquals(new OmniboxAlignment(-(ANCHOR_WIDTH - ALIGNMENT_WIDTH - 40),
                             ANCHOR_HEIGHT + ANCHOR_TOP, ALIGNMENT_WIDTH, 0, 0, 0),
                alignment);
    }
}
