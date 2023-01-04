// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.translate;

import android.content.Context;
import android.graphics.Rect;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.ListPopupWindow;
import android.widget.PopupWindow;
import android.widget.TextView;

import androidx.core.content.ContextCompat;

import java.util.ArrayList;
import java.util.List;

/**
 * A Helper class for managing the Translate Overflow Menu.
 */
public class TranslateMenuHelper implements AdapterView.OnItemClickListener {
    private final TranslateMenuListener mMenuListener;
    private final TranslateOptions mOptions;

    private ContextThemeWrapper mContextWrapper;
    private TranslateMenuAdapter mAdapter;
    private View mAnchorView;
    private ListPopupWindow mPopup;
    private boolean mIsIncognito;
    private boolean mIsSourceLangUnknown;

    /**
     * Interface for receiving the click event of menu item.
     */
    public interface TranslateMenuListener {
        void onOverflowMenuItemClicked(int itemId);
        void onTargetMenuItemClicked(String code);
        void onSourceMenuItemClicked(String code);
    }

    public TranslateMenuHelper(Context context, View anchorView, TranslateOptions options,
            TranslateMenuListener itemListener, boolean isIncognito, boolean isSourceLangUnknown) {
        mContextWrapper = new ContextThemeWrapper(context, R.style.OverflowMenuThemeOverlay);
        mAnchorView = anchorView;
        mOptions = options;
        mMenuListener = itemListener;
        mIsIncognito = isIncognito;
        mIsSourceLangUnknown = isSourceLangUnknown;
    }

    // Helper method for deciding if a language should be skipped from the list
    // in case it's a source or target language for the corresponding language list.
    private boolean shouldBeSkippedFromList(int menuType, String code) {
        // Avoid source language in the source language list.
        if (menuType == TranslateMenu.MENU_SOURCE_LANGUAGE
                && code.equals(mOptions.sourceLanguageCode())) {
            return true;
        }
        // Avoid target language in the target language list.
        if (menuType == TranslateMenu.MENU_TARGET_LANGUAGE
                && code.equals(mOptions.targetLanguageCode())) {
            return true;
        }
        return false;
    }
    /**
     *
     * Build translate menu by menu type.
     */
    private List<TranslateMenu.MenuItem> getMenuList(int menuType) {
        List<TranslateMenu.MenuItem> menuList = new ArrayList<TranslateMenu.MenuItem>();
        if (menuType == TranslateMenu.MENU_OVERFLOW) {
            // TODO(googleo): Add language short list above static menu after its data is ready.
            menuList.addAll(TranslateMenu.getOverflowMenu(mIsIncognito, mIsSourceLangUnknown));
        } else {
            int contentLanguagesCount = 0;
            if (TranslateFeatureList.isEnabled(
                        TranslateFeatureList.CONTENT_LANGUAGES_IN_LANGUAGE_PICKER)
                    && menuType == TranslateMenu.MENU_TARGET_LANGUAGE
                    && mOptions.contentLanguages() != null) {
                contentLanguagesCount = mOptions.contentLanguages().length;
                // If false it means that the list is not empty and the last element should be
                // skipped from the list, meaning the second to last should have a divider.
                boolean lastHasDivider = contentLanguagesCount > 0
                        && !(shouldBeSkippedFromList(
                                menuType, mOptions.contentLanguages()[contentLanguagesCount - 1]));

                for (int i = 0; i < contentLanguagesCount; ++i) {
                    String code = mOptions.contentLanguages()[i];
                    if (shouldBeSkippedFromList(menuType, code)) {
                        continue;
                    }
                    menuList.add(
                            new TranslateMenu.MenuItem(TranslateMenu.ITEM_CONTENT_LANGUAGE, i, code,
                                    (i == contentLanguagesCount - 1 && lastHasDivider
                                            || i == contentLanguagesCount - 2 && !lastHasDivider)));
                }

                // Keeps track how many were added.
                contentLanguagesCount = menuList.size();
            }
            for (int i = 0; i < mOptions.allLanguages().size(); ++i) {
                // "Detected Language" is the first item in the languages list and should only be
                // added to the source language menu.
                if (i == 0 && menuType == TranslateMenu.MENU_TARGET_LANGUAGE) {
                    continue;
                }
                String code = mOptions.allLanguages().get(i).mLanguageCode;
                if (shouldBeSkippedFromList(menuType, code)) {
                    continue;
                }
                // Subtract 1 from item IDs if skipping the "Detected Language" option.
                int itemID = menuType == TranslateMenu.MENU_TARGET_LANGUAGE
                        ? contentLanguagesCount + i - 1
                        : contentLanguagesCount + i;
                menuList.add(new TranslateMenu.MenuItem(TranslateMenu.ITEM_LANGUAGE, itemID, code));
            }
        }
        return menuList;
    }

