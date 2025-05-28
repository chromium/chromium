// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.text.SpannableString;
import android.util.Pair;
import android.view.View;
import android.view.ViewGroup.LayoutParams;

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
import org.chromium.components.payments.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
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
        mView = new SecurePaymentConfirmationView(sActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> sActivityTestRule.getActivity().setContentView(mView.mContentView));
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
    public void testStoreLabel() {
        final String storeText = "Store";
        bind(mModelBuilder.with(SecurePaymentConfirmationProperties.STORE_LABEL, storeText));

        assertEquals(R.id.store, mView.mStoreLabel.getId());
        assertEquals(storeText, String.valueOf(mView.mStoreLabel.getText()));
    }

    @Test
    @SmallTest
    public void testPaymentIcon_withDefaultIcon() {
        assertEquals(R.id.payment_icon, mView.mPaymentIcon.getId());
        assertNull(mView.mPaymentIcon.getDrawable());

        bind(
                mModelBuilder.with(
                        SecurePaymentConfirmationProperties.PAYMENT_ICON,
                        Pair.create(TEST_BITMAP, /* is_default_icon= */ true)));

        assertEquals(View.VISIBLE, mView.mPaymentIcon.getVisibility());
        assertSame(TEST_BITMAP, mView.mPaymentIcon.getDrawable());
        assertEquals(LayoutParams.WRAP_CONTENT, mView.mPaymentIcon.getLayoutParams().height);
        assertEquals(LayoutParams.WRAP_CONTENT, mView.mPaymentIcon.getLayoutParams().width);
    }

    @Test
    @SmallTest
    public void testPaymentIcon_withoutDefaultIcon() {
        assertEquals(R.id.payment_icon, mView.mPaymentIcon.getId());
        assertNull(mView.mPaymentIcon.getDrawable());

        bind(
                mModelBuilder.with(
                        SecurePaymentConfirmationProperties.PAYMENT_ICON,
                        Pair.create(TEST_BITMAP, /* is_default_icon= */ false)));

        assertEquals(View.VISIBLE, mView.mPaymentIcon.getVisibility());
        assertSame(TEST_BITMAP, mView.mPaymentIcon.getDrawable());
        assertNotEquals(LayoutParams.WRAP_CONTENT, mView.mPaymentIcon.getLayoutParams().height);
        assertNotEquals(LayoutParams.WRAP_CONTENT, mView.mPaymentIcon.getLayoutParams().width);
    }

    @Test
    @SmallTest
    public void testPaymentInstrumentLabel() {
        final String paymentText = "Payment";
        bind(
                mModelBuilder.with(
                        SecurePaymentConfirmationProperties.PAYMENT_INSTRUMENT_LABEL, paymentText));

        assertEquals(R.id.payment, mView.mPaymentInstrumentLabel.getId());
        assertEquals(paymentText, String.valueOf(mView.mPaymentInstrumentLabel.getText()));
    }

    @Test
    @SmallTest
    public void testCurrency() {
        final String currency = "CAD";
        bind(mModelBuilder.with(SecurePaymentConfirmationProperties.CURRENCY, currency));

        assertEquals(R.id.currency, mView.mCurrency.getId());
        assertEquals(currency, String.valueOf(mView.mCurrency.getText()));
    }

    @Test
    @SmallTest
    public void testTotal() {
        final String total = "1.50";
        bind(mModelBuilder.with(SecurePaymentConfirmationProperties.TOTAL, total));

        assertEquals(R.id.total, mView.mTotal.getId());
        assertEquals(total, String.valueOf(mView.mTotal.getText()));
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

    @Test
    @SmallTest
    public void testCancelButton() {
        assertEquals(R.id.cancel_button, mView.mCancelButton.getId());
        assertEquals(
                ContextUtils.getApplicationContext().getString(R.string.cancel),
                String.valueOf(mView.mCancelButton.getText()));
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
