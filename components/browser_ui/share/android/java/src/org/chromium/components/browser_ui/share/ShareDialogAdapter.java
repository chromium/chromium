// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.share;

import android.content.Context;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import java.util.List;

/** Adapter that provides the list of activities via which a web page can be shared. */
class ShareDialogAdapter extends ArrayAdapter<ResolveInfo> {
    private final LayoutInflater mInflater;
    private final PackageManager mManager;

    /**
     * @param context Context used to for layout inflation.
     * @param manager PackageManager used to query for activity information.
     * @param objects The list of possible share intents.
     */
    public ShareDialogAdapter(Context context, PackageManager manager, List<ResolveInfo> objects) {
        super(context, R.layout.share_dialog_item, objects);
        mInflater = (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mManager = manager;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View view;
        if (convertView == null) {
            view = mInflater.inflate(R.layout.share_dialog_item, parent, false);
        } else {
            view = convertView;
        }
        TextView text = (TextView) view.findViewById(R.id.text);
        ImageView icon = (ImageView) view.findViewById(R.id.icon);

        text.setText(getItem(position).loadLabel(mManager));
        icon.setImageDrawable(ShareHelper.loadIconForResolveInfo(getItem(position), mManager));
        return view;
    }
}