    /**
     * Content languages are the only mutable property of translate options.
     * Refresh menu when they change.
     */
    public void onContentLanguagesChanged(String[] codes) {
        mOptions.updateContentLanguages(codes);
        mAdapter.refreshMenu(TranslateMenu.MENU_TARGET_LANGUAGE);
    }

    /**
     * Show the overflow menu.
     * @param menuType The type of overflow menu to show.
     * @param maxwidth Maximum width of menu.  Set to 0 when not specified.
     */
    public void show(int menuType, int maxWidth) {
        if (mPopup == null) {
            mPopup = new ListPopupWindow(mContextWrapper, null, android.R.attr.popupMenuStyle);
            mPopup.setModal(true);
            mPopup.setAnchorView(mAnchorView);
            mPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);

            // Need to explicitly set the background here.  Relying on it being set in the style
            // caused an incorrectly drawn background.
            // TODO(martiw): We might need a new menu background here.
            mPopup.setBackgroundDrawable(
                    ContextCompat.getDrawable(mContextWrapper, R.drawable.menu_bg_tinted));

            mPopup.setOnItemClickListener(this);

            // The menu must be shifted down by the height of the anchor view in order to be
            // displayed over and above it.
            int anchorHeight = mAnchorView.getHeight();
            // Setting a positive offset here shifts the menu down.
            mPopup.setVerticalOffset(anchorHeight);

            mAdapter = new TranslateMenuAdapter(menuType);
            mPopup.setAdapter(mAdapter);
        } else {
            mAdapter.refreshMenu(menuType);
        }

        if (menuType == TranslateMenu.MENU_OVERFLOW) {
            // Use measured width when it is a overflow menu.
            Rect bgPadding = new Rect();
            mPopup.getBackground().getPadding(bgPadding);
            int measuredWidth = measureMenuWidth(mAdapter) + bgPadding.left + bgPadding.right;
            mPopup.setWidth((maxWidth > 0 && measuredWidth > maxWidth) ? maxWidth : measuredWidth);
        } else {
            // Use fixed width otherwise.
            int popupWidth = mContextWrapper.getResources().getDimensionPixelSize(
                    R.dimen.infobar_translate_menu_width);
            mPopup.setWidth(popupWidth);
        }

        // When layout is RTL, set the horizontal offset to align the menu with the left side of the
        // screen.
        if (mAnchorView.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL) {
            int[] tempLocation = new int[2];
            mAnchorView.getLocationOnScreen(tempLocation);
            mPopup.setHorizontalOffset(-tempLocation[0]);
        }

