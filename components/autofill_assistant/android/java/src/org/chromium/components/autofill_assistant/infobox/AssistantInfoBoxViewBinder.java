// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.infobox;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.TextView;

import org.chromium.components.autofill_assistant.AssistantTextUtils;
import org.chromium.components.autofill_assistant.R;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * This class is responsible for pushing updates to the Autofill Assistant info box view. These
 * updates are pulled from the {@link AssistantInfoBoxModel} when a notification of an update is
 * received.
 */
class AssistantInfoBoxViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<AssistantInfoBoxModel,
                AssistantInfoBoxViewBinder.ViewHolder, PropertyKey> {
    /**
     * A wrapper class that holds the different views of the info box.
     */
    static class ViewHolder {
        final TextView mExplanationView;

        public ViewHolder(Context context, View infoBoxView) {
            mExplanationView = infoBoxView.findViewById(R.id.info_box_explanation);
        }
    }

    private final Context mContext;
    private ImageFetcher mImageFetcher;

    /** Allows to inject an image fetcher for testing. */
    AssistantInfoBoxViewBinder(Context context, ImageFetcher imageFetcher) {
        mContext = context;
        mImageFetcher = imageFetcher;
    }

    /**
     * Explicitly clean up.
     */
    public void destroy() {
        mImageFetcher.destroy();
        mImageFetcher = null;
    }

    @Override
    public void bind(AssistantInfoBoxModel model, ViewHolder view, PropertyKey propertyKey) {
        if (AssistantInfoBoxModel.INFO_BOX == propertyKey) {
            AssistantInfoBox infoBox = model.get(AssistantInfoBoxModel.INFO_BOX);
            if (infoBox == null) {
                // Handled by the AssistantInfoBoxCoordinator.
                return;
            }

            setInfoBox(infoBox, view);
        } else {
            assert false : "Unhandled property detected in AssistantInfoBoxViewBinder!";
        }
    }

    private void setInfoBox(AssistantInfoBox infoBox, ViewHolder viewHolder) {
        String explanation = infoBox.getExplanation();
        AssistantTextUtils.applyVisualAppearanceTags(
                viewHolder.mExplanationView, explanation, null);
        viewHolder.mExplanationView.announceForAccessibility(viewHolder.mExplanationView.getText());
        if (infoBox.getImagePath().isEmpty()) {
            viewHolder.mExplanationView.setCompoundDrawablesWithIntrinsicBounds(0, 0, 0, 0);
        } else {
            ImageFetcher.Params params = ImageFetcher.Params.create(
                    infoBox.getImagePath(), ImageFetcher.ASSISTANT_INFO_BOX_UMA_CLIENT_NAME);
            mImageFetcher.fetchImage(params, image -> {
                if (image != null) {
                    Drawable d = new BitmapDrawable(mContext.getResources(), image);
                    viewHolder.mExplanationView.setCompoundDrawablesWithIntrinsicBounds(
                            null, d, null, null);
                }
            });
        }
    }
}
