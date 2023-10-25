// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.widget.Button;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.ContentPriority;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PlusAddressCreationViewBridgeTest {
    private static final long NATIVE_PLUS_ADDRESS_CREATION_VIEW = 100L;
    private static final String MODAL_TITLE = "lorem ipsum title";
    private static final String MODAL_PLUS_ADDRESS_DESCRIPTION =
            "lorem ipsum description <link>test link</link> <b>test bold</b>";
    private static final String MODAL_FORMATTED_PLUS_ADDRESS_DESCRIPTION =
            "lorem ipsum description test link test bold";
    private static final String MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER = "plus+1@plus.plus";
    private static final String MODAL_OK = "ok";
    private static final String MODAL_CANCEL = "cancel";

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private PlusAddressCreationViewBridge.Natives mPromptDelegateJni;

    @Mock private ManagedBottomSheetController mBottomSheetController;

    private WindowAndroid mWindow;
    private PlusAddressCreationViewBridge mPlusAddressCreationViewBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Activity activity = Robolectric.setupActivity(TestActivity.class);
        mWindow = new WindowAndroid(activity);
        BottomSheetControllerFactory.attach(mWindow, mBottomSheetController);
        mPlusAddressCreationViewBridge =
                PlusAddressCreationViewBridge.create(NATIVE_PLUS_ADDRESS_CREATION_VIEW, mWindow);
        mPlusAddressCreationViewBridge.setActivityForTesting(activity);
        mJniMocker.mock(PlusAddressCreationViewBridgeJni.TEST_HOOKS, mPromptDelegateJni);
    }

    @After
    public void tearDown() {
        BottomSheetControllerFactory.detach(mBottomSheetController);
        mWindow.destroy();
    }

    private void showBottomSheet() {
        mPlusAddressCreationViewBridge.show(
                MODAL_TITLE,
                MODAL_PLUS_ADDRESS_DESCRIPTION,
                MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER,
                MODAL_OK,
                MODAL_CANCEL);
    }

    @Test
    @SmallTest
    public void bottomSheetShown() {
        showBottomSheet();
        PlusAddressCreationBottomSheetContent bottomSheetContent =
                mPlusAddressCreationViewBridge.getBottomSheetContent();
        TextView modalTitleView =
                bottomSheetContent.getContentView().findViewById(R.id.plus_address_notice_title);
        TextView modalDescriptionView =
                bottomSheetContent
                        .getContentView()
                        .findViewById(R.id.plus_address_modal_explanation);
        TextView modalPlusAddressPlaceholderView =
                bottomSheetContent.getContentView().findViewById(R.id.proposed_plus_address);
        Button modalOkButton =
                bottomSheetContent.getContentView().findViewById(R.id.plus_address_confirm_button);
        Button modalCancelButton =
                bottomSheetContent.getContentView().findViewById(R.id.plus_address_cancel_button);

        Assert.assertEquals(modalTitleView.getText().toString(), MODAL_TITLE);
        Assert.assertEquals(
                modalDescriptionView.getText().toString(),
                MODAL_FORMATTED_PLUS_ADDRESS_DESCRIPTION);
        Assert.assertEquals(
                modalPlusAddressPlaceholderView.getText().toString(),
                MODAL_PROPOSED_PLUS_ADDRESS_PLACEHOLDER);
        Assert.assertEquals(modalOkButton.getText().toString(), MODAL_OK);
        Assert.assertEquals(modalCancelButton.getText().toString(), MODAL_CANCEL);
    }

    @Test
    @SmallTest
    public void okButtonPressed() {
        showBottomSheet();
        PlusAddressCreationBottomSheetContent bottomSheetContent =
                mPlusAddressCreationViewBridge.getBottomSheetContent();
        Button modalOkButton =
                bottomSheetContent.getContentView().findViewById(R.id.plus_address_confirm_button);
        modalOkButton.callOnClick();

        verify(mBottomSheetController, times(1))
                .hideContent(any(), eq(true), eq(StateChangeReason.INTERACTION_COMPLETE));
        verify(mPromptDelegateJni, times(1))
                .onConfirmed(eq(NATIVE_PLUS_ADDRESS_CREATION_VIEW), any());
    }

    @Test
    @SmallTest
    public void cancelButtonPressed() {
        showBottomSheet();
        PlusAddressCreationBottomSheetContent bottomSheetContent =
                mPlusAddressCreationViewBridge.getBottomSheetContent();
        Button modalCancelButton =
                bottomSheetContent.getContentView().findViewById(R.id.plus_address_cancel_button);
        modalCancelButton.callOnClick();

        verify(mBottomSheetController, times(1))
                .hideContent(any(), eq(true), eq(StateChangeReason.INTERACTION_COMPLETE));
        verify(mPromptDelegateJni, times(1))
                .onCanceled(eq(NATIVE_PLUS_ADDRESS_CREATION_VIEW), any());
    }

    @Test
    @SmallTest
    public void bottomSheetDismissed() {
        showBottomSheet();
        mPlusAddressCreationViewBridge.onSheetClosed(StateChangeReason.SWIPE);

        verify(mPromptDelegateJni, times(1))
                .promptDismissed(eq(NATIVE_PLUS_ADDRESS_CREATION_VIEW), any());
    }

    @Test
    @SmallTest
    public void bottomSheetAttributes() {
        showBottomSheet();
        PlusAddressCreationBottomSheetContent bottomSheetContent =
                mPlusAddressCreationViewBridge.getBottomSheetContent();
        Assert.assertEquals(bottomSheetContent.getToolbarView(), null);
        Assert.assertEquals(bottomSheetContent.getPriority(), ContentPriority.HIGH);
        Assert.assertEquals(bottomSheetContent.swipeToDismissEnabled(), true);
        Assert.assertEquals(bottomSheetContent.getPeekHeight(), HeightMode.DISABLED);
        Assert.assertEquals(bottomSheetContent.getHalfHeightRatio(), HeightMode.DISABLED, 0.1);
        Assert.assertEquals(bottomSheetContent.getFullHeightRatio(), HeightMode.WRAP_CONTENT, 0.1);
    }
}
