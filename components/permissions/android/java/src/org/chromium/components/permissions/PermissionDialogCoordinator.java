// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.Button;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.NullUnmarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.util.DimensionCompat;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.ui.LayoutInflaterUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator class for displaying the permission dialog. It proxies the communication to the
 * {@link PermissionDialogMediator}.
 */
@NullMarked
public class PermissionDialogCoordinator {
    /** A delegate interface for PermissionDialogCoordinator to interact with other classes. */
    interface Delegate {
        /**
         * Called when the user has just completed a permissions prompt flow with a result.
         *
         * @param result A ContentSettingValues type, indicating the last dialog result.
         */
        void onPermissionDialogResult(@ContentSettingValues int result);

        /**
         * Called when the user completed a permissions prompt. The dialog is dismissed, and if
         * there's another dialog in queue, it can finally pop up.
         */
        void onPermissionDialogEnded();
    }

    private class PermissionModalDialogManagerObserver
            implements ModalDialogManager.ModalDialogManagerObserver {
        @Override
        public void onDialogShown(View dialogView) {
            // If the negative button is shown, it will be the last button.
            ViewGroup buttonGroup = dialogView.findViewById(R.id.button_group);
            if (buttonGroup == null || buttonGroup.getChildCount() == 0) {
                return;
            }
            View lastView = buttonGroup.getChildAt(buttonGroup.getChildCount() - 1);
            assert lastView instanceof Button;
            Button lastButton = (Button) lastView;
            if (lastButton.getVisibility() == View.GONE
                    || lastButton.getText()
                            != assumeNonNull(mDialogDelegate).getNegativeButtonText()) {
                return;
            }

            // When `showDialog` is called and triggered this callback, the button
            // visibility is VISIBLE but the button might not been laid out yet. If that
            // is the case, we try again after the next layout.
            if (lastButton.isLaidOut()) {
                recordOutOfScreenNegativeButton(lastButton);
                return;
            }
            lastButton
                    .getViewTreeObserver()
                    .addOnGlobalLayoutListener(
                            new ViewTreeObserver.OnGlobalLayoutListener() {
                                @Override
                                public void onGlobalLayout() {
                                    if (!lastButton.isLaidOut()) {
                                        return;
                                    }
                                    lastButton
                                            .getViewTreeObserver()
                                            .removeOnGlobalLayoutListener(this);
                                    recordOutOfScreenNegativeButton(lastButton);
                                }
                            });
        }

        /** Record histogram if a button is rendered out of screen. */
        @NullUnmarked
        private void recordOutOfScreenNegativeButton(Button button) {
            int[] loc = new int[2];
            button.getLocationOnScreen(loc);
            int x = loc[0];
            int y = loc[1];
            Activity activity =
                    assumeNonNull(ContextUtils.activityFromContext(button.getContext()));
            // @NullUnmarked because passing null |positionUpdater| to DimensionCompat.create is
            // is wrong.
            DimensionCompat dimension = DimensionCompat.create(activity, null);

            boolean outOfScreen =
                    x < 0
                            || y < 0
                            || x > dimension.getWindowWidth()
                            || y > dimension.getWindowHeight();
            RecordHistogram.recordBooleanHistogram(
                    "Permissions.OneTimePermission.Android.NegativeButtonOutOfScreen", outOfScreen);
        }
    }

    private @Nullable PropertyModel mCustomViewModel;
    private @Nullable PropertyModelChangeProcessor mCustomViewModelChangeProcessor;
    private @Nullable PermissionDialogDelegate mDialogDelegate;
    private final Delegate mCoordinatorDelegate;
    private @Nullable ModalDialogManager mModalDialogManager;
    private @Nullable ModalDialogManagerObserver mModalDialogManagerObserver;
    private @Nullable PermissionDialogMediator mMediator;

    public PermissionDialogCoordinator(Delegate delegate) {
        mCoordinatorDelegate = delegate;
    }

