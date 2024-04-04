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

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/** View that wraps the fullscreen signin promo and caches references to UI elements. **/
public class FullscreenSigninView extends RelativeLayout {
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
        mPrivacyDisclaimer = (TextView) findViewById(R.id.privacy_disclaimer);
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
}
