// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.infobars;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.infobar.ActionType;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The base class for all InfoBar classes.
 * Note that infobars expire by default when a new navigation occurs.
 * Make sure to use setExpireOnNavigation(false) if you want an infobar to be sticky.
 */
@JNINamespace("infobars")
public abstract class InfoBar implements InfoBarInteractionHandler, InfoBarUiItem {
    /** Interface for InfoBar to interact with its container. */
    public interface Container {
        /** @return True if the infobar is in front. */
        boolean isFrontInfoBar(InfoBar infoBar);

        /**
         * Remove the infobar from its container.
         * @param infoBar InfoBar to remove from the View hierarchy.
         */
        void removeInfoBar(InfoBar infoBar);

        /** Notifies that an infobar's View ({@link InfoBar#getView}) has changed. */
        void notifyInfoBarViewChanged();

        /** @return True if the container's destroy() method has been called. */
        boolean isDestroyed();
    }

    private final int mIconDrawableId;
    private final Bitmap mIconBitmap;
    private final @ColorRes int mIconTintId;
    private final CharSequence mMessage;

    private @Nullable Container mContainer;
    private @Nullable View mView;
    private @Nullable Context mContext;

    private boolean mIsDismissed;
    private boolean mControlsEnabled = true;

    private @Nullable PropertyModel mModel;

    // This points to the InfoBarAndroid class not any of its subclasses.
    private long mNativeInfoBarPtr;

    /**
     * Constructor for regular infobars.
     * @param iconDrawableId ID of the resource to use for the Icon.  If 0, no icon will be shown.
     * @param iconTintId The {@link ColorRes} used as tint for the {@code iconDrawableId}.
     * @param message The message to show in the infobar.
     * @param iconBitmap Icon to draw, in bitmap form.  Used mainly for generated icons.
     */
    public InfoBar(
            int iconDrawableId, @ColorRes int iconTintId, CharSequence message, Bitmap iconBitmap) {
        mIconDrawableId = iconDrawableId;
        mIconBitmap = iconBitmap;
        mIconTintId = iconTintId;
        mMessage = message;
    }

    /**
     * Stores a pointer to the native-side counterpart of this InfoBar.
     * @param nativeInfoBarPtr Pointer to the native InfoBarAndroid, not to its subclass.
     */
    @CalledByNative
    private final void setNativeInfoBar(long nativeInfoBarPtr) {
        assert nativeInfoBarPtr != 0;
        mNativeInfoBarPtr = nativeInfoBarPtr;
    }

    @CalledByNative
    protected void resetNativeInfoBar() {
        mNativeInfoBarPtr = 0;
    }

    /** Sets the Context used when creating the InfoBar. */
    public void setContext(Context context) {
        mContext = context;
    }

    /**
     * @return The {@link Context} used to create the InfoBar. This will be null before the InfoBar
     *         is added to an {@link InfoBarContainer}, or after the InfoBar is closed.
     */
    @Nullable
    protected Context getContext() {
        return mContext;
    }

    /**
     * Creates the View that represents the InfoBar.
     * @return The View representing the InfoBar.
     */
    public final View createView() {
        assert mContext != null;

        if (usesCompactLayout()) {
            InfoBarCompactLayout layout =
                    new InfoBarCompactLayout(
                            mContext, this, mIconDrawableId, mIconTintId, mIconBitmap);
            createCompactLayoutContent(layout);
            mView = layout;
        } else {
            InfoBarLayout layout =
                    new InfoBarLayout(
                            mContext, this, mIconDrawableId, mIconTintId, mIconBitmap, mMessage);
            createContent(layout);
            layout.onContentCreated();
            mView = layout;
        }

        return mView;
    }

    /** @return The model for this infobar if one was created. */
    @Nullable
    PropertyModel getModel() {
        return mModel;
    }

    /**
     * If this returns true, the infobar contents will be replaced with a one-line layout.
     * When overriding this, also override {@link #getAccessibilityMessage}.
     */
    protected boolean usesCompactLayout() {
        return false;
    }

    /**
     * Prepares the InfoBar for display and adds InfoBar-specific controls to the layout.
     * @param layout Layout containing all of the controls.
     */
    protected void createContent(InfoBarLayout layout) {}

    /**
     * Prepares and inserts views into an {@link InfoBarCompactLayout}.
     * {@link #usesCompactLayout} must return 'true' for this function to be called.
     * @param layout Layout to plug views into.
     */
    protected void createCompactLayoutContent(InfoBarCompactLayout layout) {}

