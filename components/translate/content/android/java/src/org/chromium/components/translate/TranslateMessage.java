// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.translate;

import android.app.Activity;
import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;

import java.lang.ref.WeakReference;

/**
 * Manages the translate message UI.
 */
@JNINamespace("translate")
class TranslateMessage {
    private final WebContents mWebContents;
    private final Context mContext;
    private long mNativeTranslateMessage;
    private final int mDismissalDurationSeconds;
    private final MessageDispatcher mMessageDispatcher;

    // Will be null before the message is shown.
    private PropertyModel mMessageProperties;

    // Shows a Toast with the general translate error message.
    @CalledByNative
    public static void showTranslateError(WebContents webContents) {
        Context context = getContextFromWebContents(webContents);
        if (context == null) return;
        Toast toast = Toast.makeText(context, R.string.translate_infobar_error, Toast.LENGTH_SHORT);
        toast.show();
    }

    @CalledByNative
    public static TranslateMessage create(
            WebContents webContents, long nativeTranslateMessage, int dismissalDurationSeconds) {
        return new TranslateMessage(webContents, nativeTranslateMessage, dismissalDurationSeconds);
    }

    private TranslateMessage(
            WebContents webContents, long nativeTranslateMessage, int dismissalDurationSeconds) {
        mWebContents = webContents;
        mContext = getContextFromWebContents(webContents);
        assert mContext != null;
        mNativeTranslateMessage = nativeTranslateMessage;
        mDismissalDurationSeconds = dismissalDurationSeconds;

        mMessageDispatcher = MessageDispatcherProvider.from(webContents.getTopLevelNativeWindow());
    }

    @CalledByNative
    public void clearNativePointer() {
        mNativeTranslateMessage = 0;
    }

    @CalledByNative
    public void showBeforeTranslateMessage(
            String sourceLanguageDisplayName, String targetLanguageDisplayName) {
        boolean needsDispatch = prepareMessageProperties();

        mMessageProperties.set(MessageBannerProperties.TITLE,
                mContext.getString(R.string.translate_message_before_translate_title));
        mMessageProperties.set(MessageBannerProperties.DESCRIPTION,
                mContext.getString(R.string.translate_message_description,
                        sourceLanguageDisplayName, targetLanguageDisplayName));
        mMessageProperties.set(MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                mContext.getString(R.string.translate_button));

        if (needsDispatch) dispatchMessage();
    }

    @CalledByNative
    public void showTranslationInProgressMessage(
            String sourceLanguageDisplayName, String targetLanguageDisplayName) {
        boolean needsDispatch = prepareMessageProperties();

        mMessageProperties.set(MessageBannerProperties.TITLE,
                mContext.getString(R.string.translate_message_before_translate_title));
        mMessageProperties.set(MessageBannerProperties.DESCRIPTION,
                mContext.getString(R.string.translate_message_description,
                        sourceLanguageDisplayName, targetLanguageDisplayName));
        // TODO(crbug.com/1304118): Once the functionality exists, this should show a progress
        // indicator spinner in place of the primary button.
        mMessageProperties.set(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "");

        if (needsDispatch) dispatchMessage();
    }

    @CalledByNative
    public void showAfterTranslateMessage(
            String sourceLanguageDisplayName, String targetLanguageDisplayName) {
        boolean needsDispatch = prepareMessageProperties();

        mMessageProperties.set(MessageBannerProperties.TITLE,
                mContext.getString(R.string.translate_message_after_translate_title));
        mMessageProperties.set(MessageBannerProperties.DESCRIPTION,
                mContext.getString(R.string.translate_message_description,
                        sourceLanguageDisplayName, targetLanguageDisplayName));
        mMessageProperties.set(
                MessageBannerProperties.PRIMARY_BUTTON_TEXT, mContext.getString(R.string.undo));

        if (needsDispatch) dispatchMessage();
    }

    private boolean prepareMessageProperties() {
        if (mMessageProperties != null) return false;

        mMessageProperties =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.TRANSLATE)
                        .with(MessageBannerProperties.ICON_RESOURCE_ID,
                                R.drawable.infobar_translate_compact)
                        .with(MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID,
                                R.drawable.settings_cog)
                        .with(MessageBannerProperties.DISMISSAL_DURATION, mDismissalDurationSeconds)
                        .with(MessageBannerProperties.ON_PRIMARY_ACTION, this::handlePrimaryAction)
                        .with(MessageBannerProperties.ON_SECONDARY_ACTION,
                                this::handleSecondaryAction)
                        .with(MessageBannerProperties.ON_DISMISSED, this::handleDismiss)
                        .build();

        return true;
    }

    private void dispatchMessage() {
        mMessageDispatcher.enqueueMessage(mMessageProperties, mWebContents,
                MessageScopeType.NAVIGATION, /*highPriority=*/false);
    }

    private @PrimaryActionClickBehavior int handlePrimaryAction() {
        if (mNativeTranslateMessage != 0) {
            TranslateMessageJni.get().handlePrimaryAction(mNativeTranslateMessage);
        }
        return PrimaryActionClickBehavior.DO_NOT_DISMISS;
    }

    private void handleSecondaryAction() {
        // TODO(crbug.com/1304121): Instead of performing an action, the secondary button should
        // show the translate secondary menu.
    }

    private void handleDismiss(int dismissReason) {
        mMessageProperties = null;
        if (mNativeTranslateMessage == 0) return;
        TranslateMessageJni.get().handleDismiss(mNativeTranslateMessage, dismissReason);
    }

    @CalledByNative
    public void dismiss() {
        if (mMessageDispatcher != null) {
            mMessageDispatcher.dismissMessage(
                    mMessageProperties, DismissReason.DISMISSED_BY_FEATURE);
        }
    }

    private static Context getContextFromWebContents(WebContents webContents) {
        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return null;
        WeakReference<Activity> ref = windowAndroid.getActivity();
        if (ref == null) return null;
        return ref.get();
    }

    @NativeMethods
    interface Natives {
        void handlePrimaryAction(long nativeTranslateMessage);
        void handleDismiss(long nativeTranslateMessage, int dismissReason);
    }
}
