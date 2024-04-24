// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Java side of native MessageWrapper class that represents a message for native features. */
@JNINamespace("messages")
public final class MessageWrapper implements ListMenu.Delegate {
    private long mNativeMessageWrapper;
    private final PropertyModel mMessageProperties;
    private MessageSecondaryMenuItems mMessageSecondaryMenuItems;

    /**
     * Creates an instance of MessageWrapper and links it with native MessageWrapper object.
     * @param nativeMessageWrapper Pointer to native MessageWrapper.
     * @param messageIdentifier Message identifier of the new message.
     * @return reference to created MessageWrapper.
     */
    @CalledByNative
    @VisibleForTesting
    public static MessageWrapper create(long nativeMessageWrapper, int messageIdentifier) {
        return new MessageWrapper(nativeMessageWrapper, messageIdentifier);
    }

    private MessageWrapper(long nativeMessageWrapper, int messageIdentifier) {
        mNativeMessageWrapper = nativeMessageWrapper;
        mMessageProperties =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(MessageBannerProperties.MESSAGE_IDENTIFIER, messageIdentifier)
                        .with(MessageBannerProperties.ON_PRIMARY_ACTION, this::handleActionClick)
                        .with(
                                MessageBannerProperties.ON_SECONDARY_ACTION,
                                this::handleSecondaryActionClick)
                        .with(MessageBannerProperties.ON_DISMISSED, this::handleMessageDismissed)
                        .build();
    }

    /**
     * Get the {@link PropertyModel} wrapped inside.
     * Note that actions for this property model are linked from the native side creator, so making
     * updates for actions are not advised.
     */
    public PropertyModel getMessageProperties() {
        return mMessageProperties;
    }

    @CalledByNative
    String getTitle() {
        return mMessageProperties.get(MessageBannerProperties.TITLE);
    }

    @CalledByNative
    void setTitle(String title) {
        mMessageProperties.set(MessageBannerProperties.TITLE, title);
    }

    @CalledByNative
    String getDescription() {
        CharSequence description = mMessageProperties.get(MessageBannerProperties.DESCRIPTION);
        return description == null ? null : description.toString();
    }

    @CalledByNative
    void setDescription(CharSequence description) {
        mMessageProperties.set(MessageBannerProperties.DESCRIPTION, description);
    }

    @CalledByNative
    int getDescriptionMaxLines() {
        return mMessageProperties.get(MessageBannerProperties.DESCRIPTION_MAX_LINES);
    }

    @CalledByNative
    void setDescriptionMaxLines(int maxLines) {
        mMessageProperties.set(MessageBannerProperties.DESCRIPTION_MAX_LINES, maxLines);
    }

