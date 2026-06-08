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
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
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
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxLayoutMode;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.PopupState;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.PopupButtonData;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.PopupButtonType;
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

    private static final int[][] HOVER_STATES =
            new int[][] {
                new int[] {android.R.attr.state_hovered}, new int[] {} // Default, must be last
            };
    ;

    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    public static void bind(PropertyModel model, FuseboxViewHolder view, PropertyKey propertyKey) {
        if (propertyKey == FuseboxProperties.ACTIVATION_CHIP_CLICKED) {
            view.activationChip.setOnClickListener(
                    v -> model.get(FuseboxProperties.ACTIVATION_CHIP_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.ACTIVATION_CHIP_SELECTED) {
            view.activationChip.setSelected(model.get(FuseboxProperties.ACTIVATION_CHIP_SELECTED));
        } else if (propertyKey == FuseboxProperties.ACTIVATION_CHIP_VISIBLE) {
            updateButtonVisibility(
                    model, FuseboxProperties.ACTIVATION_CHIP_VISIBLE, view.activationChip);
        } else if (propertyKey == FuseboxProperties.ADAPTER) {
            view.attachmentsView.setAdapter(model.get(FuseboxProperties.ADAPTER));
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
        } else if (propertyKey == FuseboxProperties.COLOR_SCHEME) {
            updateButtonsVisibilityAndStyling(model, view);
        } else if (propertyKey == FuseboxProperties.FUSEBOX_STATE
                || propertyKey == FuseboxProperties.FUSEBOX_LAYOUT_MODE) {
            reanchorViewsForCompactFusebox(model, view);
        } else if (propertyKey == FuseboxProperties.PLUS_BUTTON_CLICKED) {
            view.plusButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.PLUS_BUTTON_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.PLUS_BUTTON_VISIBLE) {
            updatePlusButton(model, view);
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_CAMERA_CLICKED) {
            view.popup.mCameraButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_ATTACH_CAMERA_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_CAMERA_ENABLED) {
            view.popup.mCameraButton.setEnabled(
                    model.get(FuseboxProperties.POPUP_ATTACH_CAMERA_ENABLED));
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_CAMERA_VISIBLE) {
            updateButtonVisibility(
                    model, FuseboxProperties.POPUP_ATTACH_CAMERA_VISIBLE, view.popup.mCameraButton);
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
        } else if (propertyKey == FuseboxProperties.POPUP_MODEL_HEADER_TEXT) {
            view.popup.mModelsHeader.setText(model.get(FuseboxProperties.POPUP_MODEL_HEADER_TEXT));
        } else if (propertyKey == FuseboxProperties.POPUP_MODEL_HEADER_VISIBLE) {
            view.popup.mModelsHeader.setVisibility(
                    model.get(FuseboxProperties.POPUP_MODEL_HEADER_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (propertyKey == FuseboxProperties.POPUP_RECENT_TABS_BUTTON_DATA_LIST) {
            updateRecentTabsButtons(model, view);
        } else if (propertyKey == FuseboxProperties.POPUP_RECENT_TABS_DIVIDER_VISIBLE) {
            if (view.popup.mRecentTabsDivider != null) {
                view.popup.mRecentTabsDivider.setVisibility(
                        model.get(FuseboxProperties.POPUP_RECENT_TABS_DIVIDER_VISIBLE)
                                ? View.VISIBLE
                                : View.GONE);
            }
        } else if (propertyKey == FuseboxProperties.POPUP_RECENT_TABS_ENABLED) {
            ViewGroup container = view.popup.mRecentTabsContainer;
            if (container != null) {
                boolean enabled = model.get(FuseboxProperties.POPUP_RECENT_TABS_ENABLED);
                for (int i = 0; i < container.getChildCount(); i++) {
                    container.getChildAt(i).setEnabled(enabled);
                }
            }
        } else if (propertyKey == FuseboxProperties.POPUP_RECENT_TABS_HEADER_VISIBLE) {
            if (view.popup.mRecentTabsHeader != null) {
                view.popup.mRecentTabsHeader.setVisibility(
                        model.get(FuseboxProperties.POPUP_RECENT_TABS_HEADER_VISIBLE)
                                ? View.VISIBLE
                                : View.GONE);
            }
        } else if (propertyKey == FuseboxProperties.POPUP_STATE) {
            view.popup.setPopupState(model.get(FuseboxProperties.POPUP_STATE));
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST) {
            updateToolButtons(model, view);
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_DIVIDER_VISIBLE) {
            view.popup.mToolsDivider.setVisibility(
                    model.get(FuseboxProperties.POPUP_TOOL_DIVIDER_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_HEADER_TEXT) {
            view.popup.mToolsHeader.setText(model.get(FuseboxProperties.POPUP_TOOL_HEADER_TEXT));
        } else if (propertyKey == FuseboxProperties.POPUP_TOOL_HEADER_VISIBLE) {
            view.popup.mToolsHeader.setVisibility(
                    model.get(FuseboxProperties.POPUP_TOOL_HEADER_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (propertyKey == FuseboxProperties.REQUEST_TYPE) {
            updateRequestTypeButton(model, view);
            updateButtonsA11yAnnouncements(model, view);
        } else if (propertyKey == FuseboxProperties.REQUEST_TYPE_BUTTON_CLICKED) {
            view.requestType.setOnClickListener(
                    v -> model.get(FuseboxProperties.REQUEST_TYPE_BUTTON_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT) {
            String text = model.get(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT);
            Resources res = view.requestType.getResources();
            view.requestType.setText(text);
            view.requestType.setContentDescription(
                    res.getString(R.string.accessibility_omnibox_reset_mode, text));
        } else if (propertyKey == FuseboxProperties.REQUEST_TYPE_BUTTON_VISIBLE) {
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

    private static void updateButtons(
            PropertyModel model,
            FuseboxViewHolder view,
            ViewGroup group,
            @Nullable List<PopupButtonData> buttonDataList,
            int startIndex,
            int endIndex) {
        int currentCount = endIndex - startIndex;
        int targetCount = buttonDataList == null ? 0 : buttonDataList.size();

        if (currentCount > targetCount) {
            for (int i = startIndex + targetCount; i < endIndex; i++) {
                view.popup.mDynamicThemedButtons.remove(group.getChildAt(i));
            }
            group.removeViews(startIndex + targetCount, currentCount - targetCount);
        }

        if (buttonDataList == null) return;

        @BrandedColorScheme int brandedColorScheme = model.get(FuseboxProperties.COLOR_SCHEME);
        for (int i = 0; i < targetCount; i++) {
            View buttonView;
            if (i < currentCount) {
                buttonView = group.getChildAt(startIndex + i);
            } else {
                buttonView =
                        LayoutInflater.from(group.getContext())
                                .inflate(R.layout.fusebox_list_item, group, false);
                group.addView(buttonView, startIndex + i);
            }
            bindDynamicButton(
                    model, view.popup, buttonView, buttonDataList.get(i), brandedColorScheme);
        }
    }

    private static void updateModelButtons(PropertyModel model, FuseboxViewHolder view) {
        List<PopupButtonData> buttonDataList =
                model.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        ViewGroup group = view.popup.mViewGroup;
        int headerIndex = group.indexOfChild(view.popup.mModelsHeader);
        assert headerIndex >= 0;

        updateButtons(model, view, group, buttonDataList, headerIndex + 1, group.getChildCount());
    }

    private static void updateToolButtons(PropertyModel model, FuseboxViewHolder view) {
        List<PopupButtonData> buttonDataList =
                model.get(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST);
        ViewGroup group = view.popup.mViewGroup;
        int headerIndex = group.indexOfChild(view.popup.mToolsHeader);
        assert headerIndex >= 0;
        int dividerIndex = group.indexOfChild(view.popup.mModelsDivider);
        assert dividerIndex >= 0;

        updateButtons(model, view, group, buttonDataList, headerIndex + 1, dividerIndex);
    }

    private static void updateRecentTabsButtons(PropertyModel model, FuseboxViewHolder view) {
        ViewGroup container = view.popup.mRecentTabsContainer;
        if (container == null) return;

        List<PopupButtonData> buttonDataList =
                model.get(FuseboxProperties.POPUP_RECENT_TABS_BUTTON_DATA_LIST);
        int targetCount = buttonDataList == null ? 0 : buttonDataList.size();
        container.setVisibility(targetCount > 0 ? View.VISIBLE : View.GONE);

        updateButtons(
                model,
                view,
                container,
                buttonDataList,
                /* startIndex= */ 0,
                container.getChildCount());
    }

    private static void bindDynamicButton(
            PropertyModel model,
            FuseboxPopup popup,
            View buttonView,
            PopupButtonData data,
            @BrandedColorScheme int brandedColorScheme) {
        buttonView.setOnClickListener((v) -> data.onClicked.run());
        TextView actionText = (TextView) buttonView.findViewById(R.id.action_text);
        actionText.setText(data.text);
        if (data.type == PopupButtonType.RECENT_TAB) {
            actionText.setMaxLines(1);
            actionText.setEllipsize(TextUtils.TruncateAt.END);
            buttonView.setEnabled(model.get(FuseboxProperties.POPUP_RECENT_TABS_ENABLED));
        } else {
            buttonView.setEnabled(data.enabled);
        }

        Resources res = buttonView.getResources();
        CharSequence desc =
                data.selected
                        ? res.getString(R.string.acc_fusebox_popup_button_selected, data.text)
                        : data.text;
        buttonView.setContentDescription(desc);

        @StyleRes
        int textAppearance = OmniboxResourceProvider.getPopupButtonTextRes(brandedColorScheme);
        ColorStateList iconTint =
                OmniboxResourceProvider.getPrimaryIconTintList(
                        buttonView.getContext(), brandedColorScheme);
        ColorStateList iconBackgroundTint =
                OmniboxResourceProvider.getPrimaryIconBackgroundTintList(
                        buttonView.getContext(), brandedColorScheme);
        themeButton(buttonView, textAppearance, iconTint, iconBackgroundTint);
        if (data.customIcon != null) {
            var drawable = new BitmapDrawable(res, data.customIcon);
            setCustomButtonDrawables(buttonView, drawable, data.selected);
        } else {
            int iconId = data.iconId == 0 ? IconResourceIds.GLOBE_VALUE : data.iconId;
            @DrawableRes int iconRes = getResIdForIconId(iconId);
            setButtonDrawables(buttonView, iconRes, data.selected);
        }

        if (data.hasColor) {
            FuseboxItemViewHolder holder = getViewHolder(buttonView);
            ImageView imageView = holder.mActionIcon;
            if (imageView != null) {
                imageView.setImageTintList(null);
            }
            reapplyColorFilter(buttonView);
            popup.mDynamicThemedButtons.remove(buttonView);
        } else {
            popup.mDynamicThemedButtons.add(buttonView);
        }
    }

    private static void setButtonDrawables(
            View buttonView, @DrawableRes int iconRes, boolean selected) {
        FuseboxItemViewHolder holder = getViewHolder(buttonView);
        ImageView imageView = holder.mActionIcon;

        if (iconRes != Resources.ID_NULL) {
            imageView.setImageResource(iconRes);
            imageView.setVisibility(View.VISIBLE);
        } else {
            imageView.setImageDrawable(null);
            imageView.setVisibility(View.GONE);
        }

        setButtonSelected(holder, selected);
    }

    private static void setButtonSelected(FuseboxItemViewHolder holder, boolean selected) {
        ImageView endImageView = holder.mActionEndIcon;
        if (selected) {
            endImageView.setImageResource(R.drawable.m3_ic_check_24px);
            endImageView.setVisibility(View.VISIBLE);
        } else {
            endImageView.setImageDrawable(null);
            endImageView.setVisibility(View.GONE);
        }
    }

    private static void setCustomButtonDrawables(
            View buttonView, @Nullable Drawable startDrawable, boolean selected) {
        FuseboxItemViewHolder holder = getViewHolder(buttonView);
        ImageView imageView = holder.mActionIcon;

        imageView.setImageDrawable(startDrawable);
        imageView.setVisibility(startDrawable != null ? View.VISIBLE : View.GONE);

        setButtonSelected(holder, selected);
    }

    /** Maps ids found in generated protos to local resources backed drawable ids. */
    private static @DrawableRes int getResIdForIconId(int iconId) {
        if (iconId == IconResourceIds.GLOBE_VALUE) {
            return R.drawable.ic_globe_24dp;
        } else if (iconId == IconResourceIds.SEARCH_LOUPE_WITH_SPARKLE_VALUE) {
            return R.drawable.search_spark_black_24dp;
        } else if (iconId == IconResourceIds.BANANA_VALUE) {
            return R.drawable.create_image_24dp;
        } else if (iconId == IconResourceIds.TRAVEL_EXPLORE_VALUE) {
            return R.drawable.travel_explore_24dp;
        } else if (iconId == IconResourceIds.DRAFT_SPARK_VALUE) {
            return R.drawable.draft_spark_24dp;
        } else if (iconId == IconResourceIds.AUTORENEW_VALUE) {
            return R.drawable.autorenew_24dp;
        } else if (iconId == IconResourceIds.TIMER_VALUE) {
            return R.drawable.ic_timer;
        } else if (iconId == IconResourceIds.BOLT_VALUE) {
            return R.drawable.bolt_24dp;
        } else if (iconId == IconResourceIds.TASK_SPARK_VALUE) {
            return R.drawable.task_spark_24dp;
        }
        return Resources.ID_NULL;
    }

    private static void themeButton(
            View buttonView,
            @StyleRes int textAppearance,
            ColorStateList iconTint,
            ColorStateList iconBackgroundTint) {
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

        // The icon background is only present for horizontal attachments, so null-checking is
        // necessary.
        View iconBackground = buttonView.findViewById(R.id.start_icon_background);
        if (iconBackground != null) {
            iconBackground.setBackgroundTintList(iconBackgroundTint);
        }
    }

    private static void updateButtonsA11yAnnouncements(
            PropertyModel model, FuseboxViewHolder view) {
        @StringRes
        int navButtonAccessibilityStringRes = R.string.acc_send_button_search_or_navigate;
        switch (model.get(FuseboxProperties.REQUEST_TYPE)) {
            case AutocompleteRequestType.AI_MODE:
                navButtonAccessibilityStringRes = R.string.acc_send_button_send_to_ai;
                break;
            case AutocompleteRequestType.IMAGE_GENERATION:
                navButtonAccessibilityStringRes = R.string.acc_send_button_create_image;
                break;
            case AutocompleteRequestType.DEEP_SEARCH:
                navButtonAccessibilityStringRes = R.string.ntp_compose_deep_search;
                break;
            case AutocompleteRequestType.CANVAS:
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
    }

    private static void updateButtonsVisibilityAndStyling(
            PropertyModel model, FuseboxViewHolder view) {
        updatePlusButton(model, view);
        updateNavigateButton(model, view);
        updateRequestTypeButton(model, view);
        updatePopupTheme(model, view);
        updateActivationChip(model, view);
        Context context = view.parentView.getContext();
        @BrandedColorScheme int brandedColorScheme = model.get(FuseboxProperties.COLOR_SCHEME);
        Drawable background =
                OmniboxResourceProvider.getPopupBackgroundDrawable(context, brandedColorScheme);
        view.popup.mPopupWindow.setBackgroundDrawable(background);
    }

    private static void updatePlusButton(PropertyModel model, FuseboxViewHolder view) {
        boolean showPlusButton = model.get(FuseboxProperties.PLUS_BUTTON_VISIBLE);
        ChromeImageView plusButton = view.plusButton;
        plusButton.setVisibility(showPlusButton ? View.VISIBLE : View.GONE);
        if (showPlusButton) {
            @BrandedColorScheme int brandedColorScheme = model.get(FuseboxProperties.COLOR_SCHEME);
            Context context = view.parentView.getContext();
            plusButton.setBackground(
                    OmniboxResourceProvider.getSearchBoxIconBackground(
                            context, brandedColorScheme));
            plusButton.setImageTintList(
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
        if (!model.get(FuseboxProperties.REQUEST_TYPE_BUTTON_VISIBLE)) {
            view.requestType.setVisibility(View.GONE);
            return;
        }

        @AutocompleteRequestType int requestType = model.get(FuseboxProperties.REQUEST_TYPE);
        if (!ToolModeUtils.isAimRequest(requestType)) {
            // The model is in an inconsistent state, wait for the next event.
            return;
        }

        @BrandedColorScheme int brandedColorScheme = model.get(FuseboxProperties.COLOR_SCHEME);
        Context context = view.parentView.getContext();
        Resources res = context.getResources();

        @ColorInt
        int colorOnSurface = OmniboxResourceProvider.getColorOnSurface(context, brandedColorScheme);

        Drawable startDrawable = context.getDrawable(getIconResForTool(requestType));
        Drawable endDrawable = assumeNonNull(context.getDrawable(R.drawable.btn_close)).mutate();
        @ColorInt
        int buttonColor =
                OmniboxResourceProvider.getRequestTypeButtonColor(context, brandedColorScheme);
        if (requestType != AutocompleteRequestType.IMAGE_GENERATION) {
            assumeNonNull(startDrawable).mutate().setTint(colorOnSurface);
        }
        endDrawable.setTint(colorOnSurface);

        @Px int iconSizePx = res.getDimensionPixelSize(R.dimen.fusebox_button_icon_size);
        scaleDrawable(startDrawable, iconSizePx);
        scaleDrawable(endDrawable, iconSizePx);

        ButtonCompat button = view.requestType;
        button.setVisibility(View.VISIBLE);
        button.setButtonColor(ColorStateList.valueOf(buttonColor));
        button.setTextAppearance(
                OmniboxResourceProvider.getRequestTypeButtonTextRes(brandedColorScheme));
        button.setCompoundDrawablesRelative(startDrawable, null, endDrawable, null);
    }

    private static void updateActivationChip(
            PropertyModel propertyModel, FuseboxViewHolder viewHolder) {
        Context context = viewHolder.parentView.getContext();
        @BrandedColorScheme
        int brandedColorScheme = propertyModel.get(FuseboxProperties.COLOR_SCHEME);
        @ColorInt
        int buttonColor =
                OmniboxResourceProvider.getColorSurfaceContainerHigh(context, brandedColorScheme);
        @ColorInt
        int buttonColorHovered =
                OmniboxResourceProvider.getColorSurfaceContainerHighest(
                        context, brandedColorScheme);
        int[] backgroundColors = new int[] {buttonColorHovered, buttonColor};

        ButtonCompat button = viewHolder.activationChip;
        button.setButtonColor(new ColorStateList(HOVER_STATES, backgroundColors));

        @ColorInt
        int colorOnSurface = OmniboxResourceProvider.getColorOnSurface(context, brandedColorScheme);
        button.setCompoundDrawableTintList(ColorStateList.valueOf(colorOnSurface));
        button.setForegroundTintList(ColorStateList.valueOf(colorOnSurface));
        button.setTextColor(colorOnSurface);
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

        ColorStateList iconTint =
                OmniboxResourceProvider.getPrimaryIconTintList(context, brandedColorScheme);
        ColorStateList iconBackgroundTint =
                OmniboxResourceProvider.getPrimaryIconBackgroundTintList(
                        context, brandedColorScheme);
        @StyleRes
        int dynamicTextAppearance =
                OmniboxResourceProvider.getPopupButtonTextRes(brandedColorScheme);

        for (View button : view.popup.mAttachmentButtons) {
            @StyleRes int attachmentTextAppearance;
            if (Integer.valueOf(PopupState.BOTTOM).equals(model.get(FuseboxProperties.POPUP_STATE))
                    && view.popup.mAttachmentButtons.contains(button)) {
                attachmentTextAppearance =
                        OmniboxResourceProvider.getAttachmentButtonTextRes(brandedColorScheme);
            } else {
                attachmentTextAppearance = dynamicTextAppearance;
            }
            themeButton(button, attachmentTextAppearance, iconTint, iconBackgroundTint);
        }
        for (View button : view.popup.mDynamicThemedButtons) {
            themeButton(button, dynamicTextAppearance, iconTint, iconBackgroundTint);
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
        int topToTop;
        int topToBottom;
        int bottomToBottom;

        if (model.get(FuseboxProperties.FUSEBOX_LAYOUT_MODE)
                == FuseboxLayoutMode.SUGGESTIONS_POPOVER) {
            topToTop = ConstraintSet.UNSET;
            topToBottom = R.id.omnibox_suggestions_dropdown;
            bottomToBottom = ConstraintSet.PARENT_ID;
        } else {
            topToTop = singleLine ? R.id.url_bar : ConstraintSet.UNSET;
            topToBottom = singleLine ? ConstraintSet.UNSET : R.id.url_bar;
            bottomToBottom = singleLine ? ConstraintSet.UNSET : ConstraintSet.PARENT_ID;
        }

        var cs = new ConstraintSet();
        cs.clone(view.parentView);

        int id = view.plusButton.getId();
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
                singleLine ? R.id.action_buttons_segment : R.id.action_buttons_segment_multimodal,
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
        setCustomButtonDrawables(addCurrentTabButton, drawable, /* selected= */ false);

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
