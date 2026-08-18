// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.browser_ui.widget.chips;

import static android.view.View.MeasureSpec.UNSPECIFIED;
import static android.view.View.MeasureSpec.makeMeasureSpec;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.lessThan;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.text.TextUtils;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowView;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.widget.LoadingView;

/** Tests for {@link ChipViewTest}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowView.class})
public final class ChipViewTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private ChipView mChipView;
    private ChipView mTwoLineChipView;

    @Before
    public void setup() {
        // Disabling animations is necessary to avoid running into issues with
        // delayed hiding of loading views.
        LoadingView.setDisableAnimationForTest(true);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.getTheme().applyStyle(R.style.Theme_BrowserUI_DayNight, true);
        mChipView = new ChipView(mActivity, null);
        mActivity.setContentView(mChipView);
        mTwoLineChipView =
                (ChipView)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.two_line_chip_view_test_item, null);
    }

    @Test
    @SmallTest
    public void primaryTextView() {
        mChipView.getPrimaryTextView().setText("Primary text");

        TextView primaryText = mChipView.findViewById(R.id.chip_view_primary_text);
        assertNotNull(primaryText);
        assertEquals(View.VISIBLE, primaryText.getVisibility());
        assertEquals("Primary text", primaryText.getText());
    }

    @Test
    @SmallTest
    public void secondaryTextView() {
        mChipView.getSecondaryTextView().setText("Secondary text");

        TextView secondaryText = mChipView.findViewById(R.id.chip_view_secondary_text);
        assertNotNull(secondaryText);
        assertEquals(View.VISIBLE, secondaryText.getVisibility());
        assertEquals("Secondary text", secondaryText.getText());
    }

    @Test
    @SmallTest
    public void setMaxWidthWithPrimaryText() {
        mChipView.getPrimaryTextView().setText("Primary text");
        assertThat(mChipView.getPrimaryTextView().getEllipsize(), nullValue());
        measureChip(mChipView);

        final int fullTextWidth = mChipView.getPrimaryTextView().getMeasuredWidth();
        mChipView.setMaxWidth((int) (0.8 * mChipView.getMeasuredWidth()));
        measureChip(mChipView);

        // Make sure that the primary text width is reduced.
        assertThat(mChipView.getPrimaryTextView().getMeasuredWidth(), lessThan(fullTextWidth));
        assertThat(mChipView.getPrimaryTextView().getEllipsize(), is(TextUtils.TruncateAt.END));

        mChipView.setMaxWidth(Integer.MAX_VALUE);
        measureChip(mChipView);
        // Make sure that both the allowed text width and the truncation method are reset.
        assertThat(mChipView.getPrimaryTextView().getMeasuredWidth(), is(fullTextWidth));
        assertThat(mChipView.getPrimaryTextView().getEllipsize(), nullValue());
    }

    @Test
    @SmallTest
    public void setMaxWidthWithSecondaryText() {
        mChipView.getPrimaryTextView().setText("Primary text");
        mChipView.getSecondaryTextView().setText("SecondaryText");
        assertThat(mChipView.getPrimaryTextView().getEllipsize(), nullValue());
        assertThat(mChipView.getSecondaryTextView().getEllipsize(), nullValue());
        measureChip(mChipView);

        final int fullPrimaryTextWidth = mChipView.getPrimaryTextView().getMeasuredWidth();
        final int fullSecondaryTextWidth = mChipView.getSecondaryTextView().getMeasuredWidth();
        mChipView.setMaxWidth((int) (0.8 * mChipView.getMeasuredWidth()));
        measureChip(mChipView);

        // Make sure that the primary text width is reduced and the secondary text view is left
        // intact.
        assertThat(
                mChipView.getPrimaryTextView().getMeasuredWidth(), lessThan(fullPrimaryTextWidth));
        assertThat(mChipView.getPrimaryTextView().getEllipsize(), is(TextUtils.TruncateAt.END));
        assertThat(mChipView.getSecondaryTextView().getMeasuredWidth(), is(fullSecondaryTextWidth));
        assertThat(mChipView.getSecondaryTextView().getEllipsize(), nullValue());

        mChipView.setMaxWidth(Integer.MAX_VALUE);
        measureChip(mChipView);
        // Make sure that both the allowed text width and the truncation method are reset.
        assertThat(mChipView.getPrimaryTextView().getMeasuredWidth(), is(fullPrimaryTextWidth));
        assertThat(mChipView.getPrimaryTextView().getEllipsize(), nullValue());
        assertThat(mChipView.getSecondaryTextView().getMeasuredWidth(), is(fullSecondaryTextWidth));
        assertThat(mChipView.getSecondaryTextView().getEllipsize(), nullValue());
    }

    @Test
    @SmallTest
    public void setTwoLineChip() {
        mTwoLineChipView.getPrimaryTextView().setText("Primary text");
        mTwoLineChipView.getSecondaryTextView().setText("Secondary text");

        LinearLayout textWrapper = mTwoLineChipView.findViewById(R.id.chip_view_text_wrapper);
        assertNotNull(textWrapper);
        assertEquals(LinearLayout.VERTICAL, textWrapper.getOrientation());
        assertEquals(2, textWrapper.getChildCount());

        // Default layout parameters used for vertically oriented linear layout are (MATCH_PARENT,
        // WRAP_CONTENT). Chip view isn't measured correctly with these layout parameters. For more
        // information, see crbug.com/450830784.
        assertEquals(
                LayoutParams.WRAP_CONTENT,
                mTwoLineChipView.getPrimaryTextView().getLayoutParams().width);
        assertEquals(
                LayoutParams.WRAP_CONTENT,
                mTwoLineChipView.getSecondaryTextView().getLayoutParams().width);
    }

    @Test
    @SmallTest
    public void setMaxWidthWithTwoLineChip() {
        mTwoLineChipView.getPrimaryTextView().setText("Primary text");
        mTwoLineChipView.getSecondaryTextView().setText("SecondaryText");
        assertThat(mTwoLineChipView.getPrimaryTextView().getEllipsize(), nullValue());
        assertThat(mTwoLineChipView.getSecondaryTextView().getEllipsize(), nullValue());
        measureChip(mTwoLineChipView);

        final int fullPrimaryTextWidth = mTwoLineChipView.getPrimaryTextView().getMeasuredWidth();
        final int fullSecondaryTextWidth =
                mTwoLineChipView.getSecondaryTextView().getMeasuredWidth();
        mTwoLineChipView.setMaxWidth((int) (0.8 * mTwoLineChipView.getMeasuredWidth()));
        measureChip(mTwoLineChipView);

        // Make sure that both the primary and secondary text width is reduced.
        assertThat(
                mTwoLineChipView.getPrimaryTextView().getMeasuredWidth(),
                lessThan(fullPrimaryTextWidth));
        assertThat(
                mTwoLineChipView.getPrimaryTextView().getEllipsize(), is(TextUtils.TruncateAt.END));
        assertThat(
                mTwoLineChipView.getSecondaryTextView().getMeasuredWidth(),
                lessThan(fullSecondaryTextWidth));
        assertThat(
                mTwoLineChipView.getSecondaryTextView().getEllipsize(),
                is(TextUtils.TruncateAt.END));

        mTwoLineChipView.setMaxWidth(Integer.MAX_VALUE);
        measureChip(mTwoLineChipView);
        // Make sure that both the allowed text width and the truncation method are reset.
        assertThat(
                mTwoLineChipView.getPrimaryTextView().getMeasuredWidth(), is(fullPrimaryTextWidth));
        assertThat(mTwoLineChipView.getPrimaryTextView().getEllipsize(), nullValue());
        assertThat(
                mTwoLineChipView.getSecondaryTextView().getMeasuredWidth(),
                is(fullSecondaryTextWidth));
        assertThat(mTwoLineChipView.getSecondaryTextView().getEllipsize(), nullValue());
    }

    @Test
    @SmallTest
    public void loadingView() {
        // The start icon shouldn't be visible by default.
        ImageView startIcon = mChipView.findViewById(R.id.chip_view_start_icon);
        assertEquals(View.GONE, startIcon.getVisibility());

        // The start icon should become visible after it's set.
        mChipView.setIconWithTint(R.drawable.ic_settings_gear_24dp, /* tintWithTextColor= */ false);
        assertEquals(View.VISIBLE, startIcon.getVisibility());

        LoadingView loadingView = mChipView.findViewById(R.id.chip_view_loading_view);
        assertEquals(View.GONE, loadingView.getVisibility());

        LoadingView.Observer firstObserver = mock(LoadingView.Observer.class);
        mChipView.showLoadingView(firstObserver);
        assertEquals(View.VISIBLE, loadingView.getVisibility());
        // The start icon shouldn't be visible when the loading view is displayed.
        assertEquals(View.GONE, startIcon.getVisibility());
        verify(firstObserver).onShowLoadingUiComplete();

        LoadingView.Observer secondObserver = mock(LoadingView.Observer.class);
        mChipView.hideLoadingView(secondObserver, /* skipDelay= */ true);
        assertEquals(View.GONE, loadingView.getVisibility());
        // The start icon should be visible again when the loading view becomes hidden.
        assertEquals(View.VISIBLE, startIcon.getVisibility());
        verify(secondObserver).onHideLoadingUiComplete();
    }

    @Test
    @SmallTest
    public void loadingViewNullObserver() {
        // Calling show/hide with null should not crash.
        mChipView.showLoadingView(null);
        mChipView.hideLoadingView(null);
    }

    @Test
    @SmallTest
    public void loadingViewSkipDelay() {
        mChipView.setIconWithTint(R.drawable.ic_settings_gear_24dp, /* tintWithTextColor= */ false);
        LoadingView loadingView = mChipView.findViewById(R.id.chip_view_loading_view);
        ImageView startIcon = mChipView.findViewById(R.id.chip_view_start_icon);

        LoadingView.Observer firstObserver = mock(LoadingView.Observer.class);
        mChipView.showLoadingView(firstObserver);
        assertEquals(View.VISIBLE, loadingView.getVisibility());

        LoadingView.Observer secondObserver = mock(LoadingView.Observer.class);
        mChipView.hideLoadingView(secondObserver, /* skipDelay= */ true);
        assertEquals(View.GONE, loadingView.getVisibility());
        assertEquals(View.VISIBLE, startIcon.getVisibility());
        verify(secondObserver).onHideLoadingUiComplete();
    }

    @Test
    @SmallTest
    public void cancelButton() {
        assertNull(mChipView.findViewById(R.id.chip_view_end_icon));

        mChipView.addRemoveIcon();
        ImageView cancelButton = mChipView.findViewById(R.id.chip_view_end_icon);
        assertNotNull(cancelButton);
        assertEquals(View.VISIBLE, cancelButton.getVisibility());
        assertEquals(
                R.drawable.btn_close, shadowOf(cancelButton.getDrawable()).getCreatedFromResId());
    }

    @Test
    @SmallTest
    public void cancelButtonClickListener() {
        mChipView.addRemoveIcon();
        mChipView.getPrimaryTextView().setText("Primary text");

        OnClickListener onClickListener = mock(OnClickListener.class);
        mChipView.setRemoveIconClickListener(onClickListener);
        View cancelButtonWrapper = mChipView.findViewById(R.id.chip_cancel_btn);
        cancelButtonWrapper.performClick();
        verify(onClickListener).onClick(eq(cancelButtonWrapper));
    }

    @Test
    @SmallTest
    public void dropdownButton() {
        assertNull(mChipView.findViewById(R.id.chip_view_end_icon));

        mChipView.addDropdownIcon();
        ImageView dropdownButton = mChipView.findViewById(R.id.chip_view_end_icon);
        assertNotNull(dropdownButton);
        assertEquals(View.VISIBLE, dropdownButton.getVisibility());
        assertEquals(
                R.drawable.mtrl_dropdown_arrow,
                shadowOf(dropdownButton.getDrawable()).getCreatedFromResId());
    }

    @Test
    @SmallTest
    public void setStartIconId() {
        ImageView startIcon = mChipView.findViewById(R.id.chip_view_start_icon);
        assertEquals(View.GONE, startIcon.getVisibility());

        mChipView.setIconWithTint(
                R.drawable.test_ic_arrow_downward_black_24dp, /* tintWithTextColor= */ false);
        assertEquals(View.VISIBLE, startIcon.getVisibility());

        mChipView.setIconWithTint(
                R.drawable.test_ic_arrow_downward_black_24dp, /* tintWithTextColor= */ true);
        assertEquals(View.VISIBLE, startIcon.getVisibility());
    }

    @Test
    @SmallTest
    public void setStartIconDrawable() {
        ImageView startIcon = mChipView.findViewById(R.id.chip_view_start_icon);
        assertEquals(View.GONE, startIcon.getVisibility());

        mChipView.setIconWithTint(
                AppCompatResources.getDrawable(
                        mActivity, R.drawable.test_ic_arrow_downward_black_24dp),
                /* tintWithTextColor= */ false);
        assertEquals(View.VISIBLE, startIcon.getVisibility());

        mChipView.setIconWithTint(
                AppCompatResources.getDrawable(
                        mActivity, R.drawable.test_ic_arrow_downward_black_24dp),
                /* tintWithTextColor= */ true);
        assertEquals(View.VISIBLE, startIcon.getVisibility());
    }

    @Test
    @SmallTest
    public void compactMode() {
        mChipView.getPrimaryTextView().setText("Primary text");
        int defaultStartPadding =
                mActivity.getResources().getDimensionPixelSize(R.dimen.chip_view_start_padding);
        int defaultEndPadding =
                mActivity.getResources().getDimensionPixelSize(R.dimen.chip_view_end_padding);
        int compactPadding =
                mActivity.getResources().getDimensionPixelSize(R.dimen.chip_view_compact_padding);

        mChipView.setIsCompact(true);
        assertTrue(mChipView.isCompact());
        assertEquals(View.GONE, mChipView.getPrimaryTextView().getVisibility());
        assertEquals(compactPadding, mChipView.getPaddingStart());
        assertEquals(compactPadding, mChipView.getPaddingEnd());

        mChipView.setIsCompact(false);
        assertFalse(mChipView.isCompact());
        assertEquals(View.VISIBLE, mChipView.getPrimaryTextView().getVisibility());
        assertEquals(defaultStartPadding, mChipView.getPaddingStart());
        assertEquals(defaultEndPadding, mChipView.getPaddingEnd());
    }

    @Test
    @SmallTest
    public void compactWidthDelta_matchesMeasuredDelta_primaryTextOnly() {
        mChipView.setIcon(R.drawable.test_ic_arrow_downward_black_24dp, false);
        mChipView.setText("Primary text");

        mChipView.setIsCompact(true);
        measureChip(mChipView);
        int compactWidth = mChipView.getMeasuredWidth();

        mChipView.setIsCompact(false);
        measureChip(mChipView);
        int expandedWidth = mChipView.getMeasuredWidth();

        int expectedDelta = expandedWidth - compactWidth;
        assertEquals(expectedDelta, mChipView.getCompactWidthDelta());
    }

    @Test
    @SmallTest
    public void compactWidthDelta_matchesMeasuredDelta_withRemoveIcon() {
        mChipView.setIcon(R.drawable.test_ic_arrow_downward_black_24dp, false);
        mChipView.setText("Primary text");
        mChipView.addRemoveIcon();

        mChipView.setIsCompact(true);
        measureChip(mChipView);
        int compactWidth = mChipView.getMeasuredWidth();

        mChipView.setIsCompact(false);
        measureChip(mChipView);
        int expandedWidth = mChipView.getMeasuredWidth();

        int expectedDelta = expandedWidth - compactWidth;
        assertEquals(expectedDelta, mChipView.getCompactWidthDelta());
    }

    @Test
    @SmallTest
    public void compactWidthDelta_matchesMeasuredDelta_primaryAndSecondaryText() {
        mChipView.setIcon(R.drawable.test_ic_arrow_downward_black_24dp, false);
        mChipView.setText("Primary text");
        mChipView.getSecondaryTextView().setText("Secondary text");

        mChipView.setIsCompact(true);
        measureChip(mChipView);
        int compactWidth = mChipView.getMeasuredWidth();

        mChipView.setIsCompact(false);
        measureChip(mChipView);
        int expandedWidth = mChipView.getMeasuredWidth();

        int expectedDelta = expandedWidth - compactWidth;
        assertEquals(expectedDelta, mChipView.getCompactWidthDelta());
    }

    @Test
    @SmallTest
    public void compactWidthDelta_matchesMeasuredDelta_twoLineWrapper_primaryLonger() {
        mTwoLineChipView.setIcon(R.drawable.test_ic_arrow_downward_black_24dp, false);
        mTwoLineChipView.getPrimaryTextView().setText("A very long primary text label");
        mTwoLineChipView.getSecondaryTextView().setText("Short");

        mTwoLineChipView.setIsCompact(true);
        measureChip(mTwoLineChipView);
        int compactWidth = mTwoLineChipView.getMeasuredWidth();

        mTwoLineChipView.setIsCompact(false);
        measureChip(mTwoLineChipView);
        int expandedWidth = mTwoLineChipView.getMeasuredWidth();

        int expectedDelta = expandedWidth - compactWidth;
        assertEquals(expectedDelta, mTwoLineChipView.getCompactWidthDelta());
    }

    @Test
    @SmallTest
    public void compactWidthDelta_matchesMeasuredDelta_twoLineWrapper_secondaryLonger() {
        mTwoLineChipView.setIcon(R.drawable.test_ic_arrow_downward_black_24dp, false);
        mTwoLineChipView.getPrimaryTextView().setText("Short");
        mTwoLineChipView.getSecondaryTextView().setText("A very long secondary text label");

        mTwoLineChipView.setIsCompact(true);
        measureChip(mTwoLineChipView);
        int compactWidth = mTwoLineChipView.getMeasuredWidth();

        mTwoLineChipView.setIsCompact(false);
        measureChip(mTwoLineChipView);
        int expandedWidth = mTwoLineChipView.getMeasuredWidth();

        int expectedDelta = expandedWidth - compactWidth;
        assertEquals(expectedDelta, mTwoLineChipView.getCompactWidthDelta());
    }

    private void measureChip(ChipView chip) {
        chip.measure(makeMeasureSpec(0, UNSPECIFIED), makeMeasureSpec(0, UNSPECIFIED));
    }
}
