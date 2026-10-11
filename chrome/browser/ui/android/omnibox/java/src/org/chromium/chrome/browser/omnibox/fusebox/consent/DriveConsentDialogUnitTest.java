// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox.consent;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Intent;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.LoadingView;
import org.chromium.url.GURL;

/** Unit tests for {@link DriveConsentDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DriveConsentDialogUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Callback<Boolean> mOnConsentComplete;
    @Mock private Callback<Boolean> mShowConsentComplete;
    @Mock private Profile mProfile;
    @Mock private LoadingView.Observer mSpinnerObserver;

    private Activity mActivity;
    private LoadingView mSpinner;
    private View mLoadingOverlay;
    private DriveConsentDialog mDialog;
    private WindowAndroid mWindowAndroid;
    private PropertyModel mModalDialogModel;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mSpinner = new LoadingView(mActivity);
        mSpinner.showLoadingUi();
        mSpinner.addObserver(mSpinnerObserver);
        mLoadingOverlay = new View(mActivity);
        mModalDialogModel = new PropertyModel(ModalDialogProperties.ALL_KEYS);
        mDialog = new DriveConsentDialog(mActivity, mModalDialogManager, mOnConsentComplete);
        mDialog.setModalDialogModelForTesting(mModalDialogModel);
    }

    @After
    public void tearDown() {
        mDialog.destroy();
        if (mWindowAndroid != null) {
            mWindowAndroid.destroy();
        }
    }

    @Test
    public void show_finishingActivity_failsSynchronously() {
        mWindowAndroid =
                new ActivityWindowAndroid(
                        mActivity,
                        /* listenToActivityState= */ false,
                        IntentRequestTracker.createFromActivity(mActivity),
                        /* insetObserver= */ null,
                        /* occlusionTrackingAllowed= */ false);
        mActivity.finish();

        assertNull(DriveConsentDialog.show(mWindowAndroid, mProfile, mShowConsentComplete));
        verify(mShowConsentComplete).onResult(false);
    }

    @Test
    public void onConsentComplete_granted_notifiesCallbackAndDismisses() {
        mDialog.onConsentComplete(true);

        verify(mOnConsentComplete).onResult(true);
        verify(mModalDialogManager)
                .dismissDialog(mModalDialogModel, DialogDismissalCause.ACTION_ON_CONTENT);
        assertTrue(mDialog.isDestroyedForTesting());
    }

    @Test
    public void onDismiss_notifiesCallbackAndDismisses() {
        mDialog.onDismiss(mModalDialogModel, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

        verify(mOnConsentComplete).onResult(false);
        verify(mModalDialogManager, never())
                .dismissDialog(mModalDialogModel, DialogDismissalCause.ACTION_ON_CONTENT);
        assertTrue(mDialog.isDestroyedForTesting());
    }

    @Test
    public void onConsentComplete_reportedOnlyOnce() {
        mDialog.onConsentComplete(true);
        mDialog.onDismiss(mModalDialogModel, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

        verify(mOnConsentComplete).onResult(true);
        verify(mOnConsentComplete, never()).onResult(false);
    }

    @Test
    public void onPageFirstPaint_hidesLoadingUi() {
        mDialog.setSpinnerForTesting(mSpinner);
        mDialog.setLoadingOverlayForTesting(mLoadingOverlay);

        mDialog.onPageFirstPaint();

        assertEquals(View.GONE, mLoadingOverlay.getVisibility());
        verify(mSpinnerObserver).onHideLoadingUiComplete();
    }

    @Test
    public void destroy_cleansUpAndDismissesDialog() {
        mDialog.setSpinnerForTesting(mSpinner);
        mDialog.setLoadingOverlayForTesting(mLoadingOverlay);

        mDialog.destroy();
        mDialog.destroy();

        verify(mOnConsentComplete).onResult(false);
        verify(mModalDialogManager)
                .dismissDialog(mModalDialogModel, DialogDismissalCause.ACTION_ON_CONTENT);
        assertTrue(mSpinner.isObserverListEmpty());
    }

    @Test
    public void shouldCreateWebContents_returnsFalseAndLaunchesCustomTabIntent() {
        boolean result =
                mDialog.getWebContentsDelegateForTesting()
                        .shouldCreateWebContents(new GURL("https://policies.google.com/terms"));

        assertFalse(result);
        Intent intent = Shadows.shadowOf(mActivity).getNextStartedActivity();
        assertEquals("https://policies.google.com/terms", intent.getDataString());
    }

    @Test
    public void onConsentComplete_whileShown_coversWebContentsThenDismisses() {
        mActivity.setContentView(mLoadingOverlay);
        ShadowLooper.idleMainLooper();
        mLoadingOverlay.setVisibility(View.GONE);
        mDialog.setLoadingOverlayForTesting(mLoadingOverlay);

        mDialog.onConsentComplete(true);
        assertEquals(View.VISIBLE, mLoadingOverlay.getVisibility());
        verify(mModalDialogManager, never()).dismissDialog(any(), anyInt());

        ShadowLooper.idleMainLooper();
        verify(mModalDialogManager)
                .dismissDialog(mModalDialogModel, DialogDismissalCause.ACTION_ON_CONTENT);
        verify(mOnConsentComplete).onResult(true);
    }
}
