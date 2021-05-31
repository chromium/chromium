// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.components.browser_ui.widget.BoundedLinearLayout;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButtonDelegate;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View representing the message banner.
 */
public class MessageBannerView extends BoundedLinearLayout {
    private ImageView mIconView;
    private TextView mTitle;
    private TextView mDescription;
    private TextView mPrimaryButton;
    private ListMenuButton mSecondaryButton;
    private View mDivider;
    private String mSecondaryButtonMenuText;
    private Runnable mSecondaryActionCallback;
    private SwipeGestureListener mSwipeGestureDetector;
    private Runnable mOnTitleChanged;

    public MessageBannerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitle = findViewById(R.id.message_title);
        mDescription = findViewById(R.id.message_description);
        mPrimaryButton = findViewById(R.id.message_primary_button);
        mIconView = findViewById(R.id.message_icon);
        mSecondaryButton = findViewById(R.id.message_secondary_button);
        mDivider = findViewById(R.id.message_divider);
        mSecondaryButton.setOnClickListener((View v) -> { handleSecondaryButtonClick(); });
    }

    void setTitle(String title) {
        mTitle.setText(title);
        if (mOnTitleChanged != null) mOnTitleChanged.run();
    }

    void setDescription(CharSequence description) {
        mDescription.setVisibility(VISIBLE);
        mDescription.setText(description);
    }

    void setDescriptionMaxLines(int maxLines) {
        mDescription.setMaxLines(maxLines);
        mDescription.setEllipsize(TextUtils.TruncateAt.END);
    }

    void setIcon(Drawable icon) {
        mIconView.setImageDrawable(icon);
    }

    void setIconTint(@ColorInt int color) {
        if (color == MessageBannerProperties.TINT_NONE) {
            ApiCompatibilityUtils.setImageTintList(mIconView, null);
        } else {
            ApiCompatibilityUtils.setImageTintList(mIconView, ColorStateList.valueOf(color));
        }
    }

    void setPrimaryButtonText(String text) {
        mPrimaryButton.setVisibility(VISIBLE);
        mPrimaryButton.setText(text);
    }

    void setPrimaryButtonClickListener(OnClickListener listener) {
        mPrimaryButton.setOnClickListener(listener);
    }

    void setSecondaryIcon(Drawable icon) {
        mSecondaryButton.setImageDrawable(icon);
        mSecondaryButton.setVisibility(VISIBLE);
        mDivider.setVisibility(VISIBLE);
    }

    void setSecondaryActionCallback(Runnable callback) {
        mSecondaryActionCallback = callback;
    }

    void setSecondaryButtonMenuText(String text) {
        mSecondaryButtonMenuText = text;
    }

    void setSecondaryIconContentDescription(String description) {
        mSecondaryButton.setContentDescription(description);
    }

    void setSwipeHandler(SwipeHandler handler) {
        mSwipeGestureDetector = new MessageSwipeGestureListener(getContext(), handler);
    }

    void setOnTitleChanged(Runnable runnable) {
        mOnTitleChanged = runnable;
    }

    // TODO(crbug.com/1163302): For the M88 experiment we decided to display single item menu in
    // response to the tap on secondary button. The code below implements this logic. Past M88 it
    // will be replaced with modal dialog driven from the feature code.
    void handleSecondaryButtonClick() {
        if (mSecondaryButtonMenuText == null) {
            if (mSecondaryActionCallback != null) {
                mSecondaryActionCallback.run();
            }
            return;
        }

        final PropertyModel menuItemPropertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, mSecondaryButtonMenuText)
                        .with(ListMenuItemProperties.ENABLED, true)
                        .build();

        MVCListAdapter.ModelList menuItems = new MVCListAdapter.ModelList();
        menuItems.add(new MVCListAdapter.ListItem(
                BasicListMenu.ListMenuItemType.MENU_ITEM, menuItemPropertyModel));

        ListMenu.Delegate listMenuDelegate = (PropertyModel menuItem) -> {
            assert menuItem == menuItemPropertyModel;
            // There is only one menu item in the menu.
            if (mSecondaryActionCallback != null) {
                mSecondaryActionCallback.run();
            }
        };
        BasicListMenu listMenu = new BasicListMenu(getContext(), menuItems, listMenuDelegate);

        ListMenuButtonDelegate delegate = new ListMenuButtonDelegate() {
            @Override
            public ListMenu getListMenu() {
                return listMenu;
            }
        };
        mSecondaryButton.setDelegate(delegate);
        mSecondaryButton.showMenu();
    }

    @SuppressLint("ClickableViewAccessibility")
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (mSwipeGestureDetector != null) {
            return mSwipeGestureDetector.onTouchEvent(event) || super.onTouchEvent(event);
        }
        return super.onTouchEvent(event);
    }

    private class MessageSwipeGestureListener extends SwipeGestureListener {
        public MessageSwipeGestureListener(Context context, SwipeHandler handler) {
            super(context, handler);
        }

        @Override
        public boolean onDown(MotionEvent e) {
            return true;
        }
    }
}
