// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.translate;

import android.app.Activity;
import android.content.Context;
import android.database.DataSetObserver;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.messages.PrimaryWidgetAppearance;
import org.chromium.components.messages.SecondaryMenuMaxSize;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.Toast;

import java.lang.ref.WeakReference;

/** Manages the translate message UI. */
@JNINamespace("translate")
class TranslateMessage implements TranslateMessageSecondaryMenu.Handler {
    public static class MenuItem {
        // If |title| is an empty string, then a divider will be shown.
        public final String title;
        // If |subtitle| is an empty string, then no subtitle will be shown.
        public final String subtitle;
        public final boolean hasCheckmark;
        public final int overflowMenuItemId;
        public final String languageCode;

        public MenuItem(
                String title,
                String subtitle,
                boolean hasCheckmark,
                int overflowMenuItemId,
                String languageCode) {
            this.title = title;
            this.subtitle = subtitle;
            this.hasCheckmark = hasCheckmark;
            this.overflowMenuItemId = overflowMenuItemId;
            this.languageCode = languageCode;
        }
    }

    private final Context mContext;
    private final MessageDispatcher mMessageDispatcher;
    private final WebContents mWebContents;
    private long mNativeTranslateMessage;
    private final int mDismissalDurationSeconds;

    // Will be null before the message is shown.
    private PropertyModel mMessageProperties;

    /** Shows a Toast with the general translate error message. */
    @CalledByNative
    public static void showTranslateError(WebContents webContents) {
        Context context = getContextFromWebContents(webContents);
        if (context == null) return;
        Toast toast = Toast.makeText(context, R.string.translate_infobar_error, Toast.LENGTH_SHORT);
        toast.show();
    }

