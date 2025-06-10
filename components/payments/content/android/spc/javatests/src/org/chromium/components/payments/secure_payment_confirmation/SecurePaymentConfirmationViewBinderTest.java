// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.text.SpannableString;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.payments.R;
import org.chromium.components.payments.secure_payment_confirmation.SecurePaymentConfirmationProperties.ItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Integration tests for {@link SecurePaymentConfirmationViewBinder} */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class SecurePaymentConfirmationViewBinderTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static final BitmapDrawable TEST_BITMAP =
            new BitmapDrawable(
                    ContextUtils.getApplicationContext().getResources(),
                    Bitmap.createBitmap(
                            new int[] {Color.RED},
                            /* width= */ 1,
                            /* height= */ 1,
                            Bitmap.Config.ARGB_8888));

    private PropertyModel.Builder mModelBuilder;
    private PropertyModel mModel;
    private SecurePaymentConfirmationView mView;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        mModelBuilder = new PropertyModel.Builder(SecurePaymentConfirmationProperties.ALL_KEYS);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mView = new SecurePaymentConfirmationView(sActivityTestRule.getActivity());
                    sActivityTestRule.getActivity().setContentView(mView.mContentView);
                });
        bind(mModelBuilder);
    }

    @Test
    @SmallTest
    public void testScrollView() {
        assertEquals(R.id.scroll_view, mView.mScrollView.getId());
    }

    @Test
    @SmallTest
    public void testHeaderImage() {
        bind(
                mModelBuilder.with(
                        SecurePaymentConfirmationProperties.SHOWS_ISSUER_NETWORK_ICONS, false));

        assertEquals(R.id.secure_payment_confirmation_image, mView.mHeaderImage.getId());
        assertEquals(View.VISIBLE, mView.mHeaderImage.getVisibility());
        assertNotNull(mView.mHeaderImage.getDrawable());
        assertEquals(View.GONE, mView.mIssuerNetworkIconsRow.getVisibility());
    }

    @Test
    @SmallTest
    public void testShowIssuerNetworkIcons() {
        bind(
                mModelBuilder.with(
                        SecurePaymentConfirmationProperties.SHOWS_ISSUER_NETWORK_ICONS, true));

        assertEquals(R.id.issuer_network_icons_row, mView.mIssuerNetworkIconsRow.getId());
        assertEquals(View.VISIBLE, mView.mIssuerNetworkIconsRow.getVisibility());
        assertEquals(View.GONE, mView.mHeaderImage.getVisibility());
    }

    @Test
    @SmallTest
    public void testIssuerIcon() {
        assertEquals(R.id.issuer_icon, mView.mIssuerIcon.getId());
        assertNull(mView.mIssuerIcon.getDrawable());

        bind(mModelBuilder.with(SecurePaymentConfirmationProperties.ISSUER_ICON, TEST_BITMAP));

        assertEquals(View.VISIBLE, mView.mIssuerIcon.getVisibility());
        assertSame(TEST_BITMAP, mView.mIssuerIcon.getDrawable());
    }

    @Test
    @SmallTest
    public void testNetworkIcon() {
        assertEquals(R.id.network_icon, mView.mNetworkIcon.getId());
        assertNull(mView.mNetworkIcon.getDrawable());

        bind(mModelBuilder.with(SecurePaymentConfirmationProperties.NETWORK_ICON, TEST_BITMAP));

        assertEquals(View.VISIBLE, mView.mNetworkIcon.getVisibility());
        assertSame(TEST_BITMAP, mView.mNetworkIcon.getDrawable());
    }

    @Test
    @SmallTest
    public void testTitle() {
        final String titleText = "Title";
        bind(mModelBuilder.with(SecurePaymentConfirmationProperties.TITLE, titleText));

        assertEquals(R.id.secure_payment_confirmation_title, mView.mTitle.getId());
        assertEquals(titleText, String.valueOf(mView.mTitle.getText()));
    }

    @Test
    @SmallTest
    public void testItemList() {
        final BitmapDrawable icon = TEST_BITMAP;
        final String iconLabel = "label";
        final String primaryText = "primary text";
        final String secondaryText = "secondary text";

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
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
                    SimpleRecyclerViewAdapter itemListAdapter =
                            new SimpleRecyclerViewAdapter(itemList);
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
                            ((SimpleRecyclerViewAdapter) mView.mItemList.getAdapter())
                                    .getModelList());
                });

        RecyclerViewTestUtils.waitForStableRecyclerView(mView.mItemList);
        assertEquals(1, mView.mItemList.getChildCount());
        View itemView = mView.mItemList.getChildAt(0);
        assertSame(icon, ((ImageView) itemView.findViewById(R.id.icon)).getDrawable());
        assertEquals(iconLabel, itemView.findViewById(R.id.icon).getContentDescription());
        assertEquals(primaryText, ((TextView) itemView.findViewById(R.id.primary_text)).getText());
        assertEquals(
                secondaryText, ((TextView) itemView.findViewById(R.id.secondary_text)).getText());
    }

    @Test
    @SmallTest
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
    @SmallTest
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
    @SmallTest
    public void testContinueButtonLabel() {
        final String continueText = "Continue";
        bind(
                mModelBuilder.with(
                        SecurePaymentConfirmationProperties.CONTINUE_BUTTON_LABEL, continueText));

        assertEquals(R.id.continue_button, mView.mContinueButton.getId());
        assertEquals(continueText, String.valueOf(mView.mContinueButton.getText()));
    }

    private void bind(PropertyModel.Builder modelBuilder) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel = modelBuilder.build();
                    PropertyModelChangeProcessor.create(
                            mModel, mView, SecurePaymentConfirmationViewBinder::bind);
                });
    }
}
