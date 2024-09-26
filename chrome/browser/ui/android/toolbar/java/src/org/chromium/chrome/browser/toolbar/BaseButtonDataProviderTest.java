// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Unit test for {@link BaseButtonDataProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2})
public class BaseButtonDataProviderTest {
    private static class TestButtonDataProvider extends BaseButtonDataProvider {
        public TestButtonDataProvider(
                Supplier<Tab> activeTabSupplier,
                @Nullable ModalDialogManager modalDialogManager,
                Drawable buttonDrawable,
                String contentDescription,
                int actionChipLabelResId,
                boolean supportsTinting,
                int adaptiveButtonVariant) {
            super(
                    activeTabSupplier,
                    modalDialogManager,
                    buttonDrawable,
                    contentDescription,
                    actionChipLabelResId,
                    supportsTinting,
                    null,
                    adaptiveButtonVariant,
                    Resources.ID_NULL,
                    false);
        }

        @Override
        protected IPHCommandBuilder getIphCommandBuilder(Tab tab) {
            IPHCommandBuilder iphCommandBuilder =
                    new IPHCommandBuilder(
                            tab.getContext().getResources(),
                            FeatureConstants.CONTEXTUAL_PAGE_ACTIONS_QUIET_VARIANT,
                            /* stringId= */ R.string.iph_price_tracking_menu_item,
                            /* accessibilityStringId= */ R.string.iph_price_tracking_menu_item);
            return iphCommandBuilder;
        }

        @Override
        public void onClick(View view) {}
    }

    private Activity mActivity;
    @Mock private Tab mMockTab;
    @Mock private ObservableSupplier<Tab> mMockTabSupplier;
    @Mock private ModalDialogManager mMockModalDialogManager;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        MockitoAnnotations.initMocks(this);

        when(mMockTab.getContext()).thenReturn(mActivity);
        when(mMockTabSupplier.get()).thenReturn(mMockTab);
    }

    @Test
    public void testButtonData_QuietVariation() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2, true);
        FeatureList.setTestValues(testValues);
        AdaptiveToolbarFeatures.setActionChipOverrideForTesting(
                AdaptiveToolbarButtonVariant.READER_MODE, false);

        TestButtonDataProvider testButtonDataProvider =
                new TestButtonDataProvider(
                        mMockTabSupplier,
                        mMockModalDialogManager,
                        mock(Drawable.class),
                        mActivity.getString(R.string.enable_price_tracking_menu_item),
                        /* actionChipLabelResId= */ R.string.enable_price_tracking_menu_item,
                        /* supportsTinting= */ true,
                        AdaptiveToolbarButtonVariant.READER_MODE);
        ButtonData buttonData = testButtonDataProvider.get(mMockTab);

        // Quiet variation uses an IPHCommandBuilder to highlight the action.
        Assert.assertNotNull(buttonData.getButtonSpec().getIPHCommandBuilder());
    }

    @Test
    public void testButtonData_ActionChipVariation() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2, true);
        FeatureList.setTestValues(testValues);
        AdaptiveToolbarFeatures.setActionChipOverrideForTesting(
                AdaptiveToolbarButtonVariant.READER_MODE, true);

        TestButtonDataProvider testButtonDataProvider =
                new TestButtonDataProvider(
                        mMockTabSupplier,
                        mMockModalDialogManager,
                        mock(Drawable.class),
                        mActivity.getString(R.string.enable_price_tracking_menu_item),
                        /* actionChipLabelResId= */ R.string.enable_price_tracking_menu_item,
                        /* supportsTinting= */ true,
                        AdaptiveToolbarButtonVariant.READER_MODE);
        ButtonData buttonData = testButtonDataProvider.get(mMockTab);

        // Action chip variation should not set an IPH command builder.
        Assert.assertNull(buttonData.getButtonSpec().getIPHCommandBuilder());
    }

    @Test
    public void testSetShouldShowOnIncognito_defaultBehavior() {
        TestButtonDataProvider testButtonDataProvider =
                new TestButtonDataProvider(
                        mMockTabSupplier,
                        mMockModalDialogManager,
                        mock(Drawable.class),
                        mActivity.getString(R.string.enable_price_tracking_menu_item),
                        /* actionChipLabelResId= */ R.string.enable_price_tracking_menu_item,
                        /* supportsTinting= */ true,
                        AdaptiveToolbarButtonVariant.READER_MODE);

        when(mMockTab.isIncognito()).thenReturn(true);

        ButtonData buttonDataIncognitoTab = testButtonDataProvider.get(mMockTab);

        Assert.assertFalse(buttonDataIncognitoTab.canShow());
    }

    @Test
    public void testSetShouldShowOnIncognito_showOnIncognito() {
        TestButtonDataProvider testButtonDataProvider =
                new TestButtonDataProvider(
                        mMockTabSupplier,
                        mMockModalDialogManager,
                        mock(Drawable.class),
                        mActivity.getString(R.string.enable_price_tracking_menu_item),
                        /* actionChipLabelResId= */ R.string.enable_price_tracking_menu_item,
                        /* supportsTinting= */ true,
                        AdaptiveToolbarButtonVariant.READER_MODE);

        when(mMockTab.isIncognito()).thenReturn(true);
        testButtonDataProvider.setShouldShowOnIncognitoTabs(true);

        ButtonData buttonDataIncognitoTab = testButtonDataProvider.get(mMockTab);

        Assert.assertTrue(buttonDataIncognitoTab.canShow());
    }
}
