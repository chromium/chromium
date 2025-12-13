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
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.components.signin.test.util.FakeIdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.SyncService;

/** Unit tests for {@link SigninSnackbarController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SigninSnackbarControllerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private static final SigninAndHistorySyncCoordinator.Result SIGN_IN_AND_HISTORY_SYNC =
            new SigninAndHistorySyncCoordinator.Result(true, true);
    private static final SigninAndHistorySyncCoordinator.Result SIGN_IN_ONLY =
            new SigninAndHistorySyncCoordinator.Result(true, false);
    private static final SigninAndHistorySyncCoordinator.Result HISTORY_SYNC_ONLY =
            new SigninAndHistorySyncCoordinator.Result(false, true);
    private static final SigninAndHistorySyncCoordinator.Result ABORTED =
            SigninAndHistorySyncCoordinator.Result.aborted();

    @Mock private Profile mProfile;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private SigninSnackbarController.Listener mListener;
    @Mock private SyncService mSyncService;
    @Mock private HistorySyncHelper mHistorySyncHelper;
    @Mock private SigninManager mSigninManager;

    private final FakeIdentityManager mIdentityManager = new FakeIdentityManager();
    private ComponentActivity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(ComponentActivity.class).setup().get();
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
        mIdentityManager.setPrimaryAccount(TestAccounts.ACCOUNT1);
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
    public void testShowUndoSnackbarIfNeeded_resultSignInOnly_snackbarShown() {
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);

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

    @Test
    public void testOnUndoClick_resultSignInOnly_doesNotRevertHistorySync() {
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mIdentityServicesProvider.getSigninManager(mProfile)).thenReturn(mSigninManager);

        SigninSnackbarController.showUndoSnackbarIfNeeded(
                mActivity, mProfile, mSnackbarManager, mListener, SIGN_IN_ONLY);
        clickSnackbarUndoButton();
        verifyNoInteractions(mHistorySyncHelper);
    }

    @Test
    public void testOnUndoClick_resultSignInAndHistorySync_revertHistorySync() {
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mIdentityServicesProvider.getSigninManager(mProfile)).thenReturn(mSigninManager);

        SigninSnackbarController.showUndoSnackbarIfNeeded(
                mActivity, mProfile, mSnackbarManager, mListener, SIGN_IN_AND_HISTORY_SYNC);
        clickSnackbarUndoButton();
        verify(mHistorySyncHelper).setHistoryAndTabsSync(false);
    }

    private void clickSnackbarUndoButton() {
        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());
        Snackbar snackbar = snackbarCaptor.getValue();
        snackbar.getController().onAction(snackbar.getActionData());
    }
}
