// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.Button;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;
import androidx.annotation.StyleRes;
import androidx.constraintlayout.widget.ConstraintSet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageView;

/** Binds the Fusebox properties to the view and component. */
@NullMarked
class FuseboxViewBinder {
    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    public static void bind(PropertyModel model, FuseboxViewHolder view, PropertyKey propertyKey) {
        // go/keep-sorted start block=yes by_regex=propertyKey\s*==\s*FuseboxProperties\.(\w+)
        if (propertyKey == FuseboxProperties.ADAPTER) {
            view.attachmentsView.setAdapter(model.get(FuseboxProperties.ADAPTER));
        } else if (propertyKey == FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE) {
            view.addButton.setVisibility(
                    model.get(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
            updateButtonsVisibilityAndStyling(model, view);
        } else if (propertyKey == FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE) {
            reanchorViewsForCompactFusebox(model, view);
            updateButtonsVisibilityAndStyling(model, view);
            updateToolDrawables(model.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE), view);
        } else if (propertyKey == FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE) {
            updateButtonsVisibilityAndStyling(model, view);
        } else if (propertyKey == FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CLICKED) {
            view.requestType.setOnClickListener(
                    v -> model.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.ATTACHMENTS_VISIBLE) {
            boolean visible = model.get(FuseboxProperties.ATTACHMENTS_VISIBLE);
            view.attachmentsView.setVisibility(visible ? View.VISIBLE : View.GONE);
        } else if (propertyKey == FuseboxProperties.BUTTON_ADD_CLICKED) {
            view.addButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.BUTTON_ADD_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.COLOR_SCHEME) {
            updateButtonsVisibilityAndStyling(model, view);
            Context context = view.parentView.getContext();
            @BrandedColorScheme int brandedColorScheme = model.get(FuseboxProperties.COLOR_SCHEME);
            Drawable background =
                    OmniboxResourceProvider.getPopupBackgroundDrawable(context, brandedColorScheme);
            view.popup.mPopupWindow.setBackgroundDrawable(background);
        } else if (propertyKey == FuseboxProperties.COMPACT_UI) {
            reanchorViewsForCompactFusebox(model, view);
        } else if (propertyKey == FuseboxProperties.CURRENT_TAB_BUTTON_CLICKED) {
            view.popup.mAddCurrentTab.setOnClickListener(
                    v -> model.get(FuseboxProperties.CURRENT_TAB_BUTTON_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.CURRENT_TAB_BUTTON_ENABLED) {
            setIsEnabledAndReapplyColorFilter(
                    view.popup.mAddCurrentTab,
                    model.get(FuseboxProperties.CURRENT_TAB_BUTTON_ENABLED));
        } else if (propertyKey == FuseboxProperties.CURRENT_TAB_BUTTON_FAVICON) {
            updateForCurrentTabFavicon(
                    model.get(FuseboxProperties.CURRENT_TAB_BUTTON_FAVICON), view);
        } else if (propertyKey == FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE) {
            view.popup.mAddCurrentTab.setVisibility(
                    model.get(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (propertyKey == FuseboxProperties.POPUP_AI_MODE_CLICKED) {
            view.popup.mAiModeButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_AI_MODE_CLICKED).run());
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
            setIsEnabledAndReapplyColorFilter(
                    view.popup.mCreateImageButton,
                    model.get(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_ENABLED));
        } else if (propertyKey == FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_VISIBLE) {
            updateButtonsVisibilityAndStyling(model, view);
        } else if (propertyKey == FuseboxProperties.POPUP_CREATE_IMAGE_CLICKED) {
            view.popup.mCreateImageButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_CREATE_IMAGE_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_FILE_BUTTON_ENABLED) {
            view.popup.mFileButton.setEnabled(
                    model.get(FuseboxProperties.POPUP_FILE_BUTTON_ENABLED));
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
        } else if (propertyKey == FuseboxProperties.SHOW_DEDICATED_MODE_BUTTON) {
            updateButtonsVisibilityAndStyling(model, view);
        } else if (propertyKey == FuseboxProperties.POPUP_TAB_PICKER_ENABLED) {
            view.popup.mTabButton.setEnabled(model.get(FuseboxProperties.POPUP_TAB_PICKER_ENABLED));
        }
        // go/keep-sorted end
    }

    private static void setIsEnabledAndReapplyColorFilter(Button button, boolean isEnabled) {
        button.setEnabled(isEnabled);
        reapplyColorFilter(button);
    }

    /**
     * Most of the button's drawables used by this class will pick up a default tint from the
     * button. But some of them need to retain the original coloring, such as a tab favicon or the
     * generate image banana. It is these drawables that this function is used for. Because the
     * button will not override the tint of a drawable that has had a color filter applied, we
     * always call this method to set the color filter for these drawables. However this is
     * complicated by not having a color filter implementation that takes a {@link ColorStateList}.
     * These drawables need to slightly fade when the button is disabled, and then stop fading when
     * the button is enabled. So this method should be called any time a relevant state change
     * happens to the button that would cause the color state list to return a different color.
     */
    private static void reapplyColorFilter(Button button) {
        // Only the start drawable needs to have special handling, all the others will always use
        // the default tint of the button.
        Drawable drawable = button.getCompoundDrawablesRelative()[0];
        if (drawable == null) return;

        Context context = button.getContext();
        // For some reason, the drawable and the button don't seem to agree on state, use the
        // button's version, as it tracks what we would expect current setters on the button to
        // result in.
        int[] stateSet = button.getDrawableState();
        ColorStateList tint = context.getColorStateList(R.color.default_icon_color_white_tint_list);
        @ColorInt int color = tint.getColorForState(stateSet, Color.TRANSPARENT);
        drawable.setColorFilter(new PorterDuffColorFilter(color, PorterDuff.Mode.MULTIPLY));
    }

    private static void updateToolDrawables(
            @AutocompleteRequestType int autocompleteRequestType, FuseboxViewHolder views) {
        Context context = views.parentView.getContext();
        final Drawable aiModeButtonEndDrawable;
        final Drawable imageGenEndDrawable;
        switch (autocompleteRequestType) {
            case AutocompleteRequestType.AI_MODE -> {
                aiModeButtonEndDrawable =
                        assumeNonNull(context.getDrawable(R.drawable.m3_ic_check_24px));
                imageGenEndDrawable = null;
            }
            case AutocompleteRequestType.IMAGE_GENERATION -> {
                aiModeButtonEndDrawable = null;
                imageGenEndDrawable =
                        assumeNonNull(context.getDrawable(R.drawable.m3_ic_check_24px));
            }
            default -> {
                aiModeButtonEndDrawable = null;
                imageGenEndDrawable = null;
            }
        }

        final Drawable aiModeButtonStartDrawable =
                context.getDrawable(R.drawable.search_spark_black_24dp);
        views.popup.mAiModeButton.setCompoundDrawablesRelativeWithIntrinsicBounds(
                aiModeButtonStartDrawable, null, aiModeButtonEndDrawable, null);

        // This drawable will be manually tinted with a filter, while all the others in this method
        // will pick up the default from the button.
        final Drawable imageGenStartDrawable =
                assumeNonNull(context.getDrawable(R.drawable.create_image_24dp)).mutate();
        views.popup.mCreateImageButton.setCompoundDrawablesRelativeWithIntrinsicBounds(
                imageGenStartDrawable, null, imageGenEndDrawable, null);
        reapplyColorFilter(views.popup.mCreateImageButton);
    }

    static void updateButtonsVisibilityAndStyling(PropertyModel model, FuseboxViewHolder views) {
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
        boolean showTryAiModeHintInDedicatedModeButton =
                OmniboxFeatures.sShowTryAiModeHintInDedicatedModeButton.getValue();
        @BrandedColorScheme int brandedColorScheme = model.get(FuseboxProperties.COLOR_SCHEME);
        Context context = views.parentView.getContext();
        Resources res = context.getResources();

        ChromeImageView addButton = views.addButton;
        addButton.setVisibility(showFuseboxToolbar ? View.VISIBLE : View.GONE);
        addButton.setBackground(
                OmniboxResourceProvider.getSearchBoxIconBackground(context, brandedColorScheme));
        addButton.setImageTintList(
                OmniboxResourceProvider.getPrimaryIconTintList(context, brandedColorScheme));

        views.navigateButton
                .getDrawable()
                .setTint(
                        OmniboxResourceProvider.getSendIconContrastColor(
                                context, brandedColorScheme));
        @ColorInt
        int colorPrimary = OmniboxResourceProvider.getColorPrimary(context, brandedColorScheme);
        views.navigateButton.getBackground().setTint(colorPrimary);

        ButtonCompat typeButton = views.requestType;
        if (showFuseboxToolbar
                && (isAiModeUsed || isImageGenerationUsed || showDedicatedModeButton)) {
            final String text;
            final String description;
            final @ColorInt int buttonColor;
            final @ColorInt int borderColor;
            final @StyleRes int textAppearanceRes;
            final Drawable startDrawable;
            final Drawable endDrawable;
            if (isAiModeUsed) {
                text = res.getString(R.string.ai_mode_entrypoint_label);
                description = res.getString(R.string.accessibility_omnibox_reset_mode, text);
                buttonColor =
                        OmniboxResourceProvider.getAiModeButtonColor(context, brandedColorScheme);
                borderColor =
                        OmniboxResourceProvider.getRequestTypeButtonBorderColor(
                                context, brandedColorScheme);
                textAppearanceRes =
                        OmniboxResourceProvider.getAiModeButtonTextRes(brandedColorScheme);
                startDrawable =
                        assumeNonNull(context.getDrawable(R.drawable.search_spark_black_24dp))
                                .mutate();
                startDrawable.setTint(colorPrimary);
                endDrawable = assumeNonNull(context.getDrawable(R.drawable.btn_close)).mutate();
                endDrawable.setTint(colorPrimary);
            } else if (isImageGenerationUsed) {
                text = res.getString(R.string.omnibox_create_image);
                description = res.getString(R.string.accessibility_omnibox_reset_mode, text);
                buttonColor =
                        OmniboxResourceProvider.getImageGenButtonColor(context, brandedColorScheme);
                borderColor =
                        OmniboxResourceProvider.getRequestTypeButtonBorderColor(
                                context, brandedColorScheme);
                textAppearanceRes =
                        OmniboxResourceProvider.getImageGenButtonTextRes(brandedColorScheme);
                startDrawable = context.getDrawable(R.drawable.create_image_24dp);
                endDrawable = assumeNonNull(context.getDrawable(R.drawable.btn_close)).mutate();
                endDrawable.setTint(
                        OmniboxResourceProvider.getDefaultIconColor(context, brandedColorScheme));
            } else if (showTryAiModeHintInDedicatedModeButton) {
                text = res.getString(R.string.ai_mode_entrypoint_hint);
                description = text;
                buttonColor = Color.TRANSPARENT;
                borderColor =
                        OmniboxResourceProvider.getAiModeHintBorderColor(
                                context, brandedColorScheme);
                textAppearanceRes =
                        OmniboxResourceProvider.getAiModeHintTextRes(brandedColorScheme);
                startDrawable =
                        assumeNonNull(context.getDrawable(R.drawable.search_spark_black_24dp))
                                .mutate();
                startDrawable.setTint(
                        OmniboxResourceProvider.getAiModeHintIconTintColor(
                                context, brandedColorScheme));
                endDrawable = null;
            } else /* dedicated button with aimode off, no hint text changes. */ {
                text = res.getString(R.string.ai_mode_entrypoint_label);
                description = res.getString(R.string.accessibility_omnibox_enable_ai_mode);
                buttonColor = Color.TRANSPARENT;
                borderColor =
                        OmniboxResourceProvider.getAiModeHintBorderColor(
                                context, brandedColorScheme);
                textAppearanceRes =
                        OmniboxResourceProvider.getAiModeHintTextRes(brandedColorScheme);
                startDrawable =
                        assumeNonNull(context.getDrawable(R.drawable.search_spark_black_24dp))
                                .mutate();
                startDrawable.setTint(
                        OmniboxResourceProvider.getAiModeHintIconTintColor(
                                context, brandedColorScheme));
                endDrawable = null;
            }

            @Px int iconSizePx = res.getDimensionPixelSize(R.dimen.fusebox_button_icon_size);
            scaleDrawable(startDrawable, iconSizePx);
            scaleDrawable(endDrawable, iconSizePx);

            typeButton.setVisibility(View.VISIBLE);
            typeButton.setText(text);
            typeButton.setContentDescription(description);
            typeButton.setButtonColor(ColorStateList.valueOf(buttonColor));
            typeButton.setBorderColor(ColorStateList.valueOf(borderColor));
            typeButton.setTextAppearance(textAppearanceRes);
            typeButton.setCompoundDrawablesRelative(startDrawable, null, endDrawable, null);
        } else {
            typeButton.setVisibility(View.GONE);
        }

        boolean isCreateImageButtonVisible =
                isRequestTypeChangeable
                        && model.get(FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_VISIBLE);
        views.popup.mAiModeButton.setVisibility(isRequestTypeChangeable ? View.VISIBLE : View.GONE);
        views.popup.mCreateImageButton.setVisibility(
                isCreateImageButtonVisible ? View.VISIBLE : View.GONE);
        views.popup.mRequestTypeDivider.setVisibility(
                isRequestTypeChangeable ? View.VISIBLE : View.GONE);

        @StyleRes
        int textAppearance = OmniboxResourceProvider.getPopupButtonTextRes(brandedColorScheme);
        ColorStateList iconTint =
                OmniboxResourceProvider.getPrimaryIconTintList(context, brandedColorScheme);
        for (Button button : views.popup.mButtons) {
            button.setTextAppearance(textAppearance);
            // Color filters applied to drawables will take precedence over this tint.
            button.setCompoundDrawableTintList(iconTint);
        }

        @ColorInt
        int dividerLineColor =
                OmniboxResourceProvider.getPopupDividerLineColor(context, brandedColorScheme);
        for (View divider : views.popup.mDividers) {
            divider.setBackgroundColor(dividerLineColor);
        }
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

    private static void updateForCurrentTabFavicon(Bitmap favicon, FuseboxViewHolder viewHolder) {
        Context context = viewHolder.parentView.getContext();
        Resources res = context.getResources();
        FuseboxPopup popup = viewHolder.popup;
        Button addCurrentTabButton = popup.mAddCurrentTab;

        Drawable drawable =
                FuseboxTabUtils.getDrawableForTabFavicon(
                        context,
                        favicon,
                        res.getDimensionPixelSize(R.dimen.fusebox_popup_item_icon_size));
        addCurrentTabButton.setCompoundDrawablesRelativeWithIntrinsicBounds(
                drawable, /* top= */ null, /* end= */ null, /* bottom= */ null);

        if (favicon != null) {
            // This will change the alpha value based on the enabled state. The rgb values will
            // always be unaffected because the multiplied color is white.
            reapplyColorFilter(addCurrentTabButton);
        }
    }

    private static void scaleDrawable(@Nullable Drawable drawable, @Px int sizePx) {
        if (drawable == null) return;
        drawable.setBounds(0, 0, sizePx, sizePx);
    }
}
