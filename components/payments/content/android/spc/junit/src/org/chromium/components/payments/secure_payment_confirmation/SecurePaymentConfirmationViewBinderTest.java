// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;

import static java.util.Collections.emptyList;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.text.SpannableString;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.payments.PaymentApp.PaymentEntityLogo;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationProperties.ItemProperties;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.Arrays;
import java.util.List;

/** Integration tests for {@link SecurePaymentConfirmationViewBinder} */
@RunWith(BaseRobolectricTestRunner.class)
public class SecurePaymentConfirmationViewBinderTest {

    static class TestPaymentEntityLogo implements PaymentEntityLogo {
        private final Bitmap mIcon;
        private final String mLabel;

        TestPaymentEntityLogo(Bitmap icon, String label) {
            mIcon = icon;
            mLabel = label;
        }

        @Override
        public String getLabel() {
            return mLabel;
        }

        @Override
        public Bitmap getIcon() {
            return mIcon;
        }
    }

    private static final BitmapDrawable TEST_BITMAP =
            new BitmapDrawable(
                    ContextUtils.getApplicationContext().getResources(),
                    Bitmap.createBitmap(
                            new int[] {Color.RED},
                            /* width= */ 1,
                            /* height= */ 1,
                            Bitmap.Config.ARGB_8888));

    private Activity mActivity;
    private PropertyModel.Builder mModelBuilder;
    private PropertyModel mModel;
    private SecurePaymentConfirmationView mView;

    @Before
    public void setUp() {
        ActivityController<TestActivity> controller = Robolectric.buildActivity(TestActivity.class);
        controller.get().setTheme(R.style.Theme_BrowserUI_DayNight);
        mActivity = controller.setup().get();
        mModelBuilder = new PropertyModel.Builder(SecurePaymentConfirmationProperties.ALL_KEYS);

        mView = new SecurePaymentConfirmationView(mActivity);
        mActivity.setContentView(mView.mContentView);
        bind(mModelBuilder);
    }

    @Test
    public void testScrollView() {
        assertEquals(R.id.scroll_view, mView.mScrollView.getId());
    }

    @Test
    public void testHeaderIllustration() {
        assertEquals(
                R.id.secure_payment_confirmation_header_illustration,
                mView.mHeaderIllustration.getId());
        assertEquals(View.VISIBLE, mView.mHeaderIllustration.getVisibility());
        assertNotNull(mView.mHeaderIllustration.getDrawable());
        assertEquals(View.GONE, mView.mHeaderLogosRow.getVisibility());

        bind(mModelBuilder.with(SecurePaymentConfirmationProperties.HEADER_LOGOS, emptyList()));

        assertEquals(View.VISIBLE, mView.mHeaderIllustration.getVisibility());
        assertEquals(View.GONE, mView.mHeaderLogosRow.getVisibility());
    }

    @Test
    public void testTwoHeaderLogos() {
        assertEquals(R.id.header_logos_row, mView.mHeaderLogosRow.getId());

        List<PaymentEntityLogo> headerLogos =
                Arrays.asList(
                        new TestPaymentEntityLogo(
                                Bitmap.createBitmap(
                                        new int[] {Color.GREEN},
                                        /* width= */ 1,
                                        /* height= */ 1,
                                        Bitmap.Config.ARGB_8888),
                                "first logo label"),
                        new TestPaymentEntityLogo(
                                Bitmap.createBitmap(
                                        new int[] {Color.BLUE},
                                        /* width= */ 1,
                                        /* height= */ 1,
                                        Bitmap.Config.ARGB_8888),
                                "second logo label"));

        bind(mModelBuilder.with(SecurePaymentConfirmationProperties.HEADER_LOGOS, headerLogos));

        assertEquals(View.GONE, mView.mHeaderIllustration.getVisibility());
        assertEquals(View.VISIBLE, mView.mHeaderLogosRow.getVisibility());

        assertEquals(R.id.header_logos_divider, mView.mHeaderLogosDivider.getId());
        assertEquals(View.VISIBLE, mView.mHeaderLogosDivider.getVisibility());

        assertEquals(R.id.header_logo_primary, mView.mHeaderLogoPrimary.getId());
        assertSame(
                headerLogos.get(0).getIcon(),
                ((BitmapDrawable) mView.mHeaderLogoPrimary.getDrawable()).getBitmap());
        assertEquals(
                headerLogos.get(0).getLabel(), mView.mHeaderLogoPrimary.getContentDescription());

        assertEquals(R.id.header_logo_secondary, mView.mHeaderLogoSecondary.getId());
        assertEquals(View.VISIBLE, mView.mHeaderLogoSecondary.getVisibility());
        assertSame(
                headerLogos.get(1).getIcon(),
                ((BitmapDrawable) mView.mHeaderLogoSecondary.getDrawable()).getBitmap());
        assertEquals(
                headerLogos.get(1).getLabel(), mView.mHeaderLogoSecondary.getContentDescription());
    }

