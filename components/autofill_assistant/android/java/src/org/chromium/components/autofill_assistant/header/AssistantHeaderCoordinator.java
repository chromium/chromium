// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.header;

import android.content.Context;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.components.autofill_assistant.AssistantProfileImageUtil;
import org.chromium.components.autofill_assistant.AssistantSettingsUtil;
import org.chromium.components.autofill_assistant.LayoutUtils;
import org.chromium.components.autofill_assistant.R;
import org.chromium.components.autofill_assistant.carousel.AssistantChipAdapter;
import org.chromium.components.autofill_assistant.header.AssistantHeaderViewBinder.ViewHolder;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.util.AccessibilityUtil;

/**
 * Coordinator for the header of the Autofill Assistant.
 */
public class AssistantHeaderCoordinator implements AssistantProfileImageUtil.Observer {
    private final ViewGroup mView;
    private final ImageView mProfileView;
    private final ViewHolder mViewHolder;
    private final RecyclerView mChipsContainer;

    @Nullable
    private final AssistantProfileImageUtil mProfileImageUtil;

    public AssistantHeaderCoordinator(Context context, AssistantHeaderModel model,
            AccessibilityUtil accessibilityUtil,
            @Nullable AssistantProfileImageUtil profileImageUtil,
            AssistantSettingsUtil settingsUtil) {
        // Create the poodle and insert it before the status message. We have to create a view
        // bigger than the desired poodle size (24dp) because the actual downstream implementation
        // needs extra space for the animation.
        mView = (ViewGroup) LayoutUtils.createInflater(context).inflate(
                R.layout.autofill_assistant_header, /* root= */ null);
        AnimatedPoodle poodle = new AnimatedPoodle(context,
                context.getResources().getDimensionPixelSize(
                        R.dimen.autofill_assistant_poodle_view_size),
                context.getResources().getDimensionPixelSize(
                        R.dimen.autofill_assistant_poodle_size));
        addPoodle(mView, poodle.getView());
        mProfileView = mView.findViewById(R.id.profile_image);

        mProfileImageUtil = profileImageUtil;
        if (mProfileImageUtil != null) {
            mProfileImageUtil.addObserver(this);
        }

        mChipsContainer = new RecyclerView(context);
        final int innerChipSpacing = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_actions_spacing);
        mChipsContainer.addItemDecoration(new RecyclerView.ItemDecoration() {
            @Override
            public void getItemOffsets(@NonNull Rect outRect, @NonNull View view,
                    @NonNull RecyclerView parent, @NonNull RecyclerView.State state) {
                outRect.top = 0;
                outRect.bottom = 0;

                if (state.getItemCount() <= 1) {
                    return;
                }

                // If old position != NO_POSITION, it means the carousel is being animated and we
                // should use that position in our logic.
                int position = parent.getChildAdapterPosition(view);
                RecyclerView.ViewHolder viewHolder = parent.getChildViewHolder(view);
                if (viewHolder != null && viewHolder.getOldPosition() != RecyclerView.NO_POSITION) {
                    position = viewHolder.getOldPosition();
                }

                if (position == RecyclerView.NO_POSITION) {
                    return;
                }

                outRect.left = position == 0 ? 0 : innerChipSpacing;
                outRect.right = 0;
            }
        });

        AssistantChipAdapter chipAdapter = new AssistantChipAdapter();
        mChipsContainer.setAdapter(chipAdapter);
        LinearLayoutManager layoutManager = new LinearLayoutManager(context);
        layoutManager.setOrientation(LinearLayoutManager.HORIZONTAL);
        mChipsContainer.setLayoutManager(layoutManager);
        mView.setPadding(mChipsContainer.getPaddingLeft(), mChipsContainer.getPaddingTop(),
                context.getResources().getDimensionPixelSize(
                        R.dimen.autofill_assistant_profile_icon_padding),
                mChipsContainer.getPaddingBottom());
        ViewGroup topContainer = mView.findViewById(R.id.header_top_container);
        topContainer.addView(mChipsContainer);

        // Bind view and mediator through the model.
        mViewHolder =
                new AssistantHeaderViewBinder.ViewHolder(context, mView, poodle, mChipsContainer);
        AssistantHeaderViewBinder viewBinder =
                new AssistantHeaderViewBinder(accessibilityUtil, settingsUtil);
        PropertyModelChangeProcessor.create(model, mViewHolder, viewBinder);
    }

    @Override
    public void onProfileImageChanged(Drawable profileImage) {
        mProfileView.setImageDrawable(profileImage);
    }

    /** Return the view associated to this coordinator. */
    public View getView() {
        return mView;
    }

    /** Returns the view containing the chips. */
    public View getCarouselView() {
        return mChipsContainer;
    }

    /**
     * Cleanup resources when this goes out of scope.
     */
    public void destroy() {
        if (mProfileImageUtil != null) {
            mProfileImageUtil.removeObserver(this);
        }
    }

    private void addPoodle(View root, View poodleView) {
        ViewGroup parent = root.findViewById(R.id.poodle_wrapper);
        parent.addView(poodleView);
    }
}
