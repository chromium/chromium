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
import androidx.annotation.StringRes;
import androidx.annotation.StyleRes;
import androidx.constraintlayout.widget.ConstraintSet;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageView;

/** Binds the Fusebox properties to the view and component. */
@NullMarked
class FuseboxViewBinder {
    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    public static void bind(PropertyModel model, FuseboxViewHolder view, PropertyKey propertyKey) {
        if (propertyKey == FuseboxProperties.ADAPTER) {
            view.attachmentsView.setAdapter(model.get(FuseboxProperties.ADAPTER));
        } else if (propertyKey == FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE) {
            updateAddButton(model, view);
            updateRequestTypeButton(model, view);
            reanchorViewsForCompactFusebox(model, view);
        } else if (propertyKey == FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE) {
            updateRequestTypeButton(model, view);
            updateButtonsA11yAnnouncements(model, view);
            updateToolDrawables(model.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE), view);
        } else if (propertyKey == FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CLICKED) {
            view.requestType.setOnClickListener(
                    v -> model.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.ATTACHMENTS_VISIBLE) {
            boolean visible = model.get(FuseboxProperties.ATTACHMENTS_VISIBLE);
            view.attachmentsView.setVisibility(visible ? View.VISIBLE : View.GONE);

            // This fixes a flicker we see when transitioning from 0 attachments to 1 attachment.
            // The last attachment would be shown at the start of the fade animation, and any
            // attempt to reset the attachment View or clear out pending animations didn't help. The
            // correct solution is probably to instead allow the fade out animation to play, but
            // that's difficult due to how this and similar classes are set up here. We don't have
            // control over event sequencing or good observability on RV animations. Note when
            // trying to repro this bug, as of writing only the add current tab context is able to
            // trigger this, all of the intent based context flows have full screen animations that
            // hide inconsistencies. Lastly, this removeAllViews() fixes the issue when invoked on
            // either visibility edge. Here we're running it when hidden instead of when shown.
            // While it doesn't really matter, this kind of shows that we've given up on the fade
            // out animation, but we're trying to avoid tampering with the fade in animation, which
            // still works.
            if (!visible) {
                LayoutManager layoutManager = view.attachmentsView.getLayoutManager();
                if (layoutManager != null) {
                    layoutManager.removeAllViews();
                }
            }
        } else if (propertyKey == FuseboxProperties.BUTTON_ADD_CLICKED) {
            view.addButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.BUTTON_ADD_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.COLOR_SCHEME) {
            updateButtonsVisibilityAndStyling(model, view);
        } else if (propertyKey == FuseboxProperties.COMPACT_UI) {
            reanchorViewsForCompactFusebox(model, view);
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_CAMERA_CLICKED) {
            view.popup.mCameraButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_ATTACH_CAMERA_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_CAMERA_ENABLED) {
            view.popup.mCameraButton.setEnabled(
                    model.get(FuseboxProperties.POPUP_ATTACH_CAMERA_ENABLED));
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_CAMERA_VISIBLE) {
            updateButtonVisibility(
                    model, FuseboxProperties.POPUP_ATTACH_CAMERA_VISIBLE, view.popup.mCameraButton);
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_CLIPBOARD_CLICKED) {
            view.popup.mClipboardButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_ATTACH_CLIPBOARD_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_CLIPBOARD_ENABLED) {
            view.popup.mClipboardButton.setEnabled(
                    model.get(FuseboxProperties.POPUP_ATTACH_CLIPBOARD_ENABLED));
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_CLIPBOARD_VISIBLE) {
            updateButtonVisibility(
                    model,
                    FuseboxProperties.POPUP_ATTACH_CLIPBOARD_VISIBLE,
                    view.popup.mClipboardButton);
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_CLICKED) {
            view.popup.mAddCurrentTab.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED) {
            setIsEnabledAndReapplyColorFilter(
                    model,
                    FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED,
                    view.popup.mAddCurrentTab);
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_FAVICON) {
            updateForCurrentTabFavicon(
                    model.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_FAVICON), view);
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE) {
            updateButtonVisibility(
                    model,
                    FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE,
                    view.popup.mAddCurrentTab);
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_FILE_CLICKED) {
            view.popup.mFileButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_ATTACH_FILE_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_FILE_ENABLED) {
            view.popup.mFileButton.setEnabled(
                    model.get(FuseboxProperties.POPUP_ATTACH_FILE_ENABLED));
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_FILE_VISIBLE) {
            updateButtonVisibility(
                    model, FuseboxProperties.POPUP_ATTACH_FILE_VISIBLE, view.popup.mFileButton);
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_GALLERY_CLICKED) {
            view.popup.mGalleryButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_ATTACH_GALLERY_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_GALLERY_ENABLED) {
            view.popup.mGalleryButton.setEnabled(
                    model.get(FuseboxProperties.POPUP_ATTACH_GALLERY_ENABLED));
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_GALLERY_VISIBLE) {
            updateButtonVisibility(
                    model,
                    FuseboxProperties.POPUP_ATTACH_GALLERY_VISIBLE,
                    view.popup.mGalleryButton);
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_TAB_PICKER_CLICKED) {
            view.popup.mTabButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_TAB_PICKER_ENABLED) {
            view.popup.mTabButton.setEnabled(
                    model.get(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_ENABLED));
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_TAB_PICKER_VISIBLE) {
            updateButtonVisibility(
                    model,
                    FuseboxProperties.POPUP_ATTACH_TAB_PICKER_VISIBLE,
                    view.popup.mTabButton);
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_AI_MODE_CLICKED) {
            view.popup.mAiModeButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_TOOL_AI_MODE_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_AI_MODE_ENABLED) {
            view.popup.mAiModeButton.setEnabled(
                    model.get(FuseboxProperties.POPUP_TOOL_AI_MODE_ENABLED));
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_AI_MODE_VISIBLE) {
            updateButtonVisibility(
                    model, FuseboxProperties.POPUP_TOOL_AI_MODE_VISIBLE, view.popup.mAiModeButton);
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_CLICKED) {
            view.popup.mCreateImageButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_ENABLED) {
            setIsEnabledAndReapplyColorFilter(
                    model,
                    FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_ENABLED,
                    view.popup.mCreateImageButton);
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_VISIBLE) {
            updateButtonVisibility(
                    model,
                    FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_VISIBLE,
                    view.popup.mCreateImageButton);
        } else if (propertyKey == FuseboxProperties.SHOW_DEDICATED_MODE_BUTTON) {
            updateRequestTypeButton(model, view);
        }
    }

    private static void updateButtonVisibility(
            PropertyModel model, ReadableBooleanPropertyKey key, Button button) {
        boolean isVisible = model.get(key);
        button.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    private static void setIsEnabledAndReapplyColorFilter(
            PropertyModel model, ReadableBooleanPropertyKey key, Button button) {
        boolean isEnabled = model.get(key);
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
            @AutocompleteRequestType int autocompleteRequestType, FuseboxViewHolder view) {
        Context context = view.parentView.getContext();
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
        view.popup.mAiModeButton.setCompoundDrawablesRelativeWithIntrinsicBounds(
                aiModeButtonStartDrawable, null, aiModeButtonEndDrawable, null);

        // This drawable will be manually tinted with a filter, while all the others in this method
        // will pick up the default from the button.
        final Drawable imageGenStartDrawable =
                assumeNonNull(context.getDrawable(R.drawable.create_image_24dp)).mutate();
        view.popup.mCreateImageButton.setCompoundDrawablesRelativeWithIntrinsicBounds(
                imageGenStartDrawable, null, imageGenEndDrawable, null);
        reapplyColorFilter(view.popup.mCreateImageButton);
    }

    private static void updateButtonsA11yAnnouncements(
            PropertyModel model, FuseboxViewHolder view) {
        @StringRes
        int navButtonAccessibilityStringRes = R.string.acc_send_button_search_or_navigate;
        @StringRes
        int aiModeButtonAccessibilityStringRes = R.string.accessibility_omnibox_enable_ai_mode;
        @StringRes
        int imageGenButtonAccessibilityStringRes = R.string.accessibility_omnibox_create_image;
        switch (model.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE)) {
            case AutocompleteRequestType.AI_MODE:
                navButtonAccessibilityStringRes = R.string.acc_send_button_send_to_ai;
                aiModeButtonAccessibilityStringRes = R.string.acc_ai_mode_selected;
                break;
            case AutocompleteRequestType.IMAGE_GENERATION:
                navButtonAccessibilityStringRes = R.string.acc_send_button_create_image;
                imageGenButtonAccessibilityStringRes = R.string.acc_create_image_selected;
                break;
            case AutocompleteRequestType.SEARCH:
                break;
            default:
                assert false : "Missing A11y announcement for the fusebox button in this context";
                break;
        }

        var res = view.parentView.getResources();
        view.navigateButton.setContentDescription(res.getText(navButtonAccessibilityStringRes));
        view.popup.mAiModeButton.setContentDescription(
                res.getText(aiModeButtonAccessibilityStringRes));
        view.popup.mCreateImageButton.setContentDescription(
                res.getText(imageGenButtonAccessibilityStringRes));
    }

    private static void updateButtonsVisibilityAndStyling(
            PropertyModel model, FuseboxViewHolder view) {
        updateAddButton(model, view);
        updateNavigateButton(model, view);
        updateRequestTypeButton(model, view);
        updatePopupTheme(model, view);
        Context context = view.parentView.getContext();
        @BrandedColorScheme int brandedColorScheme = model.get(FuseboxProperties.COLOR_SCHEME);
        Drawable background =
                OmniboxResourceProvider.getPopupBackgroundDrawable(context, brandedColorScheme);
        view.popup.mPopupWindow.setBackgroundDrawable(background);
    }

    private static void updateAddButton(PropertyModel model, FuseboxViewHolder view) {
        boolean showFuseboxToolbar = model.get(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE);
        @BrandedColorScheme int brandedColorScheme = model.get(FuseboxProperties.COLOR_SCHEME);
        Context context = view.parentView.getContext();

        ChromeImageView addButton = view.addButton;
        addButton.setVisibility(showFuseboxToolbar ? View.VISIBLE : View.GONE);
        addButton.setBackground(
                OmniboxResourceProvider.getSearchBoxIconBackground(context, brandedColorScheme));
        addButton.setImageTintList(
                OmniboxResourceProvider.getPrimaryIconTintList(context, brandedColorScheme));
    }

    private static void updateNavigateButton(PropertyModel model, FuseboxViewHolder view) {
        @BrandedColorScheme int brandedColorScheme = model.get(FuseboxProperties.COLOR_SCHEME);
        Context context = view.parentView.getContext();
        view.navigateButton
                .getDrawable()
                .setTint(
                        OmniboxResourceProvider.getSendIconContrastColor(
                                context, brandedColorScheme));
        @ColorInt
        int colorPrimary = OmniboxResourceProvider.getColorPrimary(context, brandedColorScheme);
        view.navigateButton.getBackground().setTint(colorPrimary);
    }

    private static void updateRequestTypeButton(PropertyModel model, FuseboxViewHolder view) {
        boolean showFuseboxToolbar = model.get(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE);
        boolean showDedicatedModeButton = model.get(FuseboxProperties.SHOW_DEDICATED_MODE_BUTTON);
        @AutocompleteRequestType
        int requestType = model.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE);
        boolean isAiModeUsed = requestType == AutocompleteRequestType.AI_MODE;
        boolean isImageGenerationUsed = requestType == AutocompleteRequestType.IMAGE_GENERATION;

        if (!showFuseboxToolbar
                || !(isAiModeUsed || isImageGenerationUsed || showDedicatedModeButton)) {
            view.requestType.setVisibility(View.GONE);
            return;
        }

        boolean showTryAiModeHintInDedicatedModeButton =
                OmniboxFeatures.sShowTryAiModeHintInDedicatedModeButton.getValue();
        @BrandedColorScheme int brandedColorScheme = model.get(FuseboxProperties.COLOR_SCHEME);
        Context context = view.parentView.getContext();
        Resources res = context.getResources();

        final String text;
        final String description;
        final @ColorInt int buttonColor;
        final @ColorInt int borderColor;
        final @StyleRes int textAppearanceRes;
        final Drawable startDrawable;
        final Drawable endDrawable;
        @ColorInt
        int colorPrimary = OmniboxResourceProvider.getColorPrimary(context, brandedColorScheme);
        if (isAiModeUsed) {
            text = res.getString(R.string.ai_mode_entrypoint_label);
            description = res.getString(R.string.accessibility_omnibox_reset_mode, text);
            buttonColor = OmniboxResourceProvider.getAiModeButtonColor(context, brandedColorScheme);
            borderColor =
                    OmniboxResourceProvider.getRequestTypeButtonBorderColor(
                            context, brandedColorScheme);
            textAppearanceRes = OmniboxResourceProvider.getAiModeButtonTextRes(brandedColorScheme);
            startDrawable =
                    assumeNonNull(context.getDrawable(R.drawable.search_spark_black_24dp)).mutate();
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
                    OmniboxResourceProvider.getAiModeHintBorderColor(context, brandedColorScheme);
            textAppearanceRes = OmniboxResourceProvider.getAiModeHintTextRes(brandedColorScheme);
            startDrawable =
                    assumeNonNull(context.getDrawable(R.drawable.search_spark_black_24dp)).mutate();
            startDrawable.setTint(
                    OmniboxResourceProvider.getAiModeHintIconTintColor(
                            context, brandedColorScheme));
            endDrawable = null;
        } else /* dedicated button with aimode off, no hint text changes. */ {
            text = res.getString(R.string.ai_mode_entrypoint_label);
            description = res.getString(R.string.accessibility_omnibox_enable_ai_mode);
            buttonColor = Color.TRANSPARENT;
            borderColor =
                    OmniboxResourceProvider.getAiModeHintBorderColor(context, brandedColorScheme);
            textAppearanceRes = OmniboxResourceProvider.getAiModeHintTextRes(brandedColorScheme);
            startDrawable =
                    assumeNonNull(context.getDrawable(R.drawable.search_spark_black_24dp)).mutate();
            startDrawable.setTint(
                    OmniboxResourceProvider.getAiModeHintIconTintColor(
                            context, brandedColorScheme));
            endDrawable = null;
        }

        @Px int iconSizePx = res.getDimensionPixelSize(R.dimen.fusebox_button_icon_size);
        scaleDrawable(startDrawable, iconSizePx);
        scaleDrawable(endDrawable, iconSizePx);

        ButtonCompat button = view.requestType;
        button.setVisibility(View.VISIBLE);
        button.setText(text);
        button.setContentDescription(description);
        button.setButtonColor(ColorStateList.valueOf(buttonColor));
        button.setBorderColor(ColorStateList.valueOf(borderColor));
        button.setTextAppearance(textAppearanceRes);
        button.setCompoundDrawablesRelative(startDrawable, null, endDrawable, null);
    }

    private static void updatePopupTheme(PropertyModel model, FuseboxViewHolder view) {
        @BrandedColorScheme int brandedColorScheme = model.get(FuseboxProperties.COLOR_SCHEME);
        Context context = view.parentView.getContext();

        @StyleRes
        int textAppearance = OmniboxResourceProvider.getPopupButtonTextRes(brandedColorScheme);
        ColorStateList iconTint =
                OmniboxResourceProvider.getPrimaryIconTintList(context, brandedColorScheme);
        for (Button button : view.popup.mButtons) {
            button.setTextAppearance(textAppearance);
            // Color filters applied to drawables will take precedence over this tint.
            button.setCompoundDrawableTintList(iconTint);
        }

        @ColorInt
        int dividerLineColor =
                OmniboxResourceProvider.getPopupDividerLineColor(context, brandedColorScheme);
        for (View divider : view.popup.mDividers) {
            divider.setBackgroundColor(dividerLineColor);
        }
    }

    private static void reanchorViewsForCompactFusebox(
            PropertyModel model, FuseboxViewHolder view) {
        boolean shouldShowCompactUi =
                model.get(FuseboxProperties.COMPACT_UI)
                        || !model.get(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE);

        int topToTop = shouldShowCompactUi ? R.id.url_bar : ConstraintSet.UNSET;
        int topToBottom = shouldShowCompactUi ? ConstraintSet.UNSET : R.id.url_bar;
        int bottomToBottom = shouldShowCompactUi ? ConstraintSet.UNSET : ConstraintSet.PARENT_ID;

        var cs = new ConstraintSet();
        cs.clone(view.parentView);

        int id = view.addButton.getId();
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

        cs.connect(
                R.id.url_bar,
                ConstraintSet.END,
                shouldShowCompactUi ? R.id.action_buttons_segment : R.id.delete_button,
                ConstraintSet.START);

        cs.applyTo(view.parentView);
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
