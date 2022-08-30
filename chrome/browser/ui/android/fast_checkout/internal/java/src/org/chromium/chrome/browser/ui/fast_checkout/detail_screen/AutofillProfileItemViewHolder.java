// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.detail_screen;

import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.LayoutRes;
import androidx.recyclerview.widget.RecyclerView;

/** A view for an individual Autofill profile item entry. */
class AutofillProfileItemViewHolder extends RecyclerView.ViewHolder {
    AutofillProfileItemViewHolder(ViewGroup parent, @LayoutRes int layout) {
        super(LayoutInflater.from(parent.getContext()).inflate(layout, parent, false));

        // TODO(crbug.com/1355310): Look up all relevant text elements inside the view and
        // expose setter methods for the contents.
    }
}
