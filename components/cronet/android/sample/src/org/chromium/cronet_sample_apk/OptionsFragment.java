// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_sample_apk;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

public class OptionsFragment extends Fragment {
    private RecyclerView mOptionsRecyclerView;

    private void init(View view) {
        mOptionsRecyclerView = view.findViewById(R.id.options_recycler_view);
        mOptionsRecyclerView.setAdapter(new OptionsRecyclerViewAdapter());
        mOptionsRecyclerView.setLayoutManager(new LinearLayoutManager(requireActivity()));
        mOptionsRecyclerView.setHasFixedSize(true);
        mOptionsRecyclerView.addItemDecoration(
                new DividerItemDecoration(requireActivity(), DividerItemDecoration.HORIZONTAL));
    }

    @Nullable
    @Override
    public View onCreateView(
            @NonNull LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.options_fragment, container, false);
        init(view);
        return view;
    }
}
