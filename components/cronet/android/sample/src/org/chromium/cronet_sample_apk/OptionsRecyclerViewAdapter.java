// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_sample_apk;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Switch;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import java.util.List;

public class OptionsRecyclerViewAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
    private final List<Options.Option> mOptions;

    public OptionsRecyclerViewAdapter() {
        this.mOptions = Options.getOptions();
    }

    private static class ToggleOptionViewHolder extends RecyclerView.ViewHolder {
        private final TextView mOptionShortName;
        private final TextView mOptionDescription;
        private final Switch mOptionSwitch;

        public ToggleOptionViewHolder(@NonNull View itemView) {
            super(itemView);

            mOptionShortName = itemView.findViewById(R.id.option_short_name);
            mOptionDescription = itemView.findViewById(R.id.option_description);
            mOptionSwitch = itemView.findViewById(R.id.option_switch);
        }

        public void setOptionShortName(String shortName) {
            mOptionShortName.setText(shortName);
        }

        public void setOptionDescription(String optionDescriptionStr) {
            mOptionDescription.setText(optionDescriptionStr);
        }

        public void setOptionSwitch(boolean checked) {
            mOptionSwitch.setChecked(checked);
        }
    }

    @NonNull
    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        return new ToggleOptionViewHolder(
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.toggle_view, parent, false));
    }

    @Override
    public void onBindViewHolder(@NonNull RecyclerView.ViewHolder holder, int position) {
        ToggleOptionViewHolder toggleOptionViewHolder = (ToggleOptionViewHolder) holder;
        toggleOptionViewHolder.setOptionShortName(mOptions.get(position).getShortName());
        toggleOptionViewHolder.setOptionDescription(mOptions.get(position).getDescription());
        toggleOptionViewHolder.setOptionSwitch(
                ((Options.BooleanOption) mOptions.get(position)).getValue());
        toggleOptionViewHolder.mOptionSwitch.setOnClickListener(
                v ->
                        mOptions.get(position)
                                .setValue(toggleOptionViewHolder.mOptionSwitch.isChecked()));
    }

    @Override
    public int getItemCount() {
        return mOptions.size();
    }
}
