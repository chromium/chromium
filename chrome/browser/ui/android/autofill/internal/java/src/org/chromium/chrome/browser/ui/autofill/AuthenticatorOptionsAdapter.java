// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.RadioButton;
import android.widget.TextView;

import androidx.core.content.res.ResourcesCompat;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.chrome.browser.ui.autofill.data.AuthenticatorOption;
import org.chromium.chrome.browser.ui.autofill.internal.R;

import java.util.List;

/** Adapter for showing the authenticator options in a {@link RecyclerView}. */
public class AuthenticatorOptionsAdapter extends RecyclerView.Adapter<ViewHolder> {
    /** Interface for callers to be notified when an item is selected. */
    public interface ItemClickListener {
        void onItemClicked(AuthenticatorOption option);
    }

    private final List<AuthenticatorOption> mAuthenticatorOptions;
    private final ItemClickListener mItemClickListener;
    private final Context mContext;

    private int mSelectedAuthenticatorIndex;

    public AuthenticatorOptionsAdapter(
            Context context,
            List<AuthenticatorOption> authenticatorOptions,
            ItemClickListener itemClickListener) {
        this.mAuthenticatorOptions = authenticatorOptions;
        this.mItemClickListener = itemClickListener;
        this.mContext = context;
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View view =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.authenticator_option, parent, false);
        return new AuthenticatorOptionViewHolder(view, mItemClickListener);
    }

    @Override
    public void onBindViewHolder(ViewHolder viewHolder, final int position) {
        AuthenticatorOptionViewHolder holder = (AuthenticatorOptionViewHolder) viewHolder;
        AuthenticatorOption option = mAuthenticatorOptions.get(position);
        if (getItemCount() == 1) {
            holder.getRadioButton().setVisibility(View.GONE);
            int iconResId = option.getIconResId();
            if (iconResId != 0) {
                holder.getIconImageView().setVisibility(View.VISIBLE);
                holder.getIconImageView()
                        .setImageDrawable(
                                ResourcesCompat.getDrawable(
                                        mContext.getResources(), iconResId, mContext.getTheme()));
            } else {
                holder.getIconImageView().setVisibility(View.GONE);
            }
        } else {
            holder.getIconImageView().setVisibility(View.GONE);
            holder.getRadioButton().setVisibility(View.VISIBLE);
            holder.getRadioButton().setChecked(position == mSelectedAuthenticatorIndex);
        }
        holder.getTitleTextView().setText(option.getTitle());
        holder.getDescriptionTextView().setText(option.getDescription());
    }

    @Override
    public int getItemCount() {
        return mAuthenticatorOptions.size();
    }

    class AuthenticatorOptionViewHolder extends RecyclerView.ViewHolder {
        private final View mAuthenticatorOptionView;
        private final TextView mTitleTextView;
        private final TextView mDescriptionTextView;
        private final ImageView mIconImageView;
        private final RadioButton mRadioButton;

        AuthenticatorOptionViewHolder(View view, ItemClickListener itemClickListener) {
            super(view);
            mAuthenticatorOptionView = view;
            mTitleTextView = view.findViewById(R.id.authenticator_option_title);
            mDescriptionTextView = view.findViewById(R.id.authenticator_option_description);
            mIconImageView = view.findViewById(R.id.authenticator_option_icon);
            mRadioButton = view.findViewById(R.id.authenticator_option_radio_btn);
            mRadioButton.setOnClickListener(
                    radioButtonView -> {
                        int lastAuthenticatorIndex = mSelectedAuthenticatorIndex;
                        mSelectedAuthenticatorIndex = getAdapterPosition();
                        // Update both the previous and the current selection so that the radio
                        // button is updated.
                        notifyItemChanged(lastAuthenticatorIndex);
                        notifyItemChanged(mSelectedAuthenticatorIndex);
                        itemClickListener.onItemClicked(
                                mAuthenticatorOptions.get(mSelectedAuthenticatorIndex));
                    });
            if (getItemCount() > 1) {
                view.setOnClickListener((challengeOptionView) -> mRadioButton.performClick());
            }
        }

        public View getAuthenticatorOptionView() {
            return mAuthenticatorOptionView;
        }

        public TextView getTitleTextView() {
            return mTitleTextView;
        }

        public TextView getDescriptionTextView() {
            return mDescriptionTextView;
        }

        public ImageView getIconImageView() {
            return mIconImageView;
        }

        public RadioButton getRadioButton() {
            return mRadioButton;
        }
    }
}
