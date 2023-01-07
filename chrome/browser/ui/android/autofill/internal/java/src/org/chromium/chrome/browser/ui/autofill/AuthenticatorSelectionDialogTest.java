// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.autofill.AuthenticatorOptionsAdapter.AuthenticatorOptionViewHolder;
import org.chromium.chrome.browser.ui.autofill.data.AuthenticatorOption;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

import java.util.ArrayList;

/**
 * Unit tests for {@link AuthenticatorSelectionDialog}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AuthenticatorSelectionDialogTest {
    // The icon set on the AuthenticatorOption is not important and any icon would do.
    private static final AuthenticatorOption OPTION_1 =
            new AuthenticatorOption.Builder()
                    .setTitle("title1")
                    .setIdentifier("identifier1")
                    .setDescription("description1")
                    .setIconResId(android.R.drawable.ic_media_pause)
                    .build();

    private static final AuthenticatorOption OPTION_2 =
            new AuthenticatorOption.Builder()
                    .setTitle("title2")
                    .setIdentifier("identifier2")
                    .setDescription("description2")
                    .setIconResId(android.R.drawable.ic_media_play)
                    .build();

    private FakeModalDialogManager mModalDialogManager;
    private AuthenticatorSelectionDialog mAuthenticatorSelectionDialog;
    @Mock
    private AuthenticatorSelectionDialog.Listener mAuthenticatorSelectedListener;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.TAB);
        mAuthenticatorSelectionDialog =
                new AuthenticatorSelectionDialog(ApplicationProvider.getApplicationContext(),
                        mAuthenticatorSelectedListener, mModalDialogManager);
    }

    @Test
    @SmallTest
    public void testSingleAuthenticatorOption() throws Exception {
        ArrayList<AuthenticatorOption> options = new ArrayList<>();
        options.add(OPTION_1);

        mAuthenticatorSelectionDialog.show(options);

        PropertyModel model = mModalDialogManager.getShownDialogModel();
        assertThat(model).isNotNull();
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
    public void testMultipleAuthenticatorOption_changeSelectedOptionBeforeProceeding()
            throws Exception {
        ArrayList<AuthenticatorOption> options = new ArrayList<>();
        options.add(OPTION_1);
        options.add(OPTION_2);

        mAuthenticatorSelectionDialog.show(options);

        PropertyModel model = mModalDialogManager.getShownDialogModel();
        assertThat(model).isNotNull();
        AuthenticatorOptionViewHolder viewHolder1 =
                getAuthenticatorOptionViewHolderAtPosition(model, 0);
        AuthenticatorOptionViewHolder viewHolder2 =
                getAuthenticatorOptionViewHolderAtPosition(model, 1);
        // Verify that the first radio button is selected by default.
        assertThat(viewHolder1.getRadioButton().isChecked()).isTrue();
        assertThat(viewHolder2.getRadioButton().isChecked()).isFalse();

        // Perform click for the radio button of authenticator option 2.
        viewHolder2.getRadioButton().performClick();

        // Verify that the radio button's checked state reflects correctly. Note: we need to fetch
        // the viewHolder again as the performClick triggered a redraw of the views.
        assertThat(
                getAuthenticatorOptionViewHolderAtPosition(model, 0).getRadioButton().isChecked())
                .isFalse();
        assertThat(
                getAuthenticatorOptionViewHolderAtPosition(model, 1).getRadioButton().isChecked())
                .isTrue();

        // Trigger positive button click.
        mModalDialogManager.clickPositiveButton();

        // Verify that the correct identifier is passed to the listener.
        verify(mAuthenticatorSelectedListener, times(1)).onOptionSelected(OPTION_2.getIdentifier());
    }

    private AuthenticatorOptionViewHolder getAuthenticatorOptionViewHolderAtPosition(
            PropertyModel model, int position) {
        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        RecyclerView authenticatorOptionsView =
                (RecyclerView) customView.findViewById(R.id.authenticator_options_view);
        // Trigger the recycler view layout to ensure that findViewHolderForAdapterPosition does not
        // return null.
        authenticatorOptionsView.measure(0, 0);
        authenticatorOptionsView.layout(0, 0, 100, 1000);
        return (AuthenticatorOptionViewHolder)
                authenticatorOptionsView.findViewHolderForAdapterPosition(position);
    }
}
