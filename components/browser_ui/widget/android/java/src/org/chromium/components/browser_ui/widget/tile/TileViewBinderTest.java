// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.tile;

import static org.mockito.Mockito.mock;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.test.BrowserUiDummyFragmentActivity;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link TileViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class TileViewBinderTest {
    private Activity mActivity;
    private TileView mTileView;
    private ImageView mIconView;
    private TextView mTitleView;
    private ImageView mBadgeView;
    private PropertyModel mModel;

    private Resources mResources;
    private int mLargeIconEdgeSize;
    private int mSmallIconEdgeSize;
    private int mLargeIconTopMarginSize;
    private int mSmallIconTopMarginSize;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(BrowserUiDummyFragmentActivity.class).setup().get();

        mTileView = new TileView(mActivity, null);
        LayoutInflater.from(mActivity).inflate(R.layout.tile_view_modern, mTileView, true);
        // Note: this call is only needed as long as the tile_view_modern layout is a
        // <merge> layout, because LayoutInflater will not call this method in such cases.
        mTileView.onFinishInflate();

        mIconView = mTileView.findViewById(R.id.tile_view_icon);
        mTitleView = mTileView.findViewById(R.id.tile_view_title);
        mBadgeView = mTileView.findViewById(R.id.offline_badge);

        mResources = mActivity.getResources();

        mLargeIconEdgeSize = mResources.getDimensionPixelSize(R.dimen.tile_view_icon_size);
        mSmallIconEdgeSize = mResources.getDimensionPixelSize(R.dimen.tile_view_icon_size_modern);
        mLargeIconTopMarginSize =
                mResources.getDimensionPixelSize(
                        R.dimen.tile_view_icon_background_margin_top_modern);
        mSmallIconTopMarginSize =
                mResources.getDimensionPixelSize(R.dimen.tile_view_icon_margin_top_modern);

        mModel = new PropertyModel(TileViewProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mTileView, TileViewBinder::bind);
    }

    @Test
    @SmallTest
    public void testTitleTextPropertySet() {
        mModel.set(TileViewProperties.TITLE, "Testing Title");
        Assert.assertEquals("Testing Title", mTitleView.getText());

        mModel.set(TileViewProperties.TITLE, null);
        Assert.assertTrue(TextUtils.isEmpty(mTitleView.getText()));
    }

    @Test
    @SmallTest
    public void testTitleLinesPropertySet() {
        mModel.set(TileViewProperties.TITLE_LINES, 1);
        Assert.assertEquals(1, mTitleView.getMaxLines());

        mModel.set(TileViewProperties.TITLE_LINES, 100);
        Assert.assertEquals(100, mTitleView.getMaxLines());

        // Disallow invalid number of lines.
        mModel.set(TileViewProperties.TITLE_LINES, 0);
        Assert.assertEquals(1, mTitleView.getMaxLines());

        mModel.set(TileViewProperties.TITLE_LINES, -10);
        Assert.assertEquals(1, mTitleView.getMaxLines());
    }

    @Test
    @SmallTest
    public void testIconDrawablePropertySet() {
        Drawable drawable1 = mock(Drawable.class);
        Drawable drawable2 = mock(Drawable.class);

        Assert.assertEquals(null, mIconView.getDrawable());

        mModel.set(TileViewProperties.ICON, drawable1);
        Assert.assertEquals(drawable1, mIconView.getDrawable());

        mModel.set(TileViewProperties.ICON, drawable2);
        Assert.assertEquals(drawable2, mIconView.getDrawable());
    }

    @Test
    @SmallTest
    public void testBadgeVisiblePropertySet() {
        mModel.set(TileViewProperties.BADGE_VISIBLE, true);
        Assert.assertEquals(View.VISIBLE, mBadgeView.getVisibility());

        mModel.set(TileViewProperties.BADGE_VISIBLE, false);
        Assert.assertEquals(View.GONE, mBadgeView.getVisibility());
    }

    @Test
    @SmallTest
    public void testShowLargeIconPropertySet() {
        final int smallIconRoundingRadius = 13;

        Drawable drawable = mock(Drawable.class);
        mModel.set(TileViewProperties.ICON, drawable);
        mModel.set(TileViewProperties.SMALL_ICON_ROUNDING_RADIUS, smallIconRoundingRadius);

        // By default the SHOW_LARGE_ICON is false, so the small radius should be applied to round
        // the view.
        Assert.assertEquals(smallIconRoundingRadius, mTileView.getRoundingRadiusForTesting());
        Assert.assertEquals(mSmallIconEdgeSize, mIconView.getLayoutParams().width);
        Assert.assertEquals(mSmallIconEdgeSize, mIconView.getLayoutParams().height);

        mModel.set(TileViewProperties.SHOW_LARGE_ICON, true);
        // Expect the code to pick the large view edge size as the rounding radius.
        // In principle anything that is larger than half the size of the longer of
        // the edges of the view would suffice.
        Assert.assertEquals(mLargeIconEdgeSize, mTileView.getRoundingRadiusForTesting());
        Assert.assertEquals(mLargeIconEdgeSize, mIconView.getLayoutParams().width);
        Assert.assertEquals(mLargeIconEdgeSize, mIconView.getLayoutParams().height);

        // Confirm that reverting the icon size reapplies previously elected rounding radius.
        mModel.set(TileViewProperties.SHOW_LARGE_ICON, false);
        Assert.assertEquals(smallIconRoundingRadius, mTileView.getRoundingRadiusForTesting());
        Assert.assertEquals(mSmallIconEdgeSize, mIconView.getLayoutParams().width);
        Assert.assertEquals(mSmallIconEdgeSize, mIconView.getLayoutParams().height);
    }

    @Test
    @SmallTest
    public void testSmallIconRoundingRadiusPropertySet() {
        // Initial value should be 0, matching properties.
        Assert.assertEquals(0, mTileView.getRoundingRadiusForTesting());

        mModel.set(TileViewProperties.SMALL_ICON_ROUNDING_RADIUS, 10);
        Assert.assertEquals(10, mTileView.getRoundingRadiusForTesting());

        mModel.set(TileViewProperties.SMALL_ICON_ROUNDING_RADIUS, 0);
        Assert.assertEquals(0, mTileView.getRoundingRadiusForTesting());
    }
}