    /**
     * Replaces the View currently shown in the infobar with the given View. Triggers the swap
     * animation via the InfoBarContainer.
     */
    protected void replaceView(View newView) {
        mView = newView;
        mContainer.notifyInfoBarViewChanged();
    }

    /** Returns the View shown in this infobar. Only valid after createView() has been called. */
    @Override
    public View getView() {
        return mView;
    }

    /**
     * Returns the accessibility message to announce when this infobar is first shown.
     * Override this if the InfoBar doesn't have {@link R.id.infobar_message}. It is usually the
     * case when it is in CompactLayout.
     */
    protected CharSequence getAccessibilityMessage(CharSequence defaultTitle) {
        return defaultTitle == null ? "" : defaultTitle;
    }

    @Override
    public CharSequence getAccessibilityText() {
        if (mView == null) return "";

        CharSequence title = null;
        TextView messageView = (TextView) mView.findViewById(R.id.infobar_message);
        if (messageView != null) {
            title = messageView.getText();
        }
        title = getAccessibilityMessage(title);
        if (title.length() > 0) {
            title = title + " ";
        }
        // TODO(crbug.com/41349249): Avoid string concatenation due to i18n.
        return title + mContext.getString(R.string.bottom_bar_screen_position);
    }

    @Override
    public int getPriority() {
        return InfoBarPriority.PAGE_TRIGGERED;
    }

    @Override
    @InfoBarIdentifier
    public int getInfoBarIdentifier() {
        if (mNativeInfoBarPtr == 0) return InfoBarIdentifier.INVALID;
        return InfoBarJni.get().getInfoBarIdentifier(mNativeInfoBarPtr, InfoBar.this);
    }

    /** @return whether the infobar actually needed closing. */
    @CalledByNative
    private boolean closeInfoBar() {
        if (!mIsDismissed) {
            mIsDismissed = true;
            if (!mContainer.isDestroyed()) {
                // If the container was destroyed, it's already been emptied of all its infobars.
                onStartedHiding();
                mContainer.removeInfoBar(this);
            }
            mContainer = null;
            mView = null;
            mContext = null;
            return true;
        }
        return false;
    }

    /**
     * @return If the infobar is the front infobar (i.e. visible and not hidden behind other
     *         infobars).
     */
    public boolean isFrontInfoBar() {
        return mContainer.isFrontInfoBar(this);
    }

    /**
     * Called just before the Java infobar has begun hiding.  Give the chance to clean up any child
     * UI that may remain open.
     */
    protected void onStartedHiding() {}

    /**
     * Returns pointer to native InfoBarAndroid instance. TODO(crbug.com/40120294): The function is
     * used in subclasses typically to get Tab reference. When Tab is modularized, replace this
     * function with the one that returns Tab reference.
     */
    protected long getNativeInfoBarPtr() {
        return mNativeInfoBarPtr;
    }

    /** Sets the Container that displays the InfoBar. */
    public void setContainer(Container container) {
        mContainer = container;
    }

    /** @return Whether or not this InfoBar is already dismissed (i.e. closed). */
    protected boolean isDismissed() {
        return mIsDismissed;
    }

    @Override
    public boolean areControlsEnabled() {
        return mControlsEnabled;
    }

    @Override
    public void setControlsEnabled(boolean state) {
        mControlsEnabled = state;
    }

    @Override
    public void onClick() {
        setControlsEnabled(false);
    }

    @Override
    public void onButtonClicked(boolean isPrimaryButton) {}

    @Override
    public void onLinkClicked() {
        if (mNativeInfoBarPtr != 0) InfoBarJni.get().onLinkClicked(mNativeInfoBarPtr, InfoBar.this);
    }

    /**
     * Performs some action related to the button being clicked.
     * @param action The type of action defined in {@link ActionType} in this class.
     */
    protected void onButtonClicked(@ActionType int action) {
        if (mNativeInfoBarPtr != 0) {
            InfoBarJni.get().onButtonClicked(mNativeInfoBarPtr, InfoBar.this, action);
        }
    }

    @Override
    public void onCloseButtonClicked() {
        if (mNativeInfoBarPtr != 0 && !mIsDismissed) {
            InfoBarJni.get().onCloseButtonClicked(mNativeInfoBarPtr, InfoBar.this);
        }
    }

    @InfoBarIdentifier
    @NativeMethods
    interface Natives {
        int getInfoBarIdentifier(long nativeInfoBarAndroid, InfoBar caller);

        void onLinkClicked(long nativeInfoBarAndroid, InfoBar caller);

        void onButtonClicked(long nativeInfoBarAndroid, InfoBar caller, int action);

        void onCloseButtonClicked(long nativeInfoBarAndroid, InfoBar caller);
    }
}