    @CalledByNative
    String getPrimaryButtonText() {
        return mMessageProperties.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT);
    }

    @CalledByNative
    void setPrimaryButtonText(String primaryButtonText) {
        mMessageProperties.set(MessageBannerProperties.PRIMARY_BUTTON_TEXT, primaryButtonText);
    }

    @CalledByNative
    int getPrimaryButtonTextMaxLines() {
        return mMessageProperties.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT_MAX_LINES);
    }

    @CalledByNative
    void setPrimaryButtonTextMaxLines(int maxLines) {
        mMessageProperties.set(MessageBannerProperties.PRIMARY_BUTTON_TEXT_MAX_LINES, maxLines);
    }

    @CalledByNative
    String getSecondaryButtonMenuText() {
        return mMessageProperties.get(MessageBannerProperties.SECONDARY_BUTTON_MENU_TEXT);
    }

    @CalledByNative
    void setSecondaryButtonMenuText(String secondaryButtonMenuText) {
        mMessageProperties.set(
                MessageBannerProperties.SECONDARY_BUTTON_MENU_TEXT, secondaryButtonMenuText);
    }

    @CalledByNative
    void initializeSecondaryMenu(WindowAndroid windowAndroid, @SecondaryMenuMaxSize int maxSize) {
        Context context = windowAndroid.getActivity().get();
        assert context != null;
        if (mMessageSecondaryMenuItems != null) {
            mMessageProperties.set(MessageBannerProperties.SECONDARY_MENU_MAX_SIZE, maxSize);
            mMessageProperties.set(
                    MessageBannerProperties.SECONDARY_MENU_BUTTON_DELEGATE,
                    () -> mMessageSecondaryMenuItems.createListMenu(context, this));
        }
    }

    @CalledByNative
    PropertyModel addSecondaryMenuItem(int itemId, int resourceId, String itemText) {
        return addSecondaryMenuItem(itemId, resourceId, itemText, itemText);
    }

    @CalledByNative
    PropertyModel addSecondaryMenuItem(
            int itemId, int resourceId, String itemText, String itemDescription) {
        if (mMessageSecondaryMenuItems == null) {
            mMessageSecondaryMenuItems = new MessageSecondaryMenuItems();
        }
        return mMessageSecondaryMenuItems.addMenuItem(
                itemId, resourceId, itemText, itemDescription);
    }

    MessageSecondaryMenuItems getMessageSecondaryMenuItemsForTesting() {
        return mMessageSecondaryMenuItems;
    }

    @CalledByNative
    void clearSecondaryMenuItems() {
        if (mMessageSecondaryMenuItems == null) return;
        mMessageSecondaryMenuItems.clearMenuItems();
    }

    @CalledByNative
    void addSecondaryMenuItemDivider() {
        if (mMessageSecondaryMenuItems == null) return;
        mMessageSecondaryMenuItems.addMenuDivider();
    }

    @CalledByNative
    @DrawableRes
    int getIconResourceId() {
        return mMessageProperties.get(MessageBannerProperties.ICON_RESOURCE_ID);
    }

    @CalledByNative
    void setIconResourceId(@DrawableRes int resourceId) {
        mMessageProperties.set(MessageBannerProperties.ICON_RESOURCE_ID, resourceId);
    }

    @CalledByNative
    boolean isValidIcon() {
        return mMessageProperties.get(MessageBannerProperties.ICON) != null;
    }

    @CalledByNative
    void setIcon(Bitmap iconBitmap) {
        mMessageProperties.set(MessageBannerProperties.ICON, new BitmapDrawable(iconBitmap));
    }

    @CalledByNative
    void setLargeIcon(boolean enabled) {
        mMessageProperties.set(MessageBannerProperties.LARGE_ICON, enabled);
    }

    @CalledByNative
    void setIconRoundedCornerRadius(int radius) {
        mMessageProperties.set(MessageBannerProperties.ICON_ROUNDED_CORNER_RADIUS_PX, radius);
    }

    @CalledByNative
    void disableIconTint() {
        mMessageProperties.set(
                MessageBannerProperties.ICON_TINT_COLOR, MessageBannerProperties.TINT_NONE);
    }

    @CalledByNative
    @DrawableRes
    int getSecondaryIconResourceId() {
        return mMessageProperties.get(MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID);
    }

    @CalledByNative
    void setSecondaryIconResourceId(@DrawableRes int resourceId) {
        mMessageProperties.set(MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID, resourceId);
    }

    @CalledByNative
    void setDuration(long customDuration) {
        mMessageProperties.set(MessageBannerProperties.DISMISSAL_DURATION, customDuration);
    }

    @CalledByNative
    void clearNativePtr() {
        mNativeMessageWrapper = 0;
    }

    @CalledByNative
    Bitmap getIconBitmap() {
        Drawable drawable = mMessageProperties.get(MessageBannerProperties.ICON);
        assert drawable instanceof BitmapDrawable;
        return ((BitmapDrawable) drawable).getBitmap();
    }

    private @PrimaryActionClickBehavior int handleActionClick() {
        if (mNativeMessageWrapper != 0) {
            MessageWrapperJni.get().handleActionClick(mNativeMessageWrapper);
        }
        return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
    }

    private void handleSecondaryActionClick() {
        if (mNativeMessageWrapper == 0) return;
        MessageWrapperJni.get().handleSecondaryActionClick(mNativeMessageWrapper);
    }

    private void handleMessageDismissed(@DismissReason int dismissReason) {
        // mNativeMessageWrapper can be null if the message was dismissed from native API.
        // In this case dismiss callback should have already been called.
        if (mNativeMessageWrapper == 0) return;
        MessageWrapperJni.get().handleDismissCallback(mNativeMessageWrapper, dismissReason);
    }

    @Override
    public void onItemSelected(PropertyModel item) {
        assert item.getAllSetProperties().contains(ListMenuItemProperties.MENU_ITEM_ID);
        int itemId = item.get(ListMenuItemProperties.MENU_ITEM_ID);
        MessageWrapperJni.get().handleSecondaryMenuItemSelected(mNativeMessageWrapper, itemId);
    }

    @NativeMethods
    interface Natives {
        void handleActionClick(long nativeMessageWrapper);

        void handleSecondaryActionClick(long nativeMessageWrapper);

        void handleSecondaryMenuItemSelected(long nativeMessageWrapper, int itemId);

        void handleDismissCallback(long nativeMessageWrapper, @DismissReason int dismissReason);
    }
}