    /**
     * Setup a custom view for permission dialog, also set up the MVC model for the dialog's custom
     * view.
     */
    private View createCustomView() {
        Context context = getContext();
        // One time prompts don't share the same layout.
        assumeNonNull(mDialogDelegate);
        View customView =
                LayoutInflaterUtils.inflate(
                        context,
                        (mDialogDelegate.isEmbeddedPromptVariant()
                                        || mDialogDelegate.canShowEphemeralOption())
                                ? R.layout.permission_dialog_one_time_permission
                                : R.layout.permission_dialog,
                        null);

        mCustomViewModel = PermissionDialogCustomViewModelFactory.getModel(mDialogDelegate);
        mCustomViewModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mCustomViewModel,
                        customView,
                        (mDialogDelegate.isEmbeddedPromptVariant()
                                        || mDialogDelegate.canShowEphemeralOption())
                                ? PermissionOneTimeDialogCustomViewBinder::bind
                                : PermissionDialogCustomViewBinder::bind);
        return customView;
    }

    /**
     * Shows the dialog asking the user for a web API permission.
     *
     * @param delegate The wrapper for the native-side permission delegate.
     * @return returns false if something's missing (like the delegate or modal dialog manager) and
     *     we couldn't show the dialog. Otherwise, it returns true.
     */
    public boolean showDialog(PermissionDialogDelegate delegate) {
        mDialogDelegate = delegate;
        mModalDialogManager =
                mDialogDelegate != null
                        ? mDialogDelegate.getWindow().getModalDialogManager()
                        : null;

        // The tab may have navigated or been closed while we were waiting for Chrome Home to close.
        // For some embedders (e.g. WebEngine) the layout might not be inflated and so the
        // ModalDialogManager is not available.
        if (mModalDialogManager == null) {
            mCoordinatorDelegate.onPermissionDialogResult(ContentSettingValues.DEFAULT);
            if (mDialogDelegate != null) {
                mDialogDelegate.onDismiss(DismissalType.AUTODISMISS_NO_DIALOG_MANAGER);
            }
            mCoordinatorDelegate.onPermissionDialogEnded();
            return false;
        }

        mMediator =
                mDialogDelegate.getEmbeddedPromptVariant() != EmbeddedPromptVariant.UNINITIALIZED
                        ? new EmbeddedPermissionDialogMediator(mCoordinatorDelegate)
                        : new PermissionDialogMediator(mCoordinatorDelegate);
        mModalDialogManagerObserver = new PermissionModalDialogManagerObserver();
        mModalDialogManager.addObserver(mModalDialogManagerObserver);
        mMediator.showDialog(mDialogDelegate, mModalDialogManager, createCustomView());
        return true;
    }

    /** Dismiss the current dialog, called from native. */
    public void dismissFromNative() {
        assumeNonNull(mMediator).dismissFromNative();
    }

    /** Update the current dialog. This may hide the current dialog and show OS prompt instead. */
    public void updateDialog() {
        assumeNonNull(mMediator).updateDialog(createCustomView());
    }

    /**
     * Update the icon of the current custom view for the given bit map.
     *
     * @param icon The bitmap icon to display on the custom view.
     */
    public void updateIcon(Bitmap icon) {
        if (mCustomViewModel == null) {
            return;
        }

        mCustomViewModel.set(
                PermissionDialogCustomViewProperties.ICON,
                new BitmapDrawable(getContext().getResources(), icon));
        mCustomViewModel.set(PermissionDialogCustomViewProperties.ICON_TINT, null);
    }

    /**
     * Get size of icon showing on the custom view.
     *
     * @return icon The size of icon displayed on the custom view.
     */
    public int getIconSizeInPx() {
        return getContext().getResources().getDimensionPixelSize(R.dimen.large_favicon_size);
    }

    private Context getContext() {
        assert mDialogDelegate != null;
        Context context = mDialogDelegate.getWindow().getContext().get();
        assert context != null;
        // Use the context to access resources instead of the activity because the activity may not
        // have the correct resources in some cases (e.g. WebLayer).
        return context;
    }

    public void destroy() {
        if (mModalDialogManagerObserver != null) {
            assumeNonNull(mModalDialogManager).removeObserver(mModalDialogManagerObserver);
            mModalDialogManagerObserver = null;
        }

        mCustomViewModel = null;
        if (mCustomViewModelChangeProcessor != null) {
            mCustomViewModelChangeProcessor.destroy();
            mCustomViewModelChangeProcessor = null;
        }
        if (mMediator != null) {
            mMediator.destroy();
            mMediator = null;
        }
    }

    public void clickButtonForTest(@ModalDialogProperties.ButtonType int buttonType) {
        assumeNonNull(mMediator).clickButtonForTest(buttonType); // IN-TEST
    }
}
