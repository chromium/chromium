// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.PorterDuff;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.Button;

import androidx.annotation.Px;
import androidx.constraintlayout.widget.ConstraintSet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.RippleBackgroundHelper;

/** Binds the Fusebox properties to the view and component. */
@NullMarked
class FuseboxViewBinder {
    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    public static void bind(PropertyModel model, FuseboxViewHolder view, PropertyKey propertyKey) {
        if (propertyKey == FuseboxProperties.ADAPTER) {
            view.attachmentsView.setAdapter(model.get(FuseboxProperties.ADAPTER));
        } else if (propertyKey == FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE) {
            reanchorViewsForCompactFusebox(model, view);
            updateModeSelectorVisibility(model, view);
        } else if (propertyKey == FuseboxProperties.COMPACT_UI) {
            reanchorViewsForCompactFusebox(model, view);
        } else if (propertyKey == FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CLICKED) {
            view.requestType.setOnClickListener(
                    v -> model.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_AI_MODE_CLICKED) {
            view.popup.mAiModeButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_AI_MODE_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.ATTACHMENTS_VISIBLE) {
            boolean visible = model.get(FuseboxProperties.ATTACHMENTS_VISIBLE);
            view.attachmentsView.setVisibility(visible ? View.VISIBLE : View.GONE);
        } else if (propertyKey == FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE) {
            view.addButton.setVisibility(
                    model.get(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
            updateModeSelectorVisibility(model, view);
        } else if (propertyKey == FuseboxProperties.BUTTON_ADD_CLICKED) {
            view.addButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.BUTTON_ADD_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE) {
            updateModeSelectorVisibility(model, view);
        } else if (propertyKey == FuseboxProperties.POPUP_CAMERA_CLICKED) {
            view.popup.mCameraButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_CAMERA_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_CLIPBOARD_BUTTON_VISIBLE) {
            view.popup.mClipboardButton.setVisibility(
                    model.get(FuseboxProperties.POPUP_CLIPBOARD_BUTTON_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (propertyKey == FuseboxProperties.POPUP_CLIPBOARD_CLICKED) {
            view.popup.mClipboardButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_CLIPBOARD_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_ENABLED) {
            view.popup.mCreateImageButton.setEnabled(
                    model.get(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_ENABLED));
        } else if (propertyKey == FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_VISIBLE) {
            // TODO(https://crbug.com/457465693): Set create image tool visibility.
        } else if (propertyKey == FuseboxProperties.POPUP_CREATE_IMAGE_CLICKED) {
            view.popup.mCreateImageButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_CREATE_IMAGE_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_FILE_BUTTON_VISIBLE) {
            view.popup.mFileButton.setVisibility(
                    model.get(FuseboxProperties.POPUP_FILE_BUTTON_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (propertyKey == FuseboxProperties.POPUP_FILE_CLICKED) {
            view.popup.mFileButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_FILE_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_GALLERY_CLICKED) {
            view.popup.mGalleryButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_GALLERY_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_TAB_PICKER_CLICKED) {
            view.popup.mTabButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_TAB_PICKER_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.CURRENT_TAB_BUTTON_CLICKED) {
            view.popup.mAddCurrentTab.setOnClickListener(
                    v -> model.get(FuseboxProperties.CURRENT_TAB_BUTTON_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.CURRENT_TAB_BUTTON_ENABLED) {
            view.popup.mAddCurrentTab.setEnabled(
                    model.get(FuseboxProperties.CURRENT_TAB_BUTTON_ENABLED));
        } else if (propertyKey == FuseboxProperties.CURRENT_TAB_BUTTON_FAVICON) {
            updateForCurrentTabFavicon(
                    model.get(FuseboxProperties.CURRENT_TAB_BUTTON_FAVICON), view);
        } else if (propertyKey == FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE) {
            view.popup.mAddCurrentTab.setVisibility(
                    model.get(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (propertyKey == FuseboxProperties.SHOW_DEDICATED_MODE_BUTTON) {
            updateModeSelectorVisibility(model, view);
        }
    }

    static void updateModeSelectorVisibility(PropertyModel model, FuseboxViewHolder views) {
        boolean isRequestTypeChangeable =
                model.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE);
        boolean showFuseboxToolbar = model.get(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE);
        boolean showDedicatedModeButton = model.get(FuseboxProperties.SHOW_DEDICATED_MODE_BUTTON);
        boolean isAiModeUsed =
                model.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE)
                        == AutocompleteRequestType.AI_MODE;
        boolean isImageGenerationUsed =
                model.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE)
                        == AutocompleteRequestType.IMAGE_GENERATION;
        boolean isCustomModeUsed = isAiModeUsed || isImageGenerationUsed;
        Context context = views.parentView.getContext();
        Resources res = context.getResources();

        views.addButton.setVisibility(showFuseboxToolbar ? View.VISIBLE : View.GONE);

        ButtonCompat typeButton = views.requestType;
        if (showFuseboxToolbar && (isCustomModeUsed || showDedicatedModeButton)) {
            typeButton.setVisibility(View.VISIBLE);

            final String text;
            final String description;
            if (isAiModeUsed) {
                text = res.getString(R.string.ai_mode_entrypoint_label);
                description = res.getString(R.string.accessibility_omnibox_reset_mode, text);
            } else if (isImageGenerationUsed) {
                text = res.getString(R.string.omnibox_create_image);
                description = res.getString(R.string.accessibility_omnibox_reset_mode, text);
            } else if (OmniboxFeatures.sShowTryAiModeHintInDedicatedModeButton.getValue()) {
                text = res.getString(R.string.ai_mode_entrypoint_hint);
                description = text;
            } else /* dedicated button with aimode off, no hint text changes. */ {
                text = res.getString(R.string.ai_mode_entrypoint_label);
                description = res.getString(R.string.accessibility_omnibox_enable_ai_mode);
            }
            typeButton.setText(text);
            typeButton.setContentDescription(description);

            typeButton.setButtonColor(
                    isCustomModeUsed
                            ? context.getColorStateList(R.color.gm3_baseline_surface_container)
                            : context.getColorStateList(android.R.color.transparent));

            typeButton.setBorderStyle(
                    isCustomModeUsed
                            ? RippleBackgroundHelper.BorderType.SOLID
                            : RippleBackgroundHelper.BorderType.DASHED);

            typeButton.setCompoundDrawablesRelativeWithIntrinsicBounds(
                    isImageGenerationUsed
                            ? context.getDrawable(R.drawable.create_image_24dp)
                            : context.getDrawable(R.drawable.search_spark_black_24dp),
                    null,
                    isCustomModeUsed ? context.getDrawable(R.drawable.btn_close) : null,
                    null);
            typeButton.setCompoundDrawableTintList(
                    isImageGenerationUsed
                            ? null
                            : ColorStateList.valueOf(SemanticColorUtils.getColorPrimary(context)));
        } else {
            typeButton.setVisibility(View.GONE);
        }

        boolean isAiModeButtonVisible = isRequestTypeChangeable && !showDedicatedModeButton;
        boolean isCreateImageButtonVisible =
                isRequestTypeChangeable
                        && model.get(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_VISIBLE);
        views.popup.mAiModeButton.setVisibility(isAiModeButtonVisible ? View.VISIBLE : View.GONE);
        views.popup.mCreateImageButton.setVisibility(
                isCreateImageButtonVisible ? View.VISIBLE : View.GONE);
        views.popup.mRequestTypeDivider.setVisibility(
                isAiModeButtonVisible || isCreateImageButtonVisible ? View.VISIBLE : View.GONE);
        views.popup.mFileButton.setEnabled(!isImageGenerationUsed);
    }

    static void reanchorViewsForCompactFusebox(PropertyModel model, FuseboxViewHolder views) {
        boolean shouldShowCompactUi = model.get(FuseboxProperties.COMPACT_UI);

        int topToTop = shouldShowCompactUi ? R.id.url_bar : ConstraintSet.UNSET;
        int topToBottom = shouldShowCompactUi ? ConstraintSet.UNSET : R.id.url_bar;
        int bottomToBottom = shouldShowCompactUi ? ConstraintSet.UNSET : ConstraintSet.PARENT_ID;

        var cs = new ConstraintSet();
        cs.clone(views.parentView);

        int id = views.addButton.getId();
        cs.clear(id, ConstraintSet.TOP);
        cs.clear(id, ConstraintSet.BOTTOM);
        cs.clear(id, ConstraintSet.BASELINE);

        if (topToTop != ConstraintSet.UNSET) {
            cs.connect(id, ConstraintSet.TOP, topToTop, ConstraintSet.TOP);
        }
        if (topToBottom != ConstraintSet.UNSET) {
            cs.connect(id, ConstraintSet.TOP, topToBottom, ConstraintSet.BOTTOM);
        }
        if (bottomToBottom != ConstraintSet.UNSET) {
            cs.connect(id, ConstraintSet.BOTTOM, bottomToBottom, ConstraintSet.BOTTOM);
        }

        cs.applyTo(views.parentView);
    }

    // TODO(https://crbug.com/460150759): Update to correctly tint for being disabled.
    private static void updateForCurrentTabFavicon(Bitmap favicon, FuseboxViewHolder viewHolder) {
        Context context = viewHolder.parentView.getContext();
        Resources res = context.getResources();
        Button addCurrentTabButton = viewHolder.popup.mAddCurrentTab;

        final Drawable drawable;
        final ColorStateList tint;
        final PorterDuff.Mode blendMode;
        if (favicon != null) {
            @Px int iconSizePx = res.getDimensionPixelSize(R.dimen.fusebox_popup_item_icon_size);
            Bitmap bitmap =
                    Bitmap.createScaledBitmap(favicon, iconSizePx, iconSizePx, /* filter= */ true);
            drawable = new BitmapDrawable(res, bitmap);
            drawable.setBounds(
                    /* left= */ 0, /* top= */ 0, /* right= */ iconSizePx, /* bottom= */ iconSizePx);
            // This will change the alpha value based on the enabled state. The rgb values will
            // always be unaffected because the multiplied color is white.
            tint = context.getColorStateList(R.color.default_icon_color_white_tint_list);
            blendMode = PorterDuff.Mode.MULTIPLY;
        } else {
            drawable = assumeNonNull(context.getDrawable(R.drawable.ic_globe_24dp));
            tint = context.getColorStateList(R.color.default_icon_color_tint_list);
            blendMode = PorterDuff.Mode.SRC_IN;
        }

        addCurrentTabButton.setCompoundDrawablesRelativeWithIntrinsicBounds(
                drawable, /* top= */ null, /* end= */ null, /* bottom= */ null);
        addCurrentTabButton.setCompoundDrawableTintList(tint);
        addCurrentTabButton.setCompoundDrawableTintMode(blendMode);
    }
}
