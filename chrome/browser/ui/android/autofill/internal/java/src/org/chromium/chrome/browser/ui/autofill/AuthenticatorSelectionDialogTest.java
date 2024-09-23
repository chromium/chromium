// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.RadioButton;
import android.widget.TextView;

import androidx.core.content.res.ResourcesCompat;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.autofill.AuthenticatorOptionsAdapter.AuthenticatorOptionViewHolder;
import org.chromium.chrome.browser.ui.autofill.data.AuthenticatorOption;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

import java.util.ArrayList;

/** Unit tests for {@link AuthenticatorSelectionDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_MOVING_GPAY_LOGO_TO_THE_RIGHT_ON_CLANK})
public class AuthenticatorSelectionDialogTest {
    // The icon set on the AuthenticatorOption is not important and any icon would do.
    private static final AuthenticatorOption OPTION_1 =
            new AuthenticatorOption.Builder()
                    .setTitle("title1")
                    .setIdentifier("identifier1")
                    .setDescription("description1")
                    .setIconResId(android.R.drawable.ic_media_pause)
                    .setType(CardUnmaskChallengeOptionType.SMS_OTP)
                    .build();

    private static final AuthenticatorOption OPTION_2 =
            new AuthenticatorOption.Builder()
                    .setTitle("title2")
                    .setIdentifier("identifier2")
                    .setDescription("description2")
                    .setIconResId(android.R.drawable.ic_media_play)
                    .setType(CardUnmaskChallengeOptionType.SMS_OTP)
                    .build();

    private static final AuthenticatorOption OPTION_3 =
            new AuthenticatorOption.Builder()
                    .setTitle("title3")
                    .setIdentifier("identifier3")
                    .setDescription("description3")
                    .setIconResId(android.R.drawable.ic_media_play)
                    .setType(CardUnmaskChallengeOptionType.CVC)
                    .build();

    private static final AuthenticatorOption OPTION_4 =
            new AuthenticatorOption.Builder()
                    .setTitle("title4")
                    .setIdentifier("identifier4")
                    .setDescription("description4")
                    .setIconResId(android.R.drawable.ic_media_play)
                    .setType(CardUnmaskChallengeOptionType.EMAIL_OTP)
                    .build();

    private FakeModalDialogManager mModalDialogManager;
    private AuthenticatorSelectionDialog mAuthenticatorSelectionDialog;
    private Resources mResources;
    @Mock private AuthenticatorSelectionDialog.Listener mAuthenticatorSelectedListener;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.TAB);
        mResources = ApplicationProvider.getApplicationContext().getResources();
        mAuthenticatorSelectionDialog =
                new AuthenticatorSelectionDialog(
                        ApplicationProvider.getApplicationContext(),
                        mAuthenticatorSelectedListener,
                        mModalDialogManager);
    }

    @Test
    @SmallTest
    public void testSingleAuthenticatorOption() throws Exception {
        ArrayList<AuthenticatorOption> options = new ArrayList<>();
        options.add(OPTION_1);

        mAuthenticatorSelectionDialog.show(options);

        PropertyModel model = mModalDialogManager.getShownDialogModel();
        assertThat(model).isNotNull();
        forceAuthenticatorOptionsViewLayout(model);
        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        RecyclerView authenticatorOptionsView =
                (RecyclerView) customView.findViewById(R.id.authenticator_options_view);
        assertThat(authenticatorOptionsView.getAdapter().getItemCount()).isEqualTo(1);
        // Verify that radio button is not shown for a single authenticator option and instead the
        // icon image is shown.
        AuthenticatorOptionViewHolder viewHolder =
                getAuthenticatorOptionViewHolderAtPosition(model, 0);
        assertThat(viewHolder.getRadioButton().getVisibility()).isEqualTo(View.GONE);
        assertThat(viewHolder.getIconImageView().getVisibility()).isEqualTo(View.VISIBLE);

        // Trigger positive button click.
        mModalDialogManager.clickPositiveButton();
        verify(mAuthenticatorSelectedListener, times(1)).onOptionSelected(OPTION_1.getIdentifier());
        // Verify that the progress bar is shown and the positive button is disabled.
        assertThat(customView.findViewById(R.id.progress_bar_overlay).getVisibility())
                .isEqualTo(View.VISIBLE);
        assertThat(model.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED)).isTrue();
    }

    @Test
    @SmallTest
    public void testDialogDismissed() {
        ArrayList<AuthenticatorOption> options = new ArrayList<>();
        options.add(OPTION_1);
        mAuthenticatorSelectionDialog.show(options);
        PropertyModel model = mModalDialogManager.getShownDialogModel();

        mModalDialogManager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);

        verify(mAuthenticatorSelectedListener, times(1)).onDialogDismissed();
    }

    @Test
    @SmallTest
    public void testMultipleAuthenticatorOption_noActionBeforeProceeding() throws Exception {
        ArrayList<AuthenticatorOption> options = new ArrayList<>();
        options.add(OPTION_1);
        options.add(OPTION_2);

        mAuthenticatorSelectionDialog.show(options);

        PropertyModel model = mModalDialogManager.getShownDialogModel();
        assertThat(model).isNotNull();
        forceAuthenticatorOptionsViewLayout(model);
        AuthenticatorOptionViewHolder viewHolder1 =
                getAuthenticatorOptionViewHolderAtPosition(model, 0);
        AuthenticatorOptionViewHolder viewHolder2 =
                getAuthenticatorOptionViewHolderAtPosition(model, 1);
        // Verify that radio button is shown for multiple authenticator options and the
        // icon image is hidden.
        assertThat(viewHolder1.getRadioButton().getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(viewHolder2.getRadioButton().getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(viewHolder1.getIconImageView().getVisibility()).isEqualTo(View.GONE);
        assertThat(viewHolder2.getIconImageView().getVisibility()).isEqualTo(View.GONE);
        // Verify that the first radio button is selected by default.
        assertThat(viewHolder1.getRadioButton().isChecked()).isTrue();
        assertThat(viewHolder2.getRadioButton().isChecked()).isFalse();

        // Trigger positive button click.
        mModalDialogManager.clickPositiveButton();

        // Verify that the listener is called with the correct identifier.
        verify(mAuthenticatorSelectedListener, times(1)).onOptionSelected(OPTION_1.getIdentifier());
    }

    @Test
    @SmallTest
    public void testMultipleAuthenticatorOption_changeToSmsOtpChallengeOption() throws Exception {
        PropertyModel model = createAndShowModelForChangeSelectedOptionTest();

        // Perform click for the radio button of authenticator option 2.
        getRadioButtonViewAt(model, 1).performClick();
        forceAuthenticatorOptionsViewLayout(model);
        assertThat(model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT)).isEqualTo("Send");

        // Verify that the radio button's checked state reflects correctly. Note: we need to fetch
        // the viewHolder again as the performClick triggered a redraw of the views.
        assertThat(getRadioButtonViewAt(model, 0).isChecked()).isFalse();
        assertThat(getRadioButtonViewAt(model, 1).isChecked()).isTrue();
        assertThat(getRadioButtonViewAt(model, 2).isChecked()).isFalse();
        assertThat(getRadioButtonViewAt(model, 3).isChecked()).isFalse();

        // Trigger positive button click.
        mModalDialogManager.clickPositiveButton();

        // Verify that the correct identifiers are passed to the listener.
        verify(mAuthenticatorSelectedListener, times(1)).onOptionSelected(OPTION_2.getIdentifier());
    }

    @Test
    @SmallTest
    public void
            testMultipleAuthenticatorOption_changeToSmsOtpChallengeOption_clickOnViewToSelectChallengeOption()
                    throws Exception {
        PropertyModel model = createAndShowModelForChangeSelectedOptionTest();

        // Perform click for the view of authenticator option 2.
        getAuthenticatorOptionViewAt(model, 1).performClick();
        forceAuthenticatorOptionsViewLayout(model);
        assertThat(model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT)).isEqualTo("Send");

        // Verify that the radio button's checked state reflects correctly. Note: we need to fetch
        // the viewHolder again as the performClick triggered a redraw of the views.
        assertThat(getRadioButtonViewAt(model, 0).isChecked()).isFalse();
        assertThat(getRadioButtonViewAt(model, 1).isChecked()).isTrue();
        assertThat(getRadioButtonViewAt(model, 2).isChecked()).isFalse();
        assertThat(getRadioButtonViewAt(model, 3).isChecked()).isFalse();

        // Trigger positive button click.
        mModalDialogManager.clickPositiveButton();

        // Verify that the correct identifiers are passed to the listener.
        verify(mAuthenticatorSelectedListener, times(1)).onOptionSelected(OPTION_2.getIdentifier());
    }

    @Test
    @SmallTest
    public void testMultipleAuthenticatorOption_changeToCvcChallengeOption() throws Exception {
        PropertyModel model = createAndShowModelForChangeSelectedOptionTest();

        // Perform click for the radio button of authenticator option 3.
        getRadioButtonViewAt(model, 2).performClick();
        forceAuthenticatorOptionsViewLayout(model);
        assertThat(model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT)).isEqualTo("Continue");

        // Verify that the radio button's checked state reflects correctly. Note: we need to fetch
        // the viewHolder again as the performClick triggered a redraw of the views.
        assertThat(getRadioButtonViewAt(model, 0).isChecked()).isFalse();
        assertThat(getRadioButtonViewAt(model, 1).isChecked()).isFalse();
        assertThat(getRadioButtonViewAt(model, 2).isChecked()).isTrue();
        assertThat(getRadioButtonViewAt(model, 3).isChecked()).isFalse();

        // Trigger positive button click.
        mModalDialogManager.clickPositiveButton();

        // Verify that the correct identifiers are passed to the listener.
        verify(mAuthenticatorSelectedListener, times(1)).onOptionSelected(OPTION_3.getIdentifier());
    }

    @Test
    @SmallTest
    public void testMultipleAuthenticatorOption_changeToEmailOtpChallengeOption() throws Exception {
        PropertyModel model = createAndShowModelForChangeSelectedOptionTest();

        // Perform click for the radio button of authenticator option 4.
        getRadioButtonViewAt(model, 3).performClick();
        forceAuthenticatorOptionsViewLayout(model);
        assertThat(model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT)).isEqualTo("Send");

        // Verify that the radio button's checked state reflects correctly. Note: we need to fetch
        // the viewHolder again as the performClick triggered a redraw of the views.
        assertThat(getRadioButtonViewAt(model, 0).isChecked()).isFalse();
        assertThat(getRadioButtonViewAt(model, 1).isChecked()).isFalse();
        assertThat(getRadioButtonViewAt(model, 2).isChecked()).isFalse();
        assertThat(getRadioButtonViewAt(model, 3).isChecked()).isTrue();

        // Trigger positive button click.
        mModalDialogManager.clickPositiveButton();

        // Verify that the correct identifiers are passed to the listener.
        verify(mAuthenticatorSelectedListener, times(1)).onOptionSelected(OPTION_4.getIdentifier());
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_MOVING_GPAY_LOGO_TO_THE_RIGHT_ON_CLANK})
    public void testSingleAuthenticatorOption_defaultTitleView() throws Exception {
        ArrayList<AuthenticatorOption> options = new ArrayList<>();
        options.add(OPTION_1);

        mAuthenticatorSelectionDialog.show(options);

        PropertyModel model = mModalDialogManager.getShownDialogModel();
        assertThat(model).isNotNull();

        forceAuthenticatorOptionsViewLayout(model);

        // Verify that the title set by modal dialog is correct.
        assertThat(model.get(ModalDialogProperties.TITLE))
                .isEqualTo(mResources.getString(R.string.autofill_card_unmask_verification_title));

        // Verify that the title icon set by modal dialog is correct.
        Drawable expectedDrawable =
                ResourcesCompat.getDrawable(
                        mResources,
                        R.drawable.google_pay_with_divider,
                        ApplicationProvider.getApplicationContext().getTheme());
        assertTrue(
                getBitmap(expectedDrawable)
                        .sameAs(getBitmap(model.get(ModalDialogProperties.TITLE_ICON))));

        // Verify that title and title icon is not set by custom view.
        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        assertThat((TextView) customView.findViewById(R.id.title)).isNull();
        assertThat((ImageView) customView.findViewById(R.id.title_icon)).isNull();
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_MOVING_GPAY_LOGO_TO_THE_RIGHT_ON_CLANK})
    public void testMultipleAuthenticatorOption_defaultTitleView() throws Exception {
        ArrayList<AuthenticatorOption> options = new ArrayList<>();
        options.add(OPTION_1);
        options.add(OPTION_2);

        mAuthenticatorSelectionDialog.show(options);

        PropertyModel model = mModalDialogManager.getShownDialogModel();
        assertThat(model).isNotNull();

        forceAuthenticatorOptionsViewLayout(model);

        // Verify that the title set by modal dialog is correct.
        assertThat(model.get(ModalDialogProperties.TITLE))
                .isEqualTo(
                        mResources.getString(
                                R.string
                                        .autofill_card_auth_selection_dialog_title_multiple_options));

        // Verify that the title icon set by modal dialog is correct.
        Drawable expectedDrawable =
                ResourcesCompat.getDrawable(
                        mResources,
                        R.drawable.google_pay_with_divider,
                        ApplicationProvider.getApplicationContext().getTheme());
        assertTrue(
                getBitmap(expectedDrawable)
                        .sameAs(getBitmap(model.get(ModalDialogProperties.TITLE_ICON))));

        // Verify that title and title icon is not set by custom view.
        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        assertThat((TextView) customView.findViewById(R.id.title)).isNull();
        assertThat((ImageView) customView.findViewById(R.id.title_icon)).isNull();
    }

    @Test
    @SmallTest
    public void testSingleAuthenticatorOption_customTitleView() throws Exception {
        ArrayList<AuthenticatorOption> options = new ArrayList<>();
        options.add(OPTION_1);

        mAuthenticatorSelectionDialog.show(options);

        PropertyModel model = mModalDialogManager.getShownDialogModel();
        assertThat(model).isNotNull();

        forceAuthenticatorOptionsViewLayout(model);

        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);

        // Verify that the title set by custom view is correct.
        TextView title = (TextView) customView.findViewById(R.id.title);
        assertThat(title.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(title.getText())
                .isEqualTo(mResources.getString(R.string.autofill_card_unmask_verification_title));

        // Verify that the title icon set by custom view is correct.
        ImageView title_icon = (ImageView) customView.findViewById(R.id.title_icon);
        Drawable expectedDrawable =
                ResourcesCompat.getDrawable(
                        mResources,
                        R.drawable.google_pay,
                        ApplicationProvider.getApplicationContext().getTheme());
        assertThat(title_icon.getVisibility()).isEqualTo(View.VISIBLE);
        assertTrue(getBitmap(expectedDrawable).sameAs(getBitmap(title_icon.getDrawable())));

        // Verify that title and title icon is not set by modal dialog.
        assertThat(model.get(ModalDialogProperties.TITLE)).isNull();
        assertThat(model.get(ModalDialogProperties.TITLE_ICON)).isNull();
    }

    @Test
    @SmallTest
    public void testMultipleAuthenticatorOption_customTitleView() throws Exception {
        ArrayList<AuthenticatorOption> options = new ArrayList<>();
        options.add(OPTION_1);
        options.add(OPTION_2);

        mAuthenticatorSelectionDialog.show(options);

        PropertyModel model = mModalDialogManager.getShownDialogModel();
        assertThat(model).isNotNull();

        forceAuthenticatorOptionsViewLayout(model);

        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);

        // Verify that the title set by custom view is correct.
        TextView title = (TextView) customView.findViewById(R.id.title);
        assertThat(title.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(title.getText())
                .isEqualTo(
                        mResources.getString(
                                R.string
                                        .autofill_card_auth_selection_dialog_title_multiple_options));

        // Verify that the title icon set by custom view is correct.
        ImageView title_icon = (ImageView) customView.findViewById(R.id.title_icon);
        Drawable expectedDrawable =
                ResourcesCompat.getDrawable(
                        mResources,
                        R.drawable.google_pay,
                        ApplicationProvider.getApplicationContext().getTheme());
        assertThat(title_icon.getVisibility()).isEqualTo(View.VISIBLE);
        assertTrue(getBitmap(expectedDrawable).sameAs(getBitmap(title_icon.getDrawable())));

        // Verify that title and title icon is not set by modal dialog.
        assertThat(model.get(ModalDialogProperties.TITLE)).isNull();
        assertThat(model.get(ModalDialogProperties.TITLE_ICON)).isNull();
    }

    private PropertyModel createAndShowModelForChangeSelectedOptionTest() {
        ArrayList<AuthenticatorOption> options = new ArrayList<>();
        options.add(OPTION_1);
        options.add(OPTION_2);
        options.add(OPTION_3);
        options.add(OPTION_4);

        mAuthenticatorSelectionDialog.show(options);
        PropertyModel model = mModalDialogManager.getShownDialogModel();
        assertThat(model).isNotNull();
        // Force the AuthenticatorOptionsView to layout once the model is created.
        forceAuthenticatorOptionsViewLayout(model);
        return model;
    }

    private AuthenticatorOptionViewHolder getAuthenticatorOptionViewHolderAtPosition(
            PropertyModel model, int position) {
        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        RecyclerView authenticatorOptionsView =
                (RecyclerView) customView.findViewById(R.id.authenticator_options_view);
        return (AuthenticatorOptionViewHolder)
                authenticatorOptionsView.findViewHolderForAdapterPosition(position);
    }

    private RadioButton getRadioButtonViewAt(PropertyModel model, int position) {
        return getAuthenticatorOptionViewHolderAtPosition(model, position).getRadioButton();
    }

    private View getAuthenticatorOptionViewAt(PropertyModel model, int position) {
        return getAuthenticatorOptionViewHolderAtPosition(model, position)
                .getAuthenticatorOptionView();
    }

    private void forceAuthenticatorOptionsViewLayout(PropertyModel model) {
        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        RecyclerView authenticatorOptionsView =
                (RecyclerView) customView.findViewById(R.id.authenticator_options_view);
        authenticatorOptionsView.measure(0, 0);
        authenticatorOptionsView.layout(0, 0, 100, 1000);
    }

    // Convert a drawable to a Bitmap for comparison.
    private static Bitmap getBitmap(Drawable drawable) {
        Bitmap bitmap =
                Bitmap.createBitmap(
                        drawable.getIntrinsicWidth(),
                        drawable.getIntrinsicHeight(),
                        Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        drawable.draw(canvas);
        return bitmap;
    }
}
