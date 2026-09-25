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
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.Px;
import androidx.annotation.StyleRes;
import androidx.constraintlayout.widget.ConstraintSet;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.AnchoringMode;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.BackgroundStyle;
import org.chromium.chrome.browser.omnibox.fusebox.PopupButtonData.PopupButtonType;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;
import org.chromium.components.omnibox.IconResourceIdsProto.IconResourceIds;
import org.chromium.components.omnibox.IconResourceIdsProtoIntDef;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;

import java.util.List;

/** Binds the Fusebox properties to the view and component. */
@NullMarked
class FuseboxViewBinder {
    private final OmniboxResourceProvider mResourceProvider;

    public FuseboxViewBinder(OmniboxResourceProvider resourceProvider) {
        mResourceProvider = resourceProvider;
    }

    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    public void bind(PropertyModel model, FuseboxViewHolder view, PropertyKey propertyKey) {
        if (propertyKey == FuseboxProperties.ADAPTER) {
            view.attachmentsView.setAdapter(model.get(FuseboxProperties.ADAPTER));
        } else if (propertyKey == FuseboxProperties.ANCHORING_MODE) {
            reanchorViewsForCompactFusebox(model, view);
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
            updateButtonsStyling(model, view);
        } else if (propertyKey == FuseboxProperties.NAVIGATE_BUTTON_CONTENT_DESCRIPTION) {
            view.navigateButton.setContentDescription(
                    model.get(FuseboxProperties.NAVIGATE_BUTTON_CONTENT_DESCRIPTION));
        } else if (propertyKey == FuseboxProperties.PLUS_BUTTON_BACKGROUND_STYLE) {
            updatePlusButtonVisuals(model, view);
        } else if (propertyKey == FuseboxProperties.PLUS_BUTTON_CLICKED) {
            view.plusButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.PLUS_BUTTON_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.PLUS_BUTTON_VISIBLE) {
            boolean showPlusButton = model.get(FuseboxProperties.PLUS_BUTTON_VISIBLE);
            view.plusButton.setVisibility(showPlusButton ? View.VISIBLE : View.GONE);
        } else if (propertyKey == FuseboxProperties.POPUP_ACCORDION_EXPANDED) {
            view.popup.setAccordionExpanded(model.get(FuseboxProperties.POPUP_ACCORDION_EXPANDED));
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACHMENTS_HEADER_VISIBLE) {
            boolean visible = model.get(FuseboxProperties.POPUP_ATTACHMENTS_HEADER_VISIBLE);
            updateAttachmentsHeaderPadding(model, view);
            updateAttachmentsContainerPadding(model, view);
            view.popup.mAttachmentsHeader.setVisibility(visible ? View.VISIBLE : View.GONE);
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
            setIsEnabledAndReapplyColorFilter(
                    model,
                    FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED,
                    view.popup.mAddCurrentTab);
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_FAVICON) {
            updateForCurrentTabFavicon(model, view);
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE) {
            updateButtonVisibility(
                    model,
                    FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE,
                    view.popup.mAddCurrentTab);
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_DRIVE_CLICKED) {
            view.popup.mDriveButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_ATTACH_DRIVE_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_DRIVE_ENABLED) {
            view.popup.mDriveButton.setEnabled(
                    model.get(FuseboxProperties.POPUP_ATTACH_DRIVE_ENABLED));
        } else if (propertyKey == FuseboxProperties.POPUP_ATTACH_DRIVE_VISIBLE) {
            updateButtonVisibility(
                    model, FuseboxProperties.POPUP_ATTACH_DRIVE_VISIBLE, view.popup.mDriveButton);
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
        } else if (propertyKey == FuseboxProperties.POPUP_MORE_OPTIONS_CLICKED) {
            view.popup.mMoreOptionsButton.setOnClickListener(
                    v -> model.get(FuseboxProperties.POPUP_MORE_OPTIONS_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.POPUP_MORE_OPTIONS_VISIBLE) {
            boolean visible = model.get(FuseboxProperties.POPUP_MORE_OPTIONS_VISIBLE);
            updateButtonVisibility(
                    model,
                    FuseboxProperties.POPUP_MORE_OPTIONS_VISIBLE,
                    view.popup.mMoreOptionsButton);
            view.popup.setAccordionEnabled(visible);
        } else if (propertyKey == FuseboxProperties.POPUP_RECENT_TABS_BUTTON_DATA_LIST) {
            updateRecentTabsButtons(model, view);
        } else if (propertyKey == FuseboxProperties.POPUP_RECENT_TABS_DIVIDER_VISIBLE) {
            view.popup.mRecentTabsDivider.setVisibility(
                    model.get(FuseboxProperties.POPUP_RECENT_TABS_DIVIDER_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (propertyKey == FuseboxProperties.POPUP_RECENT_TABS_ENABLED) {
            ViewGroup container = view.popup.mRecentTabsContainer;
            for (int i = 0; i < container.getChildCount(); i++) {
                View child = container.getChildAt(i);
                setIsEnabledAndReapplyColorFilter(
                        model, FuseboxProperties.POPUP_RECENT_TABS_ENABLED, child);
            }
        } else if (propertyKey == FuseboxProperties.POPUP_RECENT_TABS_HEADER_VISIBLE) {
            view.popup.mRecentTabsHeader.setVisibility(
                    model.get(FuseboxProperties.POPUP_RECENT_TABS_HEADER_VISIBLE)
                            ? View.VISIBLE
                            : View.GONE);
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
        } else if (propertyKey == FuseboxProperties.REQUEST_TYPE_BUTTON_CLICKED) {
            view.requestType.setOnClickListener(
                    v -> model.get(FuseboxProperties.REQUEST_TYPE_BUTTON_CLICKED).run());
        } else if (propertyKey == FuseboxProperties.REQUEST_TYPE_BUTTON_ICON_ID
                || propertyKey == FuseboxProperties.REQUEST_TYPE_BUTTON_SHOULD_TINT_ICON) {
            updateRequestTypeButtonDrawables(model, view);
        } else if (propertyKey == FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT) {
            String text = model.get(FuseboxProperties.REQUEST_TYPE_BUTTON_TEXT);
            Resources res = view.requestType.getResources();
            view.requestType.setText(text);
            view.requestType.setContentDescription(
                    res.getString(R.string.accessibility_omnibox_reset_mode, text));
        } else if (propertyKey == FuseboxProperties.REQUEST_TYPE_BUTTON_VISIBLE) {
            updateButtonVisibility(
                    model, FuseboxProperties.REQUEST_TYPE_BUTTON_VISIBLE, view.requestType);
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
        if (!holder.mHasColor) return;

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
                                .inflate(
                                        R.layout.fusebox_list_item,
                                        group,
                                        /* attachToRoot= */ false);
                group.addView(buttonView, startIndex + i);
            }
            bindDynamicButton(
                    model, view.popup, buttonView, buttonDataList.get(i), brandedColorScheme);
        }
    }

    private static void updateModelButtons(PropertyModel model, FuseboxViewHolder view) {
        List<PopupButtonData> buttonDataList =
                model.get(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST);
        ViewGroup group = view.popup.mAccordionContainer;
        int headerIndex = group.indexOfChild(view.popup.mModelsHeader);
        assert headerIndex >= 0;

        updateButtons(model, view, group, buttonDataList, headerIndex + 1, group.getChildCount());
    }

    private static void updateToolButtons(PropertyModel model, FuseboxViewHolder view) {
        List<PopupButtonData> buttonDataList =
                model.get(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST);
        ViewGroup group = view.popup.mAccordionContainer;
        int headerIndex = group.indexOfChild(view.popup.mToolsHeader);
        assert headerIndex >= 0;
        int dividerIndex = group.indexOfChild(view.popup.mModelsDivider);
        assert dividerIndex >= 0;

        updateButtons(model, view, group, buttonDataList, headerIndex + 1, dividerIndex);
    }

    private static void updateRecentTabsButtons(PropertyModel model, FuseboxViewHolder view) {
        ViewGroup container = view.popup.mRecentTabsContainer;
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
        FuseboxItemViewHolder holder = getViewHolder(buttonView);
        buttonView.setOnClickListener((v) -> data.onClicked.run());
        buttonView.setTooltipText(data.tooltip);
        TextView actionText = holder.mActionText;
        TextView actionSubtext = assumeNonNull(holder.mActionSubtext);
        actionText.setText(data.text);
        actionSubtext.setText(data.subtext);
        actionSubtext.setVisibility(TextUtils.isEmpty(data.subtext) ? View.GONE : View.VISIBLE);
        if (data.type == PopupButtonType.RECENT_TAB) {
            actionText.setMaxLines(1);
            actionText.setEllipsize(TextUtils.TruncateAt.END);
            buttonView.setEnabled(model.get(FuseboxProperties.POPUP_RECENT_TABS_ENABLED));
        } else {
            buttonView.setEnabled(data.enabled);
        }

        Resources res = buttonView.getResources();
        String accessibilityText =
                TextUtils.isEmpty(data.subtext)
                        ? data.text
                        : res.getString(
                                R.string.acc_fusebox_popup_text_with_subtext,
                                data.text,
                                data.subtext);
        CharSequence desc =
                data.selected
                        ? res.getString(
                                R.string.acc_fusebox_popup_button_selected, accessibilityText)
                        : accessibilityText;
        buttonView.setContentDescription(desc);

        @StyleRes
        int textAppearance = OmniboxResourceProvider.getPopupButtonTextRes(brandedColorScheme);
        @StyleRes
        int subtextAppearance =
                OmniboxResourceProvider.getPopupHeaderVisibilityTextRes(brandedColorScheme);
        boolean isBottomSheet = model.get(FuseboxProperties.POPUP_IS_BOTTOM_SHEET);
        ColorStateList iconTint =
                OmniboxResourceProvider.getFuseboxPopupIconTintList(
                        buttonView.getContext(), brandedColorScheme, isBottomSheet);
        ColorStateList iconBackgroundTint =
                OmniboxResourceProvider.getFuseboxPopupIconBackgroundTintList(
                        buttonView.getContext(), brandedColorScheme, isBottomSheet);
        themeButton(buttonView, textAppearance, subtextAppearance, iconTint, iconBackgroundTint);

        @Px
        int iconSize =
                OmniboxResourceProvider.getFuseboxPopupIconSize(
                        buttonView.getContext(), isBottomSheet);

        holder.mHasColor = data.hasColor;
        updateIconSize(holder.mActionIcon, iconSize);
        updateIconSize(holder.mActionEndIcon, iconSize);

        if (data.customIcon != null) {
            var drawable = new BitmapDrawable(res, data.customIcon);
            setCustomButtonDrawables(buttonView, drawable, data.selected);
        } else {
            int iconId = data.iconId == 0 ? IconResourceIds.GLOBE_VALUE : data.iconId;
            @DrawableRes int iconRes = getResIdForIconId(iconId);
            setButtonDrawables(buttonView, iconRes, data.selected);
        }

        if (data.hasColor) {
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
        } else if (iconId == IconResourceIds.IMAGE_CREATE_VALUE) {
            return R.drawable.image_create_24dp;
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
        } else if (iconId == IconResourceIds.ACUTE_VALUE) {
            return R.drawable.acute_24dp;
        }
        return Resources.ID_NULL;
    }

    private static void themeButton(
            View buttonView,
            @StyleRes int textAppearance,
            @StyleRes int subtextAppearance,
            ColorStateList iconTint,
            @Nullable ColorStateList iconBackgroundTint) {
        FuseboxItemViewHolder holder = getViewHolder(buttonView);
        TextView textView = holder.mActionText;
        TextView subtextView = holder.mActionSubtext;
        ImageView imageView = holder.mActionIcon;
        ImageView endImageView = holder.mActionEndIcon;

        if (textView != null) {
            textView.setTextAppearance(textAppearance);
        }
        if (subtextView != null) {
            subtextView.setTextAppearance(subtextAppearance);
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
        if (iconBackground != null && iconBackgroundTint != null) {
            iconBackground.setBackgroundTintList(iconBackgroundTint);
        }
    }

    private void updateButtonsStyling(PropertyModel model, FuseboxViewHolder view) {
        updateNavigateButton(model, view);
        updatePlusButtonVisuals(model, view);
        updatePopupTheme(model, view);
        updateRequestTypeButtonColors(model, view);
        updateRequestTypeButtonDrawables(model, view);
        view.popup.mPopupWindow.setBackgroundDrawable(
                mResourceProvider.getPopupBackgroundDrawable());
    }

    private void updateAttachmentsHeaderPadding(PropertyModel model, FuseboxViewHolder view) {
        Resources resources = view.parentView.getResources();
        int verticalPadding =
                model.get(FuseboxProperties.POPUP_USE_CAROUSEL)
                        ? resources.getDimensionPixelSize(
                                R.dimen.fusebox_attachments_large_header_vertical_padding)
                        : resources.getDimensionPixelSize(
                                R.dimen.fusebox_attachments_small_header_vertical_padding);
        TextView header = view.popup.mAttachmentsHeader;
        header.setPaddingRelative(
                header.getPaddingStart(), verticalPadding, header.getPaddingEnd(), verticalPadding);
    }

    private void updateAttachmentsContainerPadding(PropertyModel model, FuseboxViewHolder view) {
        boolean headerVisible = model.get(FuseboxProperties.POPUP_ATTACHMENTS_HEADER_VISIBLE);
        boolean useCarousel = model.get(FuseboxProperties.POPUP_USE_CAROUSEL);
        int paddingTop =
                (useCarousel && !headerVisible)
                        ? view.parentView
                                .getResources()
                                .getDimensionPixelSize(R.dimen.fusebox_carousel_padding_top)
                        : 0;
        View attachmentsContainer = view.popup.mAttachmentsContainer;
        attachmentsContainer.setPaddingRelative(
                attachmentsContainer.getPaddingStart(),
                paddingTop,
                attachmentsContainer.getPaddingEnd(),
                attachmentsContainer.getPaddingBottom());
    }

    private void updateNavigateButton(PropertyModel model, FuseboxViewHolder view) {
        @BrandedColorScheme int brandedColorScheme = model.get(FuseboxProperties.COLOR_SCHEME);
        Context context = view.parentView.getContext();
        @ColorInt
        int sendIconContrastColor =
                OmniboxResourceProvider.getSendIconContrastColor(context, brandedColorScheme);
        view.navigateButton.getDrawable().setTint(sendIconContrastColor);
        view.navigateButton.setBackground(mResourceProvider.getPopoverNavigateButtonBackground());
        @ColorInt
        int colorPrimary = OmniboxResourceProvider.getColorPrimary(context, brandedColorScheme);
        view.navigateButton.getBackground().setTint(colorPrimary);
    }

    private void updatePlusButtonVisuals(PropertyModel model, FuseboxViewHolder view) {
        Context context = view.parentView.getContext();
        ImageView plusButton = view.plusButton;
        @BrandedColorScheme int brandedColorScheme = model.get(FuseboxProperties.COLOR_SCHEME);
        @BackgroundStyle int style = model.get(FuseboxProperties.PLUS_BUTTON_BACKGROUND_STYLE);

        plusButton.setImageTintList(
                OmniboxResourceProvider.getPrimaryIconTintList(context, brandedColorScheme));
        if (style == BackgroundStyle.ALWAYS_VISIBLE_WIDE) {
            plusButton.setBackground(mResourceProvider.getPopoverPlusButtonBackground());
            // Our drawable implicitly handles corner rounding, while the other background style's
            // drawable needs the outline provider.
            plusButton.setOutlineProvider(null);
        } else {
            Resources resources = context.getResources();
            plusButton.setBackground(
                    OmniboxResourceProvider.getSearchBoxIconBackground(
                            context, brandedColorScheme));
            RoundedCornerOutlineProvider outline =
                    new RoundedCornerOutlineProvider(
                            resources.getDimensionPixelSize(R.dimen.fusebox_button_corner_radius));
            outline.setClipPaddedArea(true);
            plusButton.setOutlineProvider(outline);
        }
    }

    private void updatePopupTheme(PropertyModel model, FuseboxViewHolder view) {
        FuseboxPopup popup = view.popup;
        boolean isBottomSheet = model.get(FuseboxProperties.POPUP_IS_BOTTOM_SHEET);

        ColorStateList iconTint = mResourceProvider.getFuseboxPopupIconTintList(isBottomSheet);
        ColorStateList iconBackgroundTint =
                mResourceProvider.getFuseboxPopupIconBackgroundTintList(isBottomSheet);
        int textAppearance = mResourceProvider.getPopupButtonTextRes();
        @StyleRes int smallTextAppearance = mResourceProvider.getPopupHeaderVisibilityTextRes();

        themeButton(
                popup.mMoreOptionsButton,
                textAppearance,
                smallTextAppearance,
                iconTint,
                iconBackgroundTint);

        for (View button : popup.mAttachmentButtons) {
            themeButton(button, textAppearance, smallTextAppearance, iconTint, iconBackgroundTint);
        }

        for (View button : popup.mDynamicThemedButtons) {
            themeButton(button, textAppearance, smallTextAppearance, iconTint, iconBackgroundTint);
        }

        for (TextView header : popup.mHeaders) {
            header.setTextAppearance(smallTextAppearance);
        }
        popup.mAttachmentsHeader.setTextAppearance(
                mResourceProvider.getPopupAttachmentsHeaderTextRes(
                        model.get(FuseboxProperties.POPUP_USE_CAROUSEL)));

        @ColorInt int dividerLineColor = mResourceProvider.getPopupDividerLineColor();
        for (View divider : popup.mDividers) {
            divider.setBackgroundColor(dividerLineColor);
        }
    }

    private static void updateRequestTypeButtonColors(PropertyModel model, FuseboxViewHolder view) {
        @BrandedColorScheme int brandedColorScheme = model.get(FuseboxProperties.COLOR_SCHEME);
        Context context = view.parentView.getContext();
        @ColorInt
        int buttonColor =
                OmniboxResourceProvider.getRequestTypeButtonColor(context, brandedColorScheme);
        view.requestType.setButtonColor(ColorStateList.valueOf(buttonColor));
        view.requestType.setTextAppearance(
                OmniboxResourceProvider.getRequestTypeButtonTextRes(brandedColorScheme));
    }

    private static void updateRequestTypeButtonDrawables(
            PropertyModel model, FuseboxViewHolder view) {
        @BrandedColorScheme int brandedColorScheme = model.get(FuseboxProperties.COLOR_SCHEME);
        Context context = view.parentView.getContext();
        Resources res = context.getResources();

        @ColorInt
        int colorOnSurface = OmniboxResourceProvider.getColorOnSurface(context, brandedColorScheme);

        @IconResourceIdsProtoIntDef.IconResourceIds
        int iconId = model.get(FuseboxProperties.REQUEST_TYPE_BUTTON_ICON_ID);
        @DrawableRes int startIconRes = getResIdForIconId(iconId);
        Drawable startDrawable =
                startIconRes != Resources.ID_NULL ? context.getDrawable(startIconRes) : null;
        Drawable endDrawable = assumeNonNull(context.getDrawable(R.drawable.btn_close)).mutate();
        if (startDrawable != null
                && model.get(FuseboxProperties.REQUEST_TYPE_BUTTON_SHOULD_TINT_ICON)) {
            startDrawable.mutate().setTint(colorOnSurface);
        }
        endDrawable.setTint(colorOnSurface);

        @Px int iconSizePx = res.getDimensionPixelSize(R.dimen.fusebox_button_icon_size);
        scaleDrawable(startDrawable, iconSizePx);
        scaleDrawable(endDrawable, iconSizePx);

        view.requestType.setCompoundDrawablesRelative(startDrawable, null, endDrawable, null);
    }

    private static void reanchorViewsForCompactFusebox(
            PropertyModel model, FuseboxViewHolder view) {
        long startTime = SystemClock.elapsedRealtime();
        @AnchoringMode int targetMode = model.get(FuseboxProperties.ANCHORING_MODE);

        int topToTop = ConstraintSet.UNSET;
        int topToBottom = ConstraintSet.UNSET;
        int bottomToBottom = ConstraintSet.UNSET;
        int expectedUrlBarEndToStart = R.id.action_buttons_segment;

        switch (targetMode) {
            case AnchoringMode.POPOVER -> {
                topToBottom = R.id.omnibox_suggestions_dropdown;
                bottomToBottom = ConstraintSet.PARENT_ID;
            }
            case AnchoringMode.TOOLBAR_SINGLE_LINE -> {
                topToTop = R.id.url_bar;
            }
            case AnchoringMode.TOOLBAR_MULTI_LINE -> {
                topToBottom = R.id.url_bar;
                bottomToBottom = ConstraintSet.PARENT_ID;
                expectedUrlBarEndToStart = R.id.action_buttons_segment_multimodal;
            }
            default -> {
                assert false : "Unsupported AnchoringMode: " + targetMode;
            }
        }

        ConstraintSet cs = new ConstraintSet();
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

        cs.connect(R.id.url_bar, ConstraintSet.END, expectedUrlBarEndToStart, ConstraintSet.START);

        cs.applyTo(view.parentView);

        FuseboxMetrics.recordReanchorViewsDuration(startTime);
    }

    private static void updateForCurrentTabFavicon(
            PropertyModel model, FuseboxViewHolder viewHolder) {
        Context context = viewHolder.parentView.getContext();
        FuseboxPopup popup = viewHolder.popup;
        View addCurrentTabButton = popup.mAddCurrentTab;
        Bitmap favicon = model.get(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_FAVICON);

        Drawable drawable =
                FuseboxTabUtils.getDrawableForTabFavicon(
                        context,
                        favicon,
                        OmniboxResourceProvider.getFuseboxPopupIconSize(
                                context, model.get(FuseboxProperties.POPUP_IS_BOTTOM_SHEET)));
        setCustomButtonDrawables(addCurrentTabButton, drawable, /* selected= */ false);

        getViewHolder(addCurrentTabButton).mHasColor = favicon != null;
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

    private static void updateIconSize(@Nullable ImageView iconView, @Px int sizePx) {
        if (iconView == null) return;
        ViewGroup.LayoutParams params = iconView.getLayoutParams();
        if (params.width != sizePx || params.height != sizePx) {
            params.width = sizePx;
            params.height = sizePx;
            iconView.setLayoutParams(params);
        }
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
        public final @Nullable TextView mActionSubtext;
        public final ImageView mActionEndIcon;
        public boolean mHasColor;

        public FuseboxItemViewHolder(View itemView) {
            mActionIcon = itemView.findViewById(R.id.start_icon);
            mActionText = itemView.findViewById(R.id.action_text);
            mActionSubtext = itemView.findViewById(R.id.action_subtext);
            mActionEndIcon = itemView.findViewById(R.id.end_icon);
        }
    }
}
