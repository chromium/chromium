// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.RelativeLayout;
import android.widget.TextView;

import com.airbnb.lottie.LottieAnimationView;

import org.chromium.base.FeatureList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/** View that wraps the fullscreen signin promo and caches references to UI elements. */
@NullMarked
public class FullscreenSigninView extends RelativeLayout {
    private ImageView mIcon;
    private LottieAnimationView mAnimationView;
    private TextView mTitle;
    private TextView mSubtitle;
    private View mBrowserManagedHeader;
    private TextView mPrivacyDisclaimer;
    private ProgressBar mInitialLoadProgressSpinner;
    private ViewGroup mSelectedAccount;
    private ImageView mExpandIcon;
    private ButtonCompat mContinueButton;
    private ButtonCompat mDismissButton;
    private TextViewWithClickableSpans mFooter;
    private ProgressBar mSigninProgressSpinner;
    private TextView mSigninProgressText;

    public FullscreenSigninView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mIcon = findViewById(R.id.fre_icon);
        mAnimationView = findViewById(R.id.signin_animation);
        mTitle = findViewById(R.id.title);
        mSubtitle = findViewById(R.id.subtitle);
        mBrowserManagedHeader = findViewById(R.id.fre_browser_managed_by);
        mInitialLoadProgressSpinner =
                findViewById(R.id.fre_native_and_policy_load_progress_spinner);
        mSelectedAccount = findViewById(R.id.signin_fre_selected_account);
        mExpandIcon = findViewById(R.id.signin_fre_selected_account_expand_icon);
        mContinueButton = findViewById(R.id.signin_fre_continue_button);
        mDismissButton = findViewById(R.id.signin_fre_dismiss_button);
        mFooter = findViewById(R.id.signin_fre_footer);
        mSigninProgressSpinner = findViewById(R.id.fre_signin_progress_spinner);
        mSigninProgressText = findViewById(R.id.fre_signin_progress_text);
        if (FeatureList.isNativeInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.XPLAT_SYNCED_SETUP)) {
            mSigninProgressText.setText(R.string.fre_signing_in_2);
        }
        mPrivacyDisclaimer = findViewById(R.id.privacy_disclaimer);
    }

    View getBrowserManagedHeaderView() {
        return mBrowserManagedHeader;
    }

    TextView getPrivacyDisclaimer() {
        return mPrivacyDisclaimer;
    }

    ProgressBar getInitialLoadProgressSpinnerView() {
        return mInitialLoadProgressSpinner;
    }

    ViewGroup getSelectedAccountView() {
        return mSelectedAccount;
    }

    ImageView getExpandIconView() {
        return mExpandIcon;
    }

    ButtonCompat getContinueButtonView() {
        return mContinueButton;
    }

    ButtonCompat getDismissButtonView() {
        return mDismissButton;
    }

    TextViewWithClickableSpans getFooterView() {
        return mFooter;
    }

    ProgressBar getSigninProgressSpinner() {
        return mSigninProgressSpinner;
    }

    TextView getSigninProgressText() {
        return mSigninProgressText;
    }

    TextView getSubtitle() {
        return mSubtitle;
    }

    TextView getTitle() {
        return mTitle;
    }

    ImageView getIcon() {
        return mIcon;
    }

    /**
     * @return The {@link LottieAnimationView} that is shown when the user proceeds with sign-in.
     */
    LottieAnimationView getAnimationView() {
        return mAnimationView;
    }
}
