// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RatingBar;
import android.widget.TextView;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Displays the "Add to Homescreen" dialog, which contains a (possibly editable) title, icon, and
 * possibly an origin.
 *
 * <p>When the constructor is called, the dialog is shown immediately. A spinner is displayed if any
 * data is not yet fetched, and accepting the dialog is disabled until all data is available and in
 * its place on the screen.
 */
public class AddToHomescreenDialogView
        implements View.OnClickListener, ModalDialogProperties.Controller {
    private PropertyModel mDialogModel;
    private ModalDialogManager mModalDialogManager;
    @VisibleForTesting protected AddToHomescreenViewDelegate mDelegate;

    private View mParentView;

    /**
     * {@link #mShortcutTitleInput} and the {@link #mAppLayout} are mutually exclusive, depending on
     * whether the home screen item is a bookmark shortcut or a web/native app.
     */
    private EditText mShortcutTitleInput;

    private LinearLayout mAppLayout;
    private TextView mAppNameView;
    private EditText mHomebrewAppNameInput;
    private TextView mAppOriginView;
    private RatingBar mAppRatingBar;
    private ImageView mPlayLogoView;

    private View mProgressBarView;
    private ImageView mIconView;

    private @AppType int mAppType;
    private boolean mCanSubmit;

    private final String mInstallTitleText;
    private final String mInstallButtonText;
    private final String mAddTitleText;
    private final String mAddButtonText;

    @VisibleForTesting
    public AddToHomescreenDialogView(
            Context context,
            ModalDialogManager modalDialogManager,
            AddToHomescreenViewDelegate delegate) {
        assert delegate != null;

        mModalDialogManager = modalDialogManager;
        mDelegate = delegate;
        mParentView = LayoutInflater.from(context).inflate(R.layout.add_to_homescreen_dialog, null);

        mProgressBarView = mParentView.findViewById(R.id.spinny);
        mIconView = (ImageView) mParentView.findViewById(R.id.icon);

        mShortcutTitleInput = mParentView.findViewById(R.id.shortcut_name);
        mShortcutTitleInput.setOnEditorActionListener(
                (TextView v, int actionId, KeyEvent event) -> {
                    if (actionId == EditorInfo.IME_ACTION_DONE) {
                        if (!mDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED)) {
                            onClick(mDialogModel, ModalDialogProperties.ButtonType.POSITIVE);
                        }
                        return true;
                    }
                    return false;
                });

        mAppLayout = (LinearLayout) mParentView.findViewById(R.id.app_info);
        mAppNameView = (TextView) mAppLayout.findViewById(R.id.app_name);
        mHomebrewAppNameInput = mAppLayout.findViewById(R.id.homebrew_name);
        mAppOriginView = (TextView) mAppLayout.findViewById(R.id.origin);
        mAppRatingBar = (RatingBar) mAppLayout.findViewById(R.id.control_rating);
        mPlayLogoView = (ImageView) mParentView.findViewById(R.id.play_logo);

        mAppNameView.setOnClickListener(this);
        mIconView.setOnClickListener(this);

        mParentView.addOnLayoutChangeListener(
                new View.OnLayoutChangeListener() {
                    @Override
                    public void onLayoutChange(
                            View v,
                            int left,
                            int top,
                            int right,
                            int bottom,
                            int oldLeft,
                            int oldTop,
                            int oldRight,
                            int oldBottom) {
                        if (mProgressBarView.getMeasuredHeight()
                                        == mShortcutTitleInput.getMeasuredHeight()
                                && mShortcutTitleInput.getBackground() != null) {
                            // Force the text field to align better with the icon by accounting for
                            // the padding introduced by the background drawable.
                            mShortcutTitleInput.getLayoutParams().height =
                                    mProgressBarView.getMeasuredHeight()
                                            + mShortcutTitleInput.getPaddingBottom();
                            String caller =
                                    "AddToHomescreenDialogView.<init>."
                                            + "OnLayoutChangeListener.onLayoutChange";
                            ViewUtils.requestLayout(v, caller);
                            v.removeOnLayoutChangeListener(this);
                        }
                    }
                });

        // The "Add" button should be disabled if the dialog's text field is empty.
        mShortcutTitleInput.addTextChangedListener(
                new TextWatcher() {
                    @Override
                    public void onTextChanged(CharSequence s, int start, int before, int count) {}

                    @Override
                    public void beforeTextChanged(
                            CharSequence s, int start, int count, int after) {}

                    @Override
                    public void afterTextChanged(Editable editableText) {
                        updateInstallButton();
                    }
                });

        mHomebrewAppNameInput.addTextChangedListener(
                new TextWatcher() {
                    @Override
                    public void onTextChanged(CharSequence s, int start, int before, int count) {}

                    @Override
                    public void beforeTextChanged(
                            CharSequence s, int start, int count, int after) {}

                    @Override
                    public void afterTextChanged(Editable editableText) {
                        updateInstallButton();
                    }
                });

        Resources resources = context.getResources();
        mInstallTitleText = resources.getString(R.string.menu_install_webapp);
        mInstallButtonText = resources.getString(R.string.app_banner_install);
        mAddTitleText = resources.getString(R.string.pwa_uni_install_option_shortcut);
        mAddButtonText = resources.getString(R.string.add);

        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.TITLE, mAddTitleText)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, mAddButtonText)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                R.string.cancel)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mParentView)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(
                                ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS,
                                UiUtils.PROMPT_INPUT_PROTECTION_SHORT_DELAY_MS)
                        .build();
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
    }

    // @VisibleForTests implies that a method should only be accessed from tests or within private
    // scope, but this is needed by AddToHomescreenViewBinder.
    protected void setTitle(String title) {
        mAppNameView.setText(title);
        mShortcutTitleInput.setText(title);
        mShortcutTitleInput.setSelection(mShortcutTitleInput.getText().length());
        mHomebrewAppNameInput.setText(title);
        mHomebrewAppNameInput.setSelection(mHomebrewAppNameInput.getText().length());
        mIconView.setContentDescription(title);
    }

    void setUrl(String url) {
        mAppOriginView.setText(url);
    }

    void setIcon(Bitmap icon, boolean isAdaptive) {
        if (isAdaptive && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            setAdaptiveIcon(icon);
        } else {
            assert !isAdaptive : "Adaptive icons should not be provided pre-Android O.";
            mIconView.setImageBitmap(icon);
        }
        mProgressBarView.setVisibility(View.GONE);
        mIconView.setVisibility(View.VISIBLE);
    }

    @RequiresApi(Build.VERSION_CODES.O)
    private void setAdaptiveIcon(Bitmap icon) {
        mIconView.setImageIcon(Icon.createWithAdaptiveBitmap(icon));
    }

    void setType(@AppType int type) {
        mAppType = type;
        resetAllVisibility();
        switch (mAppType) {
            case AppType.NATIVE:
                mAppLayout.setVisibility(View.VISIBLE);
                mAppNameView.setVisibility(View.VISIBLE);
                mAppRatingBar.setVisibility(View.VISIBLE);
                mPlayLogoView.setVisibility(View.VISIBLE);
                break;
            case AppType.SHORTCUT:
                mShortcutTitleInput.setVisibility(View.VISIBLE);
                break;
            case AppType.WEBAPK:
                mAppLayout.setVisibility(View.VISIBLE);
                mAppNameView.setVisibility(View.VISIBLE);
                mAppOriginView.setVisibility(View.VISIBLE);
                break;
            case AppType.WEBAPK_DIY:
                mAppLayout.setVisibility(View.VISIBLE);
                mHomebrewAppNameInput.setVisibility(View.VISIBLE);
                mAppOriginView.setVisibility(View.VISIBLE);
                mAppOriginView.setVisibility(View.VISIBLE);
                break;
        }

        if (mAppType == AppType.SHORTCUT) {
            mDialogModel.set(ModalDialogProperties.TITLE, mAddTitleText);
            mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_TEXT, mAddButtonText);
        } else {
            mDialogModel.set(ModalDialogProperties.TITLE, mInstallTitleText);
            mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_TEXT, mInstallButtonText);
        }
    }

    void setNativeInstallButtonText(String installButtonText) {
        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_TEXT, installButtonText);
        mDialogModel.set(
                ModalDialogProperties.POSITIVE_BUTTON_CONTENT_DESCRIPTION,
                ContextUtils.getApplicationContext()
                        .getString(
                                R.string.app_banner_view_native_app_install_accessibility,
                                installButtonText));
    }

    void setNativeAppRating(float rating) {
        mAppRatingBar.setRating(rating);
        mPlayLogoView.setImageResource(R.drawable.google_play);
    }

    // @VisibleForTests implies that a method should only be accessed from tests or within private
    // scope, but this is needed by AddToHomescreenViewBinder.
    protected void setCanSubmit(boolean canSubmit) {
        mCanSubmit = canSubmit;
        updateInstallButton();
    }

    // Update button state for shortcut or diy app.
    private void updateInstallButton() {
        TextView appNameView = getAppNameView();
        boolean missingTitle =
                appNameView.getVisibility() == View.VISIBLE
                        && TextUtils.isEmpty(appNameView.getText());
        mDialogModel.set(
                ModalDialogProperties.POSITIVE_BUTTON_DISABLED, !mCanSubmit || missingTitle);
    }

    private void resetAllVisibility() {
        mShortcutTitleInput.setVisibility(View.GONE);
        mAppLayout.setVisibility(View.GONE);
        mHomebrewAppNameInput.setVisibility(View.GONE);
        mAppNameView.setVisibility(View.GONE);
        mAppOriginView.setVisibility(View.GONE);
        mAppRatingBar.setVisibility(View.GONE);
        mPlayLogoView.setVisibility(View.GONE);
    }

    /**
     * From {@link View.OnClickListener}. Called when the views that have this class registered as
     * their {@link View.OnClickListener} are clicked.
     *
     * @param v The view that was clicked.
     */
    @Override
    public void onClick(View v) {
        if ((v == mAppNameView || v == mIconView)) {
            if (mDelegate.onAppDetailsRequested()) {
                mModalDialogManager.dismissDialog(
                        mDialogModel, DialogDismissalCause.ACTION_ON_CONTENT);
            }
        }
    }

    /**
     * From {@link ModalDialogProperties.Controller}. Called when a dialog button is clicked.
     *
     * @param model The dialog model that is associated with this click event.
     * @param buttonType The type of the button.
     */
    @Override
    public void onClick(PropertyModel model, int buttonType) {
        int dismissalCause = DialogDismissalCause.NEGATIVE_BUTTON_CLICKED;
        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
            String title = getAppNameView().getText().toString();
            mDelegate.onAddToHomescreen(title, mAppType);
            dismissalCause = DialogDismissalCause.POSITIVE_BUTTON_CLICKED;
        }
        mModalDialogManager.dismissDialog(mDialogModel, dismissalCause);
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) return;

        mDelegate.onViewDismissed();
    }

    @VisibleForTesting
    TextView getAppNameView() {
        switch (mAppType) {
            case AppType.SHORTCUT:
                return mShortcutTitleInput;
            case AppType.WEBAPK_DIY:
                return mHomebrewAppNameInput;
            case AppType.WEBAPK:
            case AppType.NATIVE:
                return mAppNameView;
            default:
                assert false;
                return null;
        }
    }

    View getParentViewForTest() {
        return mParentView;
    }
}