    /**
     * Create a new TranslateMessage, or return null if creation failed.
     *
     * Creation could fail in cases where the MessageDispatcher cannot be retrieved, such as when
     * the activity is being recreated or destroyed.
     */
    @CalledByNative
    public static TranslateMessage create(
            WebContents webContents, long nativeTranslateMessage, int dismissalDurationSeconds) {
        Context context = getContextFromWebContents(webContents);
        if (context == null) return null;
        MessageDispatcher messageDispatcher =
                MessageDispatcherProvider.from(webContents.getTopLevelNativeWindow());
        if (messageDispatcher == null) return null;

        return new TranslateMessage(
                context,
                messageDispatcher,
                webContents,
                nativeTranslateMessage,
                dismissalDurationSeconds);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    TranslateMessage(
            @NonNull Context context,
            @NonNull MessageDispatcher messageDispatcher,
            @NonNull WebContents webContents,
            long nativeTranslateMessage,
            int dismissalDurationSeconds) {
        mContext = context;
        mMessageDispatcher = messageDispatcher;
        mWebContents = webContents;
        mNativeTranslateMessage = nativeTranslateMessage;
        mDismissalDurationSeconds = dismissalDurationSeconds;
    }

    @CalledByNative
    public void clearNativePointer() {
        mNativeTranslateMessage = 0;
    }

    @CalledByNative
    public void showMessage(
            String title, String description, String primaryButtonText, boolean hasOverflowMenu) {
        boolean needsDispatch = mMessageProperties == null;
        if (needsDispatch) {
            mMessageProperties =
                    new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                            .with(
                                    MessageBannerProperties.MESSAGE_IDENTIFIER,
                                    MessageIdentifier.TRANSLATE)
                            .with(
                                    MessageBannerProperties.ICON_RESOURCE_ID,
                                    R.drawable.infobar_translate_compact)
                            .with(
                                    MessageBannerProperties.ICON_TINT_COLOR,
                                    MessageBannerProperties.TINT_NONE)
                            .with(
                                    MessageBannerProperties.SECONDARY_MENU_BUTTON_DELEGATE,
                                    new SecondaryMenuButtonDelegate())
                            .with(
                                    MessageBannerProperties.SECONDARY_MENU_MAX_SIZE,
                                    SecondaryMenuMaxSize.LARGE)
                            .with(
                                    MessageBannerProperties.DISMISSAL_DURATION,
                                    mDismissalDurationSeconds)
                            .with(
                                    MessageBannerProperties.ON_PRIMARY_ACTION,
                                    this::handlePrimaryAction)
                            .with(MessageBannerProperties.ON_DISMISSED, this::handleDismiss)
                            .build();
        }

        mMessageProperties.set(MessageBannerProperties.TITLE, title);
        mMessageProperties.set(MessageBannerProperties.DESCRIPTION, description);

        if (TextUtils.isEmpty(primaryButtonText)) {
            mMessageProperties.set(
                    MessageBannerProperties.PRIMARY_WIDGET_APPEARANCE,
                    PrimaryWidgetAppearance.PROGRESS_SPINNER);
        } else {
            mMessageProperties.set(MessageBannerProperties.PRIMARY_BUTTON_TEXT, primaryButtonText);
            mMessageProperties.set(
                    MessageBannerProperties.PRIMARY_WIDGET_APPEARANCE,
                    PrimaryWidgetAppearance.BUTTON_IF_TEXT_IS_SET);
        }

        if (hasOverflowMenu) {
            mMessageProperties.set(
                    MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID, R.drawable.settings_cog);
        }

        if (needsDispatch) {
            mMessageDispatcher.enqueueMessage(
                    mMessageProperties,
                    mWebContents,
                    MessageScopeType.NAVIGATION,
                    /* highPriority= */ false);
        }
    }

    private @PrimaryActionClickBehavior int handlePrimaryAction() {
        if (mNativeTranslateMessage != 0) {
            TranslateMessageJni.get().handlePrimaryAction(mNativeTranslateMessage);
        }
        return PrimaryActionClickBehavior.DO_NOT_DISMISS;
    }

    private void handleDismiss(int dismissReason) {
        mMessageProperties = null;
        if (mNativeTranslateMessage == 0) return;
        TranslateMessageJni.get().handleDismiss(mNativeTranslateMessage, dismissReason);
    }

    @CalledByNative
    public void dismiss() {
        mMessageDispatcher.dismissMessage(mMessageProperties, DismissReason.DISMISSED_BY_FEATURE);
    }

    private static Context getContextFromWebContents(WebContents webContents) {
        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return null;
        WeakReference<Activity> ref = windowAndroid.getActivity();
        if (ref == null) return null;
        return ref.get();
    }

    @CalledByNative
    public static MenuItem[] constructMenuItemArray(
            String[] titles,
            String[] subtitles,
            boolean[] hasCheckmarks,
            int[] overflowMenuItemIds,
            String[] languageCodes) {
        assert titles.length == subtitles.length
                && titles.length == hasCheckmarks.length
                && titles.length == overflowMenuItemIds.length
                && titles.length == languageCodes.length;
        MenuItem[] menuItems = new MenuItem[titles.length];
        for (int i = 0; i < titles.length; ++i) {
            menuItems[i] =
                    new MenuItem(
                            titles[i],
                            subtitles[i],
                            hasCheckmarks[i],
                            overflowMenuItemIds[i],
                            languageCodes[i]);
        }
        return menuItems;
    }

    // TranslateMessageSecondaryMenu.Handler implementation:
    @Override
    public MenuItem[] handleSecondaryMenuItemClicked(MenuItem menuItem) {
        if (mNativeTranslateMessage == 0) return null;
        return TranslateMessageJni.get()
                .handleSecondaryMenuItemClicked(
                        mNativeTranslateMessage,
                        menuItem.overflowMenuItemId,
                        menuItem.languageCode,
                        menuItem.hasCheckmark);
    }

    private final class SecondaryMenuButtonDelegate extends DataSetObserver
            implements ListMenuButtonDelegate {
        /**
         * Keeps track of the RectProvider supplied to anchor the AnchoredPopupWindow to the
         * ListMenuButton. It's kept as a WeakReference so that this doesn't inadvertently extend
         * the lifetime of the RectProvider and all of its references past the time when the popup
         * window is dismissed.
         */
        private WeakReference<RectProvider> mRectProvider;

        // ListMenuButtonDelegate implementation:
        @Override
        public RectProvider getRectProvider(View listMenuButton) {
            RectProvider provider = ListMenuButtonDelegate.super.getRectProvider(listMenuButton);
            mRectProvider = new WeakReference<RectProvider>(provider);
            return provider;
        }

        @Override
        public ListMenu getListMenu() {
            return new TranslateMessageSecondaryMenu(
                    mContext,
                    /* handler= */ TranslateMessage.this,
                    /* dataSetObserver= */ this,
                    mNativeTranslateMessage == 0
                            ? null
                            : TranslateMessageJni.get().buildOverflowMenu(mNativeTranslateMessage));
        }

        // DataSetObserver implementation:
        @Override
        public void onChanged() {
            // If the mRectProvider is set, then call setRect() with the existing Rect in order to
            // force it to notify its observer, which will cause the AnchoredPopupWindow to update
            // its onscreen dimensions to fit the new menu items.
            RectProvider provider = mRectProvider.get();
            if (provider != null) provider.setRect(provider.getRect());
        }
    }

    @NativeMethods
    interface Natives {
        void handlePrimaryAction(long nativeTranslateMessage);

        void handleDismiss(long nativeTranslateMessage, int dismissReason);

        MenuItem[] buildOverflowMenu(long nativeTranslateMessage);

        MenuItem[] handleSecondaryMenuItemClicked(
                long nativeTranslateMessage,
                int overflowMenuItemId,
                String languageCode,
                boolean hadCheckmark);
    }
}
