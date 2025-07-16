// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.provider.Browser;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.style.ForegroundColorSpan;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.widget.ChromeImageView;

/**
 * View that displays connection security information, including whether or not the connection is
 * secure, and a button to view the TLS certificate (if one exists).
 */
@NullMarked
public class ConnectionSecurityView extends FrameLayout implements OnClickListener {
    private static final String TAG = "ConnectionSecurity";
    private static final String HELP_URL =
            "https://support.google.com/chrome?p=android_connection_info";

    /** Parameters to configure the row view. */
    public static class ViewParams {
        public @DrawableRes int iconResId;
        public @ColorRes int iconTint;
        public @Nullable CharSequence summary;
        public @Nullable CharSequence details;
        public @Nullable Runnable resetDecisionsCallback;
        public byte @Nullable [][] certChain;
        public boolean isCert1Qwac;
        public byte @Nullable [][] twoQwacCertChain;
        public @Nullable CharSequence qwacIdentity;
    }

    private final Context mContext;
    private final ChromeImageView mIcon;
    private final TextView mSummary;
    private final TextView mDetails;
    private final TextView mResetDecision;
    private final ViewGroup mQwacCertInfo;
    private final ViewGroup mCertDetailsButton;
    private final CertificateViewer mCertificateViewer;
    private byte @Nullable [][] mCertChain;
    private byte @Nullable [][] mTwoQwacCertChain;

    public ConnectionSecurityView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        LayoutInflater.from(context).inflate(R.layout.connection_security, this, true);
        mContext = context;
        mIcon = findViewById(R.id.identity_status_icon);
        mSummary = findViewById(R.id.security_description_summary);
        mDetails = findViewById(R.id.security_description_details);
        mResetDecision = findViewById(R.id.security_description_reset_decision);
        mQwacCertInfo = findViewById(R.id.qwac_cert_info);
        mCertDetailsButton = findViewById(R.id.certificate_details_button);
        setVisibility(GONE);

        mCertificateViewer = new CertificateViewer(context);
    }

    public void setParams(ViewParams params) {
        boolean visible = params.summary != null || params.details != null;
        setVisibility(visible ? VISIBLE : GONE);
        if (!visible) return;

        DisplayMetrics displayMetrics = mContext.getResources().getDisplayMetrics();
        mIcon.setImageResource(params.iconResId);

        ImageViewCompat.setImageTintList(
                mIcon,
                params.iconTint != 0
                        ? ColorStateList.valueOf(mContext.getColor(params.iconTint))
                        : AppCompatResources.getColorStateList(
                                mContext, R.color.default_icon_color_tint_list));

        mSummary.setText(params.summary);
        mSummary.setVisibility(params.summary != null ? VISIBLE : GONE);
        if (params.summary != null && params.details != null) {
            mSummary.setPadding(0, 0, 0, ViewUtils.dpToPx(displayMetrics, 4));
        }

        if (params.details != null) {
            mDetails.setText(
                    buildTextWithLink(params.details, mContext.getString(R.string.learn_more)));
        }
        mDetails.setVisibility(params.details != null ? VISIBLE : GONE);
        mDetails.setOnClickListener(this);

        if (params.resetDecisionsCallback != null) {
            Runnable resetCallback = params.resetDecisionsCallback;
            CharSequence description =
                    mContext.getString(R.string.page_info_invalid_certificate_description);
            CharSequence linkText =
                    mContext.getString(
                            R.string.page_info_reset_invalid_certificate_decisions_button);
            mResetDecision.setText(buildTextWithLink(description, linkText));
            mResetDecision.setVisibility(VISIBLE);
            mResetDecision.setOnClickListener(v -> resetCallback.run());
        }

        mCertChain = params.certChain;
        if (mCertChain == null) {
            mCertDetailsButton.setVisibility(GONE);
        } else {
            mCertDetailsButton.setOnClickListener(this);
        }

        mTwoQwacCertChain = params.twoQwacCertChain;
        if (params.isCert1Qwac || mTwoQwacCertChain != null) {
            mQwacCertInfo.setVisibility(VISIBLE);
            TextView subtitle = findViewById(R.id.qwac_subtitle);
            if (params.qwacIdentity != null) {
                subtitle.setText(params.qwacIdentity);
            } else {
                subtitle.setVisibility(GONE);
            }
            if (mTwoQwacCertChain == null) {
                findViewById(R.id.qwac_cert_details_icon).setVisibility(GONE);
            } else {
                mQwacCertInfo.setOnClickListener(this);
            }
        }
    }

    private SpannableStringBuilder buildTextWithLink(CharSequence text, CharSequence linkText) {
        SpannableStringBuilder builder = new SpannableStringBuilder();
        builder.append(text);
        builder.append(" ");
        SpannableString link = new SpannableString(linkText);
        final ForegroundColorSpan blueSpan =
                new ForegroundColorSpan(SemanticColorUtils.getDefaultTextColorLink(mContext));
        link.setSpan(blueSpan, 0, link.length(), Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
        builder.append(link);
        return builder;
    }

    @Override
    public void onClick(View v) {
        if (v == mCertDetailsButton && mCertChain != null) {
            mCertificateViewer.showCertificateChain(mCertChain);
        } else if (v == mDetails) {
            showConnectionSecurityInfo();
        } else if (v == mQwacCertInfo && mTwoQwacCertChain != null) {
            mCertificateViewer.showCertificateChain(mTwoQwacCertChain);
        }
    }

    private void showConnectionSecurityInfo() {
        try {
            Intent i = Intent.parseUri(HELP_URL, Intent.URI_INTENT_SCHEME);
            i.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
            i.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());
            mContext.startActivity(i);
        } catch (Exception ex) {
            // Do nothing intentionally.
            Log.w(TAG, "Bad URI %s", HELP_URL, ex);
        }
    }
}