        if (!mPopup.isShowing()) {
            mPopup.show();
            mPopup.getListView().setItemsCanFocus(true);
        }
    }

    private int measureMenuWidth(TranslateMenuAdapter adapter) {
        final int widthMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        final int heightMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        final int count = adapter.getCount();
        int width = 0;
        int itemType = 0;
        View itemView = null;
        for (int i = 0; i < count; i++) {
            final int positionType = adapter.getItemViewType(i);
            if (positionType != itemType) {
                itemType = positionType;
                itemView = null;
            }
            itemView = adapter.getView(i, itemView, null);
            itemView.measure(widthMeasureSpec, heightMeasureSpec);
            width = Math.max(width, itemView.getMeasuredWidth());
        }
        return width;
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        dismiss();

        TranslateMenu.MenuItem item = mAdapter.getItem(position);
        switch (mAdapter.mMenuType) {
            case TranslateMenu.MENU_OVERFLOW:
                mMenuListener.onOverflowMenuItemClicked(item.mId);
                return;
            case TranslateMenu.MENU_TARGET_LANGUAGE:
                mMenuListener.onTargetMenuItemClicked(item.mCode);
                return;
            case TranslateMenu.MENU_SOURCE_LANGUAGE:
                mMenuListener.onSourceMenuItemClicked(item.mCode);
                return;
            default:
                assert false : "Unsupported Menu Item Id";
        }
    }

    /**
     * Dismisses the translate option menu.
     */
    public void dismiss() {
        if (isShowing()) {
            mPopup.dismiss();
        }
    }

    /**
     * @return Whether the menu is currently showing.
     */
    public boolean isShowing() {
        if (mPopup == null) {
            return false;
        }
        return mPopup.isShowing();
    }

    /**
     * The provides the views of the menu items and dividers.
     */
    private final class TranslateMenuAdapter extends ArrayAdapter<TranslateMenu.MenuItem> {
        private final LayoutInflater mInflater;
        private int mMenuType;

        public TranslateMenuAdapter(int menuType) {
            super(mContextWrapper, R.layout.translate_menu_item, getMenuList(menuType));
            mInflater = LayoutInflater.from(mContextWrapper);
            mMenuType = menuType;
        }

        private void refreshMenu(int menuType) {
            // MENU_OVERFLOW is static and it should not reload.
            if (menuType == TranslateMenu.MENU_OVERFLOW) return;

            clear();

            mMenuType = menuType;
            addAll(getMenuList(menuType));
            notifyDataSetChanged();
        }

        private String getItemViewText(TranslateMenu.MenuItem item) {
            if (mMenuType == TranslateMenu.MENU_OVERFLOW) {
                // Overflow menu items are manually defined one by one.
                String source = mOptions.sourceLanguageName();
                switch (item.mId) {
                    case TranslateMenu.ID_OVERFLOW_ALWAYS_TRANSLATE:
                        return mContextWrapper.getString(
                                R.string.translate_option_always_translate, source);
                    case TranslateMenu.ID_OVERFLOW_MORE_LANGUAGE:
                        return mContextWrapper.getString(R.string.translate_option_more_language);
                    case TranslateMenu.ID_OVERFLOW_NEVER_SITE:
                        return mContextWrapper.getString(R.string.translate_never_translate_site);
                    case TranslateMenu.ID_OVERFLOW_NEVER_LANGUAGE:
                        return mContextWrapper.getString(
                                R.string.translate_option_never_translate, source);
                    case TranslateMenu.ID_OVERFLOW_NOT_THIS_LANGUAGE:
                        return mContextWrapper.getString(
                                R.string.translate_option_not_source_language, source);
                    default:
                        assert false : "Unexpected Overflow Item Id";
                }
            } else {
                // Get source and target language menu items text by language code.
                return mOptions.getRepresentationFromCode(item.mCode);
            }
            return "";
        }

        @Override
        public int getItemViewType(int position) {
            return getItem(position).mType;
        }

        @Override
        public int getViewTypeCount() {
            return TranslateMenu.MENU_ITEM_TYPE_COUNT;
        }

        private View getItemView(
                View menuItemView, int position, ViewGroup parent, int resourceId) {
            if (menuItemView == null) {
                menuItemView = mInflater.inflate(resourceId, parent, false);
            }
            ((TextView) menuItemView.findViewById(R.id.menu_item_text))
                    .setText(getItemViewText(getItem(position)));
            return menuItemView;
        }

        private View getExtendedItemView(
                View menuItemView, int position, ViewGroup parent, int resourceId) {
            if (menuItemView == null) {
                menuItemView = mInflater.inflate(resourceId, parent, false);
            }
            TranslateMenu.MenuItem item = getItem(position);
            ((TextView) menuItemView.findViewById(R.id.menu_item_text))
                    .setText(getItemViewText(item));
            ((TextView) menuItemView.findViewById(R.id.menu_item_secondary_text))
                    .setText(mOptions.getNativeRepresentationFromCode(item.mCode));

            int dividerVisibility = item.mWithDivider ? View.VISIBLE : View.GONE;
            menuItemView.findViewById(R.id.menu_item_list_divider).setVisibility(dividerVisibility);
            return menuItemView;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            View menuItemView = convertView;
            switch (getItemViewType(position)) {
                case TranslateMenu.ITEM_CHECKBOX_OPTION:
                    menuItemView = getItemView(
                            menuItemView, position, parent, R.layout.translate_menu_item_checked);

                    ImageView checkboxIcon = menuItemView.findViewById(R.id.menu_item_icon);
                    if (getItem(position).mId == TranslateMenu.ID_OVERFLOW_ALWAYS_TRANSLATE
                            && mOptions.getTranslateState(TranslateOptions.Type.ALWAYS_LANGUAGE)) {
                        checkboxIcon.setVisibility(View.VISIBLE);
                    } else if (getItem(position).mId == TranslateMenu.ID_OVERFLOW_NEVER_LANGUAGE
                            && mOptions.getTranslateState(TranslateOptions.Type.NEVER_LANGUAGE)) {
                        checkboxIcon.setVisibility(View.VISIBLE);
                    } else if (getItem(position).mId == TranslateMenu.ID_OVERFLOW_NEVER_SITE
                            && mOptions.getTranslateState(TranslateOptions.Type.NEVER_DOMAIN)) {
                        checkboxIcon.setVisibility(View.VISIBLE);
                    } else {
                        checkboxIcon.setVisibility(View.INVISIBLE);
                    }

                    View divider = (View) menuItemView.findViewById(R.id.menu_item_divider);
                    if (getItem(position).mWithDivider) {
                        divider.setVisibility(View.VISIBLE);
                    }
                    break;
                case TranslateMenu.ITEM_CONTENT_LANGUAGE:
                    menuItemView = getExtendedItemView(
                            menuItemView, position, parent, R.layout.translate_menu_extended_item);
                    break;
                case TranslateMenu.ITEM_LANGUAGE:
                    menuItemView = getItemView(
                            menuItemView, position, parent, R.layout.translate_menu_item);
                    break;
                default:
                    assert false : "Unexpected MenuItem type";
            }
            return menuItemView;
        }
    }
}