    @Test
    public void testOneHeaderLogo() {
        List<PaymentEntityLogo> headerLogos =
                List.of(
                        new TestPaymentEntityLogo(
                                Bitmap.createBitmap(
                                        new int[] {Color.GREEN},
                                        /* width= */ 1,
                                        /* height= */ 1,
                                        Config.ARGB_8888),
                                "first logo label"));

        bind(mModelBuilder.with(SecurePaymentConfirmationProperties.HEADER_LOGOS, headerLogos));

        assertEquals(View.GONE, mView.mHeaderIllustration.getVisibility());
        assertEquals(View.VISIBLE, mView.mHeaderLogosRow.getVisibility());

        assertEquals(View.GONE, mView.mHeaderLogosDivider.getVisibility());

        assertSame(
                headerLogos.get(0).getIcon(),
                ((BitmapDrawable) mView.mHeaderLogoPrimary.getDrawable()).getBitmap());
        assertEquals(
                headerLogos.get(0).getLabel(), mView.mHeaderLogoPrimary.getContentDescription());

        assertEquals(View.GONE, mView.mHeaderLogoSecondary.getVisibility());
    }

    @Test
    public void testTitle() {
        final String titleText = "Title";
        bind(mModelBuilder.with(SecurePaymentConfirmationProperties.TITLE, titleText));

        assertEquals(R.id.secure_payment_confirmation_title, mView.mTitle.getId());
        assertEquals(titleText, String.valueOf(mView.mTitle.getText()));
    }

    @Test
    public void testItemList() {
        final BitmapDrawable icon = TEST_BITMAP;
        final String iconLabel = "label";
        final String primaryText = "primary text";
        final String secondaryText = "secondary text";

        ModelList itemList = new ModelList();
        itemList.add(
                new ListItem(
                        /* type= */ 0,
                        new PropertyModel.Builder(ItemProperties.ALL_KEYS)
                                .with(ItemProperties.ICON, icon)
                                .with(ItemProperties.ICON_LABEL, iconLabel)
                                .with(ItemProperties.PRIMARY_TEXT, primaryText)
                                .with(ItemProperties.SECONDARY_TEXT, secondaryText)
                                .build()));
        SimpleRecyclerViewAdapter itemListAdapter = new SimpleRecyclerViewAdapter(itemList);
        itemListAdapter.registerType(
                /* typeId= */ 0,
                SecurePaymentConfirmationView::createItemView,
                SecurePaymentConfirmationViewBinder::bindItem);

        mModel =
                mModelBuilder
                        .with(
                                SecurePaymentConfirmationProperties.ITEM_LIST_ADAPTER,
                                itemListAdapter)
                        .build();
        PropertyModelChangeProcessor.create(
                mModel, mView, SecurePaymentConfirmationViewBinder::bind);

        assertNotNull(mView.mItemList.getAdapter());
        assertEquals(
                itemList,
                ((SimpleRecyclerViewAdapter) mView.mItemList.getAdapter()).getModelList());

        mView.mItemList.measure(0, 0);
        mView.mItemList.layout(0, 0, 100, 1000);
        assertEquals(1, mView.mItemList.getChildCount());
        View itemView = mView.mItemList.getChildAt(0);
        assertSame(icon, ((ImageView) itemView.findViewById(R.id.icon)).getDrawable());
        assertEquals(iconLabel, itemView.findViewById(R.id.icon).getContentDescription());
        assertEquals(primaryText, ((TextView) itemView.findViewById(R.id.primary_text)).getText());
        assertEquals(
                TextUtils.TruncateAt.END,
                ((TextView) itemView.findViewById(R.id.primary_text)).getEllipsize());
        assertEquals(View.VISIBLE, itemView.findViewById(R.id.secondary_text).getVisibility());
        assertEquals(
                secondaryText, ((TextView) itemView.findViewById(R.id.secondary_text)).getText());
        assertEquals(
                TextUtils.TruncateAt.END,
                ((TextView) itemView.findViewById(R.id.secondary_text)).getEllipsize());
    }

