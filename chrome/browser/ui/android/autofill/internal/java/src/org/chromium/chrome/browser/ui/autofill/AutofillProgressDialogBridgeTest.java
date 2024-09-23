// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.content.res.ResourcesCompat;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/** Unit tests for {@link AutofillProgressDialogBridge} */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_MOVING_GPAY_LOGO_TO_THE_RIGHT_ON_CLANK})
public class AutofillProgressDialogBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mMocker = new JniMocker();

    private static final String PROGRESS_DIALOG_TITLE = "Verify your card";
    private static final String PROGRESS_DIALOG_MESSAGE = "Contacting your bank...";
    private static final String PROGRESS_DIALOG_CONFIRMATION = "Your card is confirmed";
    private static final String PROGRESS_DIALOG_BUTTON_LABEL = "Cancel";
    private static final long NATIVE_AUTOFILL_PROGRESS_DIALOG_VIEW = 100L;

    @Mock private AutofillProgressDialogBridge.Natives mNativeMock;

    private AutofillProgressDialogBridge mAutofillProgressDialogBridge;
    private FakeModalDialogManager mModalDialogManager;
    private Resources mResources;

    private void showProgressDialog(int titleIconId) {
        mAutofillProgressDialogBridge.showDialog(
                PROGRESS_DIALOG_TITLE,
                PROGRESS_DIALOG_MESSAGE,
                PROGRESS_DIALOG_BUTTON_LABEL,
                /* iconId= */ titleIconId);
    }

    @Before
    public void setUp() {
        reset(mNativeMock);
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.TAB);
        mResources = ApplicationProvider.getApplicationContext().getResources();
        mAutofillProgressDialogBridge =
                new AutofillProgressDialogBridge(
                        NATIVE_AUTOFILL_PROGRESS_DIALOG_VIEW,
                        mModalDialogManager,
                        ApplicationProvider.getApplicationContext());
        mMocker.mock(AutofillProgressDialogBridgeJni.TEST_HOOKS, mNativeMock);
    }

    @Test
    @SmallTest
    public void testBasic() throws Exception {
        showProgressDialog(/* titleIconId= */ 0);
        Assert.assertNotNull(mModalDialogManager.getShownDialogModel());

        mAutofillProgressDialogBridge.dismiss();
        // Verify that no dialog is shown and that the callback is triggered on dismissal.
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
        verify(mNativeMock, times(1)).onDismissed(NATIVE_AUTOFILL_PROGRESS_DIALOG_VIEW);
    }

    @Test
    public void testSuccessful() throws Exception {
        showProgressDialog(/* titleIconId= */ 0);
        Assert.assertNotNull(mModalDialogManager.getShownDialogModel());
        View dialogView =
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.CUSTOM_VIEW);
        TextView messageView = dialogView.findViewById(R.id.message);
        View progressBar = dialogView.findViewById(R.id.progress_bar);
        View confirmationIcon = dialogView.findViewById(R.id.confirmation_icon);

        Assert.assertEquals(PROGRESS_DIALOG_MESSAGE, messageView.getText());
        Assert.assertEquals(View.VISIBLE, progressBar.getVisibility());
        Assert.assertEquals(View.GONE, confirmationIcon.getVisibility());

        // Verify that the dialog is still shown.
        mAutofillProgressDialogBridge.showConfirmation(PROGRESS_DIALOG_CONFIRMATION);
        Assert.assertNotNull(mModalDialogManager.getShownDialogModel());

        Assert.assertEquals(PROGRESS_DIALOG_CONFIRMATION, messageView.getText());
        Assert.assertEquals(View.GONE, progressBar.getVisibility());
        Assert.assertEquals(View.VISIBLE, confirmationIcon.getVisibility());

        mAutofillProgressDialogBridge.dismiss();
        // Verify that no dialog is shown and that the callback is triggered on dismissal.
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
        verify(mNativeMock, times(1)).onDismissed(NATIVE_AUTOFILL_PROGRESS_DIALOG_VIEW);
    }

    @Test
    @SmallTest
    public void testDismissedCalledOnButtonClick() throws Exception {
        showProgressDialog(/* titleIconId= */ 0);

        mModalDialogManager.clickNegativeButton();

        verify(mNativeMock, times(1)).onDismissed(NATIVE_AUTOFILL_PROGRESS_DIALOG_VIEW);
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_MOVING_GPAY_LOGO_TO_THE_RIGHT_ON_CLANK})
    public void testDefaultTitleView() throws Exception {
        int titleIconId = R.drawable.google_pay_with_divider;
        showProgressDialog(/* titleIconId= */ titleIconId);

        PropertyModel model = mModalDialogManager.getShownDialogModel();
        Assert.assertNotNull(model);

        // Verify that the title set by modal dialog is correct.
        assertThat(model.get(ModalDialogProperties.TITLE)).isEqualTo(PROGRESS_DIALOG_TITLE);

        // Verify that the title icon set by modal dialog is correct.
        Drawable expectedDrawable =
                ResourcesCompat.getDrawable(
                        mResources,
                        titleIconId,
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
    public void testCustomTitleView() throws Exception {
        int titleIconId = R.drawable.google_pay;
        showProgressDialog(/* titleIconId= */ titleIconId);

        PropertyModel model = mModalDialogManager.getShownDialogModel();
        Assert.assertNotNull(model);

        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);

        // Verify that the title set by custom view is correct.
        TextView title = (TextView) customView.findViewById(R.id.title);
        assertThat(title.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(title.getText()).isEqualTo(PROGRESS_DIALOG_TITLE);

        // Verify that the title icon set by custom view is correct.
        ImageView title_icon = (ImageView) customView.findViewById(R.id.title_icon);
        Drawable expectedDrawable =
                ResourcesCompat.getDrawable(
                        mResources,
                        titleIconId,
                        ApplicationProvider.getApplicationContext().getTheme());
        assertThat(title_icon.getVisibility()).isEqualTo(View.VISIBLE);
        assertTrue(getBitmap(expectedDrawable).sameAs(getBitmap(title_icon.getDrawable())));

        // Verify that title and title icon is not set by modal dialog.
        assertThat(model.get(ModalDialogProperties.TITLE)).isNull();
        assertThat(model.get(ModalDialogProperties.TITLE_ICON)).isNull();
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
