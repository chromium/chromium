// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Coordinator for the bottom bar introductory promo dialog. */
@NullMarked
public class BottomBarPromoDialogCoordinator
        implements ModalDialogProperties.Controller, Destroyable {

    /** Listener interface to notify dialog events. */
    public interface BottomBarPromoDialogListener {
        /** Called when the promo dialog is accepted by the user. */
        void onPromoDialogAccepted();
    }

    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final NullableObservableSupplier<Profile> mProfileSupplier;

    private @Nullable BottomBarPromoDialogListener mListener;
    private @Nullable PropertyModel mDialogModel;
    private @Nullable Tracker mTracker;

    /**
     * Constructs a {@link BottomBarPromoDialogCoordinator} instance.
     *
     * @param context The {@link Context} used to retrieve resources and inflate the layout.
     * @param modalDialogManagerSupplier The supplier of {@link ModalDialogManager} used to display
     *     the dialog.
     * @param profileSupplier The supplier of the active {@link Profile} used for feature tracking.
     */
    public BottomBarPromoDialogCoordinator(
            Context context,
            NonNullObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            NullableObservableSupplier<Profile> profileSupplier) {
        mContext = context;
        mModalDialogManager = modalDialogManagerSupplier.get();
        mProfileSupplier = profileSupplier;
    }

    @Override
    public void destroy() {
        mListener = null;
        if (mDialogModel != null) {
            mModalDialogManager.dismissDialog(
                    mDialogModel, DialogDismissalCause.ACTIVITY_DESTROYED);
            mDialogModel = null;
        }
    }

    /** Sets the listener to be notified of dialog events. */
    public void setListener(BottomBarPromoDialogListener listener) {
        mListener = listener;
    }

    /** Evaluates whether to show the introductory promo dialog and shows it if eligible. */
    public void maybeShowPromoDialog() {
        if (mDialogModel != null) return;

        Profile profile = mProfileSupplier.get();
        if (profile == null) return;

        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        if (!tracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_BOTTOM_BAR_PROMO_DIALOG)) {
            return;
        }
        mTracker = tracker;

        Context context = mContext;
        View dialogView =
                LayoutInflater.from(context).inflate(R.layout.bottom_bar_promo_dialog_view, null);
        TextView titleView = dialogView.findViewById(R.id.title);
        TextView descriptionView = dialogView.findViewById(R.id.description);

        titleView.setText(context.getString(R.string.iph_android_bottom_bar_dialog_title));
        descriptionView.setText(
                context.getString(R.string.iph_android_bottom_bar_dialog_description));

        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.CUSTOM_VIEW, dialogView)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                context.getString(R.string.iph_android_bottom_bar_trigger_iph))
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                context.getString(R.string.iph_android_bottom_bar_dialog_ack))
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .build();

        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP, true);
    }

    @Override
    public void onClick(PropertyModel model, @ModalDialogProperties.ButtonType int buttonType) {
        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
    }

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        mDialogModel = null;

        if (mTracker != null) {
            mTracker.dismissed(FeatureConstants.ANDROID_BOTTOM_BAR_PROMO_DIALOG);
            mTracker = null;
        }

        if (mListener != null && dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
            mListener.onPromoDialogAccepted();
        }
    }
}
