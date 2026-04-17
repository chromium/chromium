// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.annotation.StyleRes;
import androidx.constraintlayout.widget.ConstraintSet;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.PopupButtonData;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.IconResourceIdsProto.IconResourceIds;
import org.chromium.components.omnibox.ToolModeUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageView;

import java.util.List;

/** Binds the Fusebox properties to the view and component. */
@NullMarked
class FuseboxViewBinder {
    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    public static void bind(PropertyModel model, FuseboxViewHolder view, PropertyKey propertyKey) {
        if (propertyKey == FuseboxProperties.ADAPTER) {
            view.attachmentsView.setAdapter(model.get(FuseboxProperties.ADAPTER));
        } else if (propertyKey == FuseboxProperties.ADD_BUTTON_VISIBLE) {
            updateAddButton(model, view);
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
        } else if (propertyKey == FuseboxProperties.FUSEBOX_STATE) {
            updateRequestTypeButton(model, view);
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
            boolean hasFavicon =
                    model.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_FAVICON) != null;
            if (hasFavicon) {
                setIsEnabledAndReapplyColorFilter(
                        model,
                        FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED,
                        view.popup.mAddCurrentTab);
            } else {
                view.popup.mAddCurrentTab.setEnabled(
                        model.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED));
            }
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
        } else if (propertyKey == FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST) {
            updateModelButtons(model, view);
        } else if (propertyKey == FuseboxProperties.POPUP_MODEL_DIVIDER_VISIBLE) {
            view.popup.mModelsDivider.setVisibility(
                    model.get(FuseboxProperties.POPUP_MODEL_DIVIDER_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (propertyKey == FuseboxProperties.POPUP_MODEL_HEADER_VISIBLE) {
            view.popup.mModelsHeader.setVisibility(
                    model.get(FuseboxProperties.POPUP_MODEL_HEADER_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (propertyKey == FuseboxProperties.POPUP_STATE) {
            view.popup.setPopupState(model.get(FuseboxProperties.POPUP_STATE));
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_AI_MODE_CLICKED) {
            view.popup.mAiModeButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_TOOL_AI_MODE_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_AI_MODE_ENABLED) {
            view.popup.mAiModeButton.setEnabled(
                    model.get(FuseboxProperties.POPUP_TOOL_AI_MODE_ENABLED));
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_AI_MODE_VISIBLE) {
            updateButtonVisibility(
                    model, FuseboxProperties.POPUP_TOOL_AI_MODE_VISIBLE, view.popup.mAiModeButton);
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_CANVAS_CLICKED) {
            view.popup.mCanvasButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_TOOL_CANVAS_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_CANVAS_ENABLED) {
            view.popup.mCanvasButton.setEnabled(
                    model.get(FuseboxProperties.POPUP_TOOL_CANVAS_ENABLED));
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_CANVAS_VISIBLE) {
            updateButtonVisibility(
                    model, FuseboxProperties.POPUP_TOOL_CANVAS_VISIBLE, view.popup.mCanvasButton);
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
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_DEEP_SEARCH_CLICKED) {
            view.popup.mDeepSearchButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_TOOL_DEEP_SEARCH_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_DEEP_SEARCH_ENABLED) {
            view.popup.mDeepSearchButton.setEnabled(
                    model.get(FuseboxProperties.POPUP_TOOL_DEEP_SEARCH_ENABLED));
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_DEEP_SEARCH_VISIBLE) {
            updateButtonVisibility(
                    model,
                    FuseboxProperties.POPUP_TOOL_DEEP_SEARCH_VISIBLE,
                    view.popup.mDeepSearchButton);
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_DIVIDER_VISIBLE) {
            view.popup.mToolsDivider.setVisibility(
                    model.get(FuseboxProperties.POPUP_TOOL_DIVIDER_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_HEADER_VISIBLE) {
            view.popup.mToolsHeader.setVisibility(
                    model.get(FuseboxProperties.POPUP_TOOL_HEADER_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (propertyKey == FuseboxProperties.SHOW_DEDICATED_MODE_BUTTON) {
            updateRequestTypeButton(model, view);
        }
    }

    private static void updateButtonVisibility(
            PropertyModel model, ReadableBooleanPropertyKey key, View button) {
        boolean isVisible = model.get(key);
        button.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    private static void setIsEnabledAndReapplyColorFilter(
            PropertyModel model, ReadableBooleanPropertyKey key, View button) {
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
    private static void reapplyColorFilter(View buttonView) {
        FuseboxItemViewHolder holder = getViewHolder(buttonView);
        ImageView imageView = holder.mActionIcon;
        if (imageView == null) return;

        Drawable drawable = imageView.getDrawable();
        if (drawable == null) return;

        Context context = buttonView.getContext();
        int[] stateSet = buttonView.getDrawableState();
        ColorStateList tint = context.getColorStateList(R.color.default_icon_color_white_tint_list);
        @ColorInt int color = tint.getColorForState(stateSet, Color.TRANSPARENT);
        drawable.setColorFilter(new PorterDuffColorFilter(color, PorterDuff.Mode.MULTIPLY));
    }

    private static void updateToolDrawables(
            @AutocompleteRequestType int autocompleteRequestType, FuseboxViewHolder view) {
        Context context = view.parentView.getContext();
        Drawable aiModeButtonEndDrawable = null;
        Drawable imageGenEndDrawable = null;
        Drawable deepSearchEndDrawable = null;
        Drawable canvasEndDrawable = null;
        if (autocompleteRequestType != AutocompleteRequestType.SEARCH
                && autocompleteRequestType != AutocompleteRequestType.SEARCH_PREFETCH) {
            Drawable checkmark = context.getDrawable(R.drawable.m3_ic_check_24px);
            switch (autocompleteRequestType) {
                case AutocompleteRequestType.AI_MODE -> {
                    aiModeButtonEndDrawable = checkmark;
                }
                case AutocompleteRequestType.IMAGE_GENERATION -> {
                    imageGenEndDrawable = checkmark;
                }
                case AutocompleteRequestType.DEEP_SEARCH -> {
                    deepSearchEndDrawable = checkmark;
                }
                case AutocompleteRequestType.CANVAS -> {
                    canvasEndDrawable = checkmark;
                }
            }
        }

        Drawable aiModeButtonStartDrawable =
                context.getDrawable(R.drawable.search_spark_black_24dp);
        setCustomButtonDrawables(
                view.popup.mAiModeButton, aiModeButtonStartDrawable, aiModeButtonEndDrawable);

        // This drawable will be manually tinted with a filter, while all the others in this method
        // will pick up the default from the button.
        Drawable imageGenStartDrawable =
                assumeNonNull(context.getDrawable(R.drawable.create_image_24dp)).mutate();
        setCustomButtonDrawables(
                view.popup.mCreateImageButton, imageGenStartDrawable, imageGenEndDrawable);
        reapplyColorFilter(view.popup.mCreateImageButton);

        Drawable deepSearchStartDrawable = context.getDrawable(R.drawable.travel_explore_24dp);
        setCustomButtonDrawables(
                view.popup.mDeepSearchButton, deepSearchStartDrawable, deepSearchEndDrawable);

        Drawable canvasStartDrawable = context.getDrawable(R.drawable.draft_spark_24dp);
        setCustomButtonDrawables(view.popup.mCanvasButton, canvasStartDrawable, canvasEndDrawable);
    }

    private static void updateModelButtons(PropertyModel model, FuseboxViewHolder view) {
        List<PopupButtonData> buttonDataList =
                model.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        ViewGroup group = view.popup.mViewGroup;
        int headerIndex = group.indexOfChild(view.popup.mModelsHeader);
        if (headerIndex == -1) {
            assert false;
            // TODO(https://crbug.com/493288340): Remove this return if this assert is never hit.
            return;
        }

        int startIndex = headerIndex + 1;
        int currentCount = group.getChildCount() - startIndex;
        int targetCount = buttonDataList == null ? 0 : buttonDataList.size();

        if (currentCount > targetCount) {
            for (int i = startIndex + targetCount; i < group.getChildCount(); i++) {
                view.popup.mButtons.remove(group.getChildAt(i));
            }
            group.removeViews(startIndex + targetCount, currentCount - targetCount);
        }

        @BrandedColorScheme int brandedColorScheme = model.get(FuseboxProperties.COLOR_SCHEME);
        for (int i = 0; i < targetCount; i++) {
            View buttonView;
            if (i < currentCount) {
                buttonView = group.getChildAt(startIndex + i);
            } else {
                buttonView =
                        LayoutInflater.from(group.getContext())
                                .inflate(R.layout.fusebox_list_item, group, false);
                group.addView(buttonView);
                view.popup.mButtons.add(buttonView);
            }
            bindDynamicButton(buttonView, buttonDataList.get(i), brandedColorScheme);
        }
    }

    private static void bindDynamicButton(
            View buttonView, PopupButtonData data, @BrandedColorScheme int brandedColorScheme) {
        buttonView.setOnClickListener((v) -> data.onClicked.run());
        ((TextView) buttonView.findViewById(R.id.action_text)).setText(data.text);
        buttonView.setEnabled(data.enabled);

        @StyleRes
        int textAppearance = OmniboxResourceProvider.getPopupButtonTextRes(brandedColorScheme);
        ColorStateList iconTint =
                OmniboxResourceProvider.getPrimaryIconTintList(
                        buttonView.getContext(), brandedColorScheme);
        themeButton(buttonView, textAppearance, iconTint);

        @DrawableRes int iconRes = getResIdForIconId(data.iconId);
        setButtonDrawables(buttonView, data.selected, iconRes);
    }

    private static void setButtonDrawables(
            View buttonView, boolean selected, @DrawableRes int iconRes) {
        FuseboxItemViewHolder holder = getViewHolder(buttonView);
        ImageView imageView = holder.mActionIcon;
        ImageView endImageView = holder.mActionEndIcon;

        if (iconRes != Resources.ID_NULL) {
            imageView.setImageResource(iconRes);
            imageView.setVisibility(View.VISIBLE);
        } else {
            imageView.setImageDrawable(null);
            imageView.setVisibility(View.GONE);
        }

        if (selected) {
            endImageView.setImageResource(R.drawable.m3_ic_check_24px);
            endImageView.setVisibility(View.VISIBLE);
        } else {
            endImageView.setImageDrawable(null);
            endImageView.setVisibility(View.GONE);
        }
    }

    private static void setCustomButtonDrawables(
            View buttonView, @Nullable Drawable startDrawable, @Nullable Drawable endDrawable) {
        FuseboxItemViewHolder holder = getViewHolder(buttonView);
        ImageView imageView = holder.mActionIcon;
        ImageView endImageView = holder.mActionEndIcon;

        imageView.setImageDrawable(startDrawable);
        imageView.setVisibility(startDrawable != null ? View.VISIBLE : View.GONE);

        endImageView.setImageDrawable(endDrawable);
        endImageView.setVisibility(endDrawable != null ? View.VISIBLE : View.GONE);
    }

    /** Maps ids found in generated protos to local resources backed drawable ids. */
    private static @DrawableRes int getResIdForIconId(int iconId) {
        if (iconId == IconResourceIds.AUTORENEW_VALUE) {
            return R.drawable.autorenew_24dp;
        } else if (iconId == IconResourceIds.TIMER_VALUE) {
            return R.drawable.ic_timer;
        } else if (iconId == IconResourceIds.BOLT_VALUE) {
            return R.drawable.bolt_24dp;
        }
        return Resources.ID_NULL;
    }

    private static void themeButton(
            View buttonView, @StyleRes int textAppearance, ColorStateList iconTint) {
        FuseboxItemViewHolder holder = getViewHolder(buttonView);
        TextView textView = holder.mActionText;
        ImageView imageView = holder.mActionIcon;
        ImageView endImageView = holder.mActionEndIcon;

        if (textView != null) {
            textView.setTextAppearance(textAppearance);
        }
        if (imageView != null) {
            imageView.setImageTintList(iconTint);
        }
        if (endImageView != null) {
            endImageView.setImageTintList(iconTint);
        }
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
            case AutocompleteRequestType.DEEP_SEARCH:
                // TODO(https://crbug.com/489115052): Improve accessibility strings here.
                navButtonAccessibilityStringRes = R.string.ntp_compose_deep_search;
                break;
            case AutocompleteRequestType.CANVAS:
                // TODO(https://crbug.com/476434308): Improve accessibility strings here.
                navButtonAccessibilityStringRes = R.string.ntp_compose_canvas;
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
        boolean showAddButton = model.get(FuseboxProperties.ADD_BUTTON_VISIBLE);
        ChromeImageView addButton = view.addButton;
        addButton.setVisibility(showAddButton ? View.VISIBLE : View.GONE);
        if (showAddButton) {
            @BrandedColorScheme int brandedColorScheme = model.get(FuseboxProperties.COLOR_SCHEME);
            Context context = view.parentView.getContext();
            addButton.setBackground(
                    OmniboxResourceProvider.getSearchBoxIconBackground(
                            context, brandedColorScheme));
            addButton.setImageTintList(
                    OmniboxResourceProvider.getPrimaryIconTintList(context, brandedColorScheme));
        }
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
        boolean fuseboxDisabled =
                model.get(FuseboxProperties.FUSEBOX_STATE) == FuseboxState.DISABLED;
        boolean showDedicatedModeButton = model.get(FuseboxProperties.SHOW_DEDICATED_MODE_BUTTON);
        @AutocompleteRequestType
        int requestType = model.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE);
        boolean aiToolSelected = ToolModeUtils.isAimRequest(requestType);

        if (fuseboxDisabled || !(aiToolSelected || showDedicatedModeButton)) {
            view.requestType.setVisibility(View.GONE);
            return;
        }

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

        if (aiToolSelected) {
            text = res.getString(getTextResForTool(requestType));
            description = res.getString(R.string.accessibility_omnibox_reset_mode, text);
            startDrawable = context.getDrawable(getIconResForTool(requestType));
            endDrawable = assumeNonNull(context.getDrawable(R.drawable.btn_close)).mutate();
            borderColor =
                    OmniboxResourceProvider.getRequestTypeButtonBorderColor(
                            context, brandedColorScheme);
            if (requestType == AutocompleteRequestType.IMAGE_GENERATION) {
                buttonColor =
                        OmniboxResourceProvider.getImageGenButtonColor(context, brandedColorScheme);

                textAppearanceRes =
                        OmniboxResourceProvider.getImageGenButtonTextRes(brandedColorScheme);
                endDrawable.setTint(
                        OmniboxResourceProvider.getDefaultIconColor(context, brandedColorScheme));
            } else {
                buttonColor =
                        OmniboxResourceProvider.getAiModeButtonColor(context, brandedColorScheme);
                textAppearanceRes =
                        OmniboxResourceProvider.getAiModeButtonTextRes(brandedColorScheme);
                assumeNonNull(startDrawable).mutate().setTint(colorPrimary);
                endDrawable.setTint(colorPrimary);
            }
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

    @SuppressLint("SwitchIntDef")
    private static @StringRes int getTextResForTool(@AutocompleteRequestType int requestType) {
        return switch (requestType) {
            case AutocompleteRequestType.AI_MODE -> R.string.ai_mode_entrypoint_label;
            case AutocompleteRequestType.IMAGE_GENERATION -> R.string.omnibox_create_image;
            case AutocompleteRequestType.DEEP_SEARCH -> R.string.ntp_compose_deep_search;
            case AutocompleteRequestType.CANVAS -> R.string.ntp_compose_canvas;
            default -> {
                assert false : "AutocompleteRequestType was not a valid tool type.";
                yield Resources.ID_NULL;
            }
        };
    }

    @SuppressLint("SwitchIntDef")
    private static @DrawableRes int getIconResForTool(@AutocompleteRequestType int requestType) {
        return switch (requestType) {
            case AutocompleteRequestType.AI_MODE -> R.drawable.search_spark_black_24dp;
            case AutocompleteRequestType.IMAGE_GENERATION -> R.drawable.create_image_24dp;
            case AutocompleteRequestType.DEEP_SEARCH -> R.drawable.travel_explore_24dp;
            case AutocompleteRequestType.CANVAS -> R.drawable.draft_spark_24dp;
            default -> {
                assert false : "AutocompleteRequestType was not a valid tool type.";
                yield Resources.ID_NULL;
            }
        };
    }

    private static void updatePopupTheme(PropertyModel model, FuseboxViewHolder view) {
        @BrandedColorScheme int brandedColorScheme = model.get(FuseboxProperties.COLOR_SCHEME);
        Context context = view.parentView.getContext();

        @StyleRes
        int textAppearance = OmniboxResourceProvider.getPopupButtonTextRes(brandedColorScheme);
        ColorStateList iconTint =
                OmniboxResourceProvider.getPrimaryIconTintList(context, brandedColorScheme);
        for (View button : view.popup.mButtons) {
            themeButton(button, textAppearance, iconTint);
        }

        @StyleRes
        int headerTextAppearance =
                OmniboxResourceProvider.getPopupHeaderVisibilityTextRes(brandedColorScheme);
        for (TextView header : view.popup.mHeaders) {
            header.setTextAppearance(headerTextAppearance);
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
        boolean singleLine = model.get(FuseboxProperties.FUSEBOX_STATE) != FuseboxState.EXPANDED;
        int topToTop = singleLine ? R.id.url_bar : ConstraintSet.UNSET;
        int topToBottom = singleLine ? ConstraintSet.UNSET : R.id.url_bar;
        int bottomToBottom = singleLine ? ConstraintSet.UNSET : ConstraintSet.PARENT_ID;

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
                singleLine ? R.id.action_buttons_segment : R.id.delete_button,
                ConstraintSet.START);

        cs.applyTo(view.parentView);
    }

    private static void updateForCurrentTabFavicon(Bitmap favicon, FuseboxViewHolder viewHolder) {
        Context context = viewHolder.parentView.getContext();
        Resources res = context.getResources();
        FuseboxPopup popup = viewHolder.popup;
        View addCurrentTabButton = popup.mAddCurrentTab;

        Drawable drawable =
                FuseboxTabUtils.getDrawableForTabFavicon(
                        context,
                        favicon,
                        res.getDimensionPixelSize(R.dimen.fusebox_popup_item_icon_size));
        setCustomButtonDrawables(addCurrentTabButton, drawable, null);

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

    /** Helper to retrieve view holder, creating a new one if needed. */
    private static FuseboxItemViewHolder getViewHolder(View view) {
        FuseboxItemViewHolder holder =
                (FuseboxItemViewHolder) view.getTag(R.id.fusebox_view_holder_key);
        if (holder == null) {
            holder = new FuseboxItemViewHolder(view);
            view.setTag(R.id.fusebox_view_holder_key, holder);
        }
        return holder;
    }

    /** View holder to cache frequently accessed views. */
    private static class FuseboxItemViewHolder {
        public final ImageView mActionIcon;
        public final TextView mActionText;
        public final ImageView mActionEndIcon;

        public FuseboxItemViewHolder(View itemView) {
            mActionIcon = itemView.findViewById(R.id.start_icon);
            mActionText = itemView.findViewById(R.id.action_text);
            mActionEndIcon = itemView.findViewById(R.id.end_icon);
        }
    }
}