    @Test
    public void testItemListWhenSecondaryTextEmpty() {
        ModelList itemList = new ModelList();
        itemList.add(
                new ListItem(
                        /* type= */ 0,
                        new PropertyModel.Builder(ItemProperties.ALL_KEYS)
                                .with(ItemProperties.ICON, TEST_BITMAP)
                                .with(ItemProperties.ICON_LABEL, "label")
                                .with(ItemProperties.PRIMARY_TEXT, "text")
                                .with(ItemProperties.SECONDARY_TEXT, "")
                                .build()));
        SimpleRecyclerViewAdapter itemListAdapter = new SimpleRecyclerViewAdapter(itemList);
        itemListAdapter.registerType(
                /* typeId= */ 0,
                SecurePaymentConfirmationView::createItemView,
                SecurePaymentConfirmationViewBinder::bindItem);

        mModel =
                mModelBuilder
                        .with(
                                SecurePaymentConfirmationProperties.ITEM_LIST_ADAPTER,
                                itemListAdapter)
                        .build();
        PropertyModelChangeProcessor.create(
                mModel, mView, SecurePaymentConfirmationViewBinder::bind);

        mView.mItemList.measure(0, 0);
        mView.mItemList.layout(0, 0, 100, 1000);
        assertEquals(
                View.GONE,
                mView.mItemList.getChildAt(0).findViewById(R.id.secondary_text).getVisibility());
    }

    @Test
    public void testItemListWhenIconLabelNotProvided() {
        ModelList itemList = new ModelList();
        itemList.add(
                new ListItem(
                        /* type= */ 0,
                        new PropertyModel.Builder(ItemProperties.ALL_KEYS)
                                .with(ItemProperties.ICON, TEST_BITMAP)
                                // ICON_LABEL intentionally not provided to test
                                // fallback.
                                .with(ItemProperties.PRIMARY_TEXT, "text")
                                .with(ItemProperties.SECONDARY_TEXT, "")
                                .build()));
        SimpleRecyclerViewAdapter itemListAdapter = new SimpleRecyclerViewAdapter(itemList);
        itemListAdapter.registerType(
                /* typeId= */ 0,
                SecurePaymentConfirmationView::createItemView,
                SecurePaymentConfirmationViewBinder::bindItem);

        mModel =
                mModelBuilder
                        .with(
                                SecurePaymentConfirmationProperties.ITEM_LIST_ADAPTER,
                                itemListAdapter)
                        .build();
        PropertyModelChangeProcessor.create(
                mModel, mView, SecurePaymentConfirmationViewBinder::bind);

        mView.mItemList.measure(0, 0);
        mView.mItemList.layout(0, 0, 100, 1000);
        View itemView = mView.mItemList.getChildAt(0);
        assertEquals(
                itemView.getContext().getString(R.string.payments_instrument_icon),
                itemView.findViewById(R.id.icon).getContentDescription());
    }

    @Test
    public void testOptOutText() {
        assertEquals(
                R.id.secure_payment_confirmation_nocredmatch_opt_out, mView.mOptOutText.getId());

        bind(mModelBuilder.with(SecurePaymentConfirmationProperties.OPT_OUT_TEXT, null));
        assertEquals(View.GONE, mView.mOptOutText.getVisibility());

        SpannableString optOutText = new SpannableString("Opt out text");
        bind(mModelBuilder.with(SecurePaymentConfirmationProperties.OPT_OUT_TEXT, optOutText));
        assertEquals(View.VISIBLE, mView.mOptOutText.getVisibility());
        assertEquals(optOutText.toString(), mView.mOptOutText.getText().toString());
    }

    @Test
    public void testFootnote() {
        assertEquals(R.id.secure_payment_confirmation_footnote, mView.mFootnote.getId());

        bind(mModelBuilder.with(SecurePaymentConfirmationProperties.FOOTNOTE, null));
        assertEquals(View.GONE, mView.mFootnote.getVisibility());

        SpannableString footnote = new SpannableString("Footnote");
        bind(mModelBuilder.with(SecurePaymentConfirmationProperties.FOOTNOTE, footnote));
        assertEquals(View.VISIBLE, mView.mFootnote.getVisibility());
        assertEquals(footnote.toString(), mView.mFootnote.getText().toString());
    }

    @Test
    public void testContinueButtonLabel() {
        final String continueText = "Continue";
        bind(
                mModelBuilder.with(
                        SecurePaymentConfirmationProperties.CONTINUE_BUTTON_LABEL, continueText));

        assertEquals(R.id.continue_button, mView.mContinueButton.getId());
        assertEquals(continueText, String.valueOf(mView.mContinueButton.getText()));
    }

    private void bind(PropertyModel.Builder modelBuilder) {
        mModel = modelBuilder.build();
        PropertyModelChangeProcessor.create(
                mModel, mView, SecurePaymentConfirmationViewBinder::bind);
    }
}
