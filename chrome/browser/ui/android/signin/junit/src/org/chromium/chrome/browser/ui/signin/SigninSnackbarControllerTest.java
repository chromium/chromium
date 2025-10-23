// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.containsString;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import androidx.activity.ComponentActivity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;

/** Unit tests for {@link SigninSnackbarController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SigninSnackbarControllerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private static final SigninAndHistorySyncCoordinator.Result SIGN_IN_ONLY =
            new SigninAndHistorySyncCoordinator.Result(true, false);
    private static final SigninAndHistorySyncCoordinator.Result HISTORY_SYNC_ONLY =
            new SigninAndHistorySyncCoordinator.Result(false, true);
    private static final SigninAndHistorySyncCoordinator.Result ABORTED =
            SigninAndHistorySyncCoordinator.Result.aborted();

    @Mock private Profile mProfile;
    @Mock private IdentityManager mIdentityManager;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private SigninSnackbarController.Listener mListener;

    private ComponentActivity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(ComponentActivity.class).setup().get();
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
    }

    @Test
    public void testShowUndoSnackbarIfNeeded_resultAborted_snackbarNotShown() {
        SigninSnackbarController.showUndoSnackbarIfNeeded(
                mActivity, mProfile, mSnackbarManager, mListener, ABORTED);
        verifyNoInteractions(mSnackbarManager);
    }

    @Test
    public void testShowUndoSnackbarIfNeeded_resultHistorySyncOnly_snackbarNotShown() {
        SigninSnackbarController.showUndoSnackbarIfNeeded(
                mActivity, mProfile, mSnackbarManager, mListener, HISTORY_SYNC_ONLY);
        verifyNoInteractions(mSnackbarManager);
    }

    @Test
    public void testShowUndoSnackbarIfNeeded_resultSignedInOnly_snackbarShown() {
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(TestAccounts.ACCOUNT1);

        SigninSnackbarController.showUndoSnackbarIfNeeded(
                mActivity, mProfile, mSnackbarManager, mListener, SIGN_IN_ONLY);

        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());
        Snackbar snackbar = snackbarCaptor.getValue();
        assertThat(
                "Snackbar text should contain the user's email.",
                snackbar.getTextForTesting().toString(),
                containsString(TestAccounts.ACCOUNT1.getEmail()));
    }
}
