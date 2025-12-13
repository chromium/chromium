// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.MotionEvent;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.LayoutParams;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownEmbedder.OmniboxAlignment;
import org.chromium.chrome.browser.omnibox.test.R;

/** Unit tests for {@link OmniboxSuggestionsContainer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 29)
public class OmniboxSuggestionsContainerUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock OmniboxSuggestionsDropdown mDropdown;
    private @Mock RecyclerView.RecycledViewPool mRecycledViewPool;

    private Context mContext;
    private TestOmniboxSuggestionsContainer mContainer;
    private OmniboxAlignment mOmniboxAlignment;
    private final ObservableSupplierImpl<OmniboxAlignment> mOmniboxAlignmentSupplier =
            new ObservableSupplierImpl<>();
    private boolean mIsTablet;
    private boolean mAttachedToWindow;
    private boolean mShouldPassThroughUnhandledTouchEvents;
    private final OmniboxSuggestionsDropdownEmbedder mEmbedder =
            new OmniboxSuggestionsDropdownEmbedder() {
                @Override
                public boolean isTablet() {
                    return mIsTablet;
                }

                @Override
                public void onAttachedToWindow() {
                    mAttachedToWindow = true;
                }

                @Override
                public void onDetachedFromWindow() {
                    mAttachedToWindow = false;
                }

                @Override
                public OmniboxAlignment addAlignmentObserver(Callback<OmniboxAlignment> obs) {
                    return mOmniboxAlignmentSupplier.addObserver(obs);
                }

                @Override
                public void removeAlignmentObserver(Callback<OmniboxAlignment> obs) {
                    mOmniboxAlignmentSupplier.removeObserver(obs);
                }

                @Nullable
                @Override
                public OmniboxAlignment getCurrentAlignment() {
                    return mOmniboxAlignmentSupplier.get();
                }

                @Override
                public float getVerticalTranslationForAnimation() {
                    return 0.0f;
                }

                @Override
                public boolean shouldPassThroughUnhandledTouchEvents() {
                    return mShouldPassThroughUnhandledTouchEvents;
                }
            };

    // TODO(341377411): resolve issues with mockito not being able to stub the isInLayout method.
    private static class TestOmniboxSuggestionsContainer extends OmniboxSuggestionsContainer {
        private boolean mIsInLayout;

        public TestOmniboxSuggestionsContainer(Context context) {
            super(context, null);
        }

        @Override
        public boolean isInLayout() {
            return mIsInLayout;
        }

        public void setIsInLayout(boolean isInLayout) {
            mIsInLayout = isInLayout;
        }
    }

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mContainer = new TestOmniboxSuggestionsContainer(mContext);
        mContainer.setSuggestionsDropdownForTest(mDropdown);

        // Replace the view created via inflation with a mock.
        when(mDropdown.getId()).thenReturn(R.id.omnibox_suggestions_dropdown);
        when(mDropdown.getRecycledViewPool()).thenReturn(mRecycledViewPool);
    }

    @Test
    public void onOmniboxSessionStateChange_withEmbedder() {
        mContainer.setEmbedder(mEmbedder);

        assertFalse(mAttachedToWindow);
        mContainer.onOmniboxSessionStateChange(true);
        assertTrue(mAttachedToWindow);

        mContainer.onOmniboxSessionStateChange(false);
        assertFalse(mAttachedToWindow);
    }

    @Test
    public void onOmniboxSessionStateChange_withoutEmbedder() {
        assertFalse(mAttachedToWindow);
        mContainer.onOmniboxSessionStateChange(true);
        assertFalse(mAttachedToWindow);
        mContainer.onOmniboxSessionStateChange(false);
        assertFalse(mAttachedToWindow);
    }

    @Test
    public void testAlignmentProvider_widthChange() {
        mContainer.setEmbedder(mEmbedder);
        mContainer.onOmniboxSessionStateChange(true);

        mOmniboxAlignment = new OmniboxAlignment(0, 100, 600, 0, 10, 10, 0);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);
        layoutDropdown(600, 800);
        assertEquals(600, mContainer.getMeasuredWidth());

        mOmniboxAlignment = new OmniboxAlignment(0, 100, 400, 0, 10, 10, 0);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);
        ShadowLooper.runUiThreadTasks();
        assertTrue(mContainer.isLayoutRequested());

        layoutDropdown(600, 800);
        assertEquals(400, mContainer.getMeasuredWidth());
        assertFalse(mContainer.isLayoutRequested());
    }

    @Test
    public void testAlignmentProvider_topChange() {
        mContainer.setEmbedder(mEmbedder);
        mContainer.onOmniboxSessionStateChange(true);

        mContainer.setLayoutParams(
                new LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        int marginTop = 100;
        int height = 800 - marginTop;
        mOmniboxAlignment = new OmniboxAlignment(0, 100, 600, height, 10, 10, 0);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);
        layoutDropdown(600, height);

        MarginLayoutParams layoutParams = (MarginLayoutParams) mContainer.getLayoutParams();
        assertNotNull(layoutParams);
        assertEquals(marginTop, layoutParams.topMargin);

        mOmniboxAlignment = new OmniboxAlignment(0, 54, 600, 0, 10, 10, 0);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);
        layoutDropdown(600, height);

        layoutParams = (MarginLayoutParams) mContainer.getLayoutParams();
        assertNotNull(layoutParams);
        assertEquals(54, layoutParams.topMargin);
    }

    @Test
    public void testAlignmentProvider_heightChange() {
        mContainer.setEmbedder(mEmbedder);
        mContainer.onOmniboxSessionStateChange(true);

        mContainer.setLayoutParams(
                new LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        int height = 400;
        mOmniboxAlignment = new OmniboxAlignment(0, 80, 600, height, 10, 10, 0);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);
        layoutDropdown(600, 800);

        assertEquals(height, mContainer.getMeasuredHeight());

        height = 300;
        mOmniboxAlignment = new OmniboxAlignment(0, 80, 600, height, 10, 10, 0);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);
        layoutDropdown(600, 800);

        assertEquals(height, mContainer.getMeasuredHeight());
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testAlignmentProvider_bottomPaddingChange() {
        mContainer.setEmbedder(mEmbedder);
        mContainer.onOmniboxSessionStateChange(true);
        mContainer.setLayoutParams(
                new LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        when(mDropdown.getPaddingTop()).thenReturn(1);
        when(mDropdown.getPaddingLeft()).thenReturn(2);
        when(mDropdown.getPaddingRight()).thenReturn(3);
        when(mDropdown.getPaddingBottom()).thenReturn(4);
        when(mDropdown.getBaseBottomPadding()).thenReturn(4);

        int bottomPadding = 40;
        mOmniboxAlignment = new OmniboxAlignment(0, 80, 600, 400, 10, 10, bottomPadding);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);

        assertEquals(4, mDropdown.getPaddingBottom());

        bottomPadding = 20;
        mOmniboxAlignment = new OmniboxAlignment(0, 80, 600, 400, 10, 10, bottomPadding);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);

        assertEquals(4, mDropdown.getPaddingBottom());
    }

    @Test
    @LooperMode(Mode.PAUSED)
    public void testAlignmentProvider_changeDuringlayout() {
        mContainer.setEmbedder(mEmbedder);
        mContainer.onOmniboxSessionStateChange(true);

        mContainer.setIsInLayout(true);
        mOmniboxAlignment = new OmniboxAlignment(0, 80, 400, 600, 10, 10, 0);
        mOmniboxAlignmentSupplier.set(mOmniboxAlignment);

        mContainer.layout(0, 0, 600, 800);
        assertFalse(mContainer.isLayoutRequested());

        // The posted task should re-request layout.
        ShadowLooper.runUiThreadTasks();
        assertTrue(mContainer.isLayoutRequested());
    }

    private void layoutDropdown(int width, int height) {
        int widthSpec = MeasureSpec.makeMeasureSpec(width, MeasureSpec.AT_MOST);
        int heightSpec = MeasureSpec.makeMeasureSpec(height, MeasureSpec.AT_MOST);
        mContainer.measure(widthSpec, heightSpec);
        mContainer.layout(0, 0, mContainer.getMeasuredWidth(), mContainer.getMeasuredHeight());
    }

    @Test
    public void testOnTouchEvent_returnsTrue() {
        var event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        assertTrue(mContainer.onTouchEvent(event));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testOnTouchEvent_whenIncognitoNtpAndAutofocusEnabled_returnsFalse() {
        checkContainerConsumesTouchEvents(false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testOnTouchEvent_whenNotIncognitoNtpAndAutofocusEnabled_returnsTrue() {
        checkContainerConsumesTouchEvents(true);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testOnTouchEvent_whenAutofocusDisabled_returnsTrue() {
        checkContainerConsumesTouchEvents(true);
    }

    public void checkContainerConsumesTouchEvents(boolean consume) {
        mContainer.setEmbedder(mEmbedder);
        mShouldPassThroughUnhandledTouchEvents = !consume;

        var event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        boolean isConsumed = mContainer.onTouchEvent(event);

        if (consume) {
            assertTrue(isConsumed);
        } else {
            assertFalse(isConsumed);
        }
    }

    @Test
    public void testPerformClick_returnsFalse() {
        assertFalse(mContainer.performClick());
    }
}
