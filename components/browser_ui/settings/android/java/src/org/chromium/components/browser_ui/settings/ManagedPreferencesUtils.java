// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.LayoutRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.widget.Toast;

import java.util.Locale;

/** Utilities and common methods to handle settings managed by policies. */
public class ManagedPreferencesUtils {
    private static Toast showToastWithResourceId(Context context, @StringRes int resId) {
        Toast toast = Toast.makeText(context, context.getString(resId), Toast.LENGTH_LONG);
        toast.show();
        return toast;
    }

    /**
     * Shows a toast indicating that the previous action is managed by the system administrator.
     *
     * This is usually used to explain to the user why a given control is disabled in the settings.
     *
     * @param context The context where the Toast will be shown.
     */
    public static Toast showManagedByAdministratorToast(Context context) {
        return showToastWithResourceId(context, R.string.managed_by_your_organization);
    }

    /**
     * Shows a toast indicating that the previous action is managed by the parent(s) of the
     * supervised user.
     * This is usually used to explain to the user why a given control is disabled in the settings.
     *
     * @param context The context where the Toast will be shown.
     */
    public static Toast showManagedByParentToast(
            Context context, @Nullable ManagedPreferenceDelegate delegate) {
        return showToastWithResourceId(context, getManagedByParentStringRes(delegate));
    }

    /**
     * Shows a toast indicating that some of the preferences in the list of preferences to reset are
     * managed by the system administrator.
     *
     * @param context The context where the Toast will be shown.
     */
    public static Toast showManagedSettingsCannotBeResetToast(Context context) {
        return showToastWithResourceId(context, R.string.managed_settings_cannot_be_reset);
    }

    /** @return The resource ID for the Managed By Enterprise icon. */
    public static @DrawableRes int getManagedByEnterpriseIconId() {
        return R.drawable.ic_business_small;
    }

    /** @return The resource ID for the Managed by Custodian icon. */
    public static @DrawableRes int getManagedByCustodianIconId() {
        return R.drawable.ic_account_child_grey600_36dp;
    }

    /**
     * @return The appropriate Drawable based on whether the preference is controlled by a policy or
     *         a custodian.
     */
    public static Drawable getManagedIconDrawable(
            @Nullable ManagedPreferenceDelegate delegate, Preference preference) {
        int resId = getManagedIconResId(delegate, preference);
        return resId == 0
                ? preference.getIcon()
                : SettingsUtils.getTintedIcon(preference.getContext(), resId);
    }

    /**
     * @return The resource ID for the managed icon to show. Returns 0 if no managed icon should be
     *         shown.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static int getManagedIconResId(
            @Nullable ManagedPreferenceDelegate delegate, Preference preference) {
        if (delegate == null) return 0;

        if (delegate.isPreferenceControlledByPolicy(preference)) {
            return getManagedByEnterpriseIconId();
        } else if (delegate.isPreferenceControlledByCustodian(preference)) {
            return getManagedByCustodianIconId();
        }

        return 0;
    }

    /**
     * Initializes the Preference based on the state of any policies that may affect it,
     * e.g. by showing a managed icon or disabling clicks on the preference. If |preference| is an
     * instance of ChromeImageViewPreference, the icon is not set since the ImageView widget will
     * display the managed icons.
     *
     * This should be called once, before the preference is displayed.
     *
     * @param delegate The delegate that controls whether the preference is managed. May be null,
     *         then this method does nothing.
     * @param preference The Preference that is being initialized.
     * @param allowManagedIcon Whether the icon view should show the managed icon when the
     *         preference is managed.
     * @param hasCustomLayout Whether the preference defines its own layout or should use the
     *         embedder's default layout.
     */
    public static void initPreference(
            @Nullable ManagedPreferenceDelegate delegate,
            Preference preference,
            boolean allowManagedIcon,
            boolean hasCustomLayout) {
        if (delegate == null) return;

        // Embedders may define its own default layout for preferences, which can only be applied
        // if the preference doesn't use a custom layout and if the preference is controlled by
        // policy.
        if (!hasCustomLayout && delegate.isPreferenceControlledByPolicy(preference)) {
            @LayoutRes int layoutResource = delegate.defaultPreferenceLayoutResource();
            if (layoutResource != 0) {
                preference.setLayoutResource(layoutResource);
            }
        }

        if (allowManagedIcon) {
            preference.setIcon(getManagedIconDrawable(delegate, preference));
        }

        if (delegate.isPreferenceClickDisabled(preference)) {
            // Disable the views and prevent the Preference from mucking with the enabled state.
            preference.setShouldDisableView(false);
            preference.setEnabled(false);

            // Prevent default click behavior.
            preference.setFragment(null);
            preference.setIntent(null);
            preference.setOnPreferenceClickListener(null);
        }
    }

    /**
     * Disables the Preference's views if the preference is not clickable and adds a disclaimer
     * indicating that the preference is managed.
     *
     * This should be called from the Preference's onBindView() method.
     *
     * @param delegate The delegate that controls whether the preference is managed. May be null,
     *         then this method does nothing.
     * @param preference The Preference that owns the view
     * @param view The View that was bound to the Preference
     */
    public static void onBindViewToPreference(
            @Nullable ManagedPreferenceDelegate delegate, Preference preference, View view) {
        if (delegate == null) return;

        if (delegate.isPreferenceClickDisabled(preference)) {
            ViewUtils.setEnabledRecursive(view, false);
        }

        // Append managed information to summary if necessary.
        TextView summaryView = view.findViewById(android.R.id.summary);
        CharSequence descriptionText =
                summaryView.getVisibility() == View.VISIBLE ? summaryView.getText() : null;
        // Fallback to the old UI if the managed disclaimer view doesn't exist, which may happen if
        // a {@link ChromeBasePreference} defines its own layout.
        // Highlighted managed disclaimers only apply to preferences managed by policy. For
        // preferences managed by a custodian, fallback to the legacy UI.
        // TODO(crbug.com/40236420): Remove this fallback once all custom layouts for all affected
        //                          preferences include the managed disclaimer view.
        // TODO(crbug.com/40243868): Apply highlighted managed disclaimer for preferences managed
        //                          by a custodian.
        if (view.findViewById(R.id.managed_disclaimer_text) != null
                && delegate.isPreferenceControlledByPolicy(preference)) {
            // Hide the icon since it will be shown on the highlighted managed disclaimer.
            hideManagedIcon(view);
            setSummaryWithHighlightedManagedInfo(descriptionText, view);
        } else {
            CharSequence managedDisclaimerText = getManagedDisclaimerText(delegate, preference);
            setSummaryWithManagedInfo(descriptionText, managedDisclaimerText, view);
        }
    }

    /**
     * Calls onBindViewToPreference() above. Then, if the ChromeImageViewPreference is managed, the
     * widget ImageView is set to the appropriate managed icon, and its onClick listener is set to
     * show the appropriate managed message toast.
     *
     * This should be called from the Preference's onBindView() method.
     *
     * @param delegate The delegate that controls whether the preference is managed. May be null,
     *                 then this method does nothing.
     * @param preference The ChromeImageViewPreference that owns the view.
     * @param view The View that was bound to the ChromeImageViewPreference.
     */
    public static void onBindViewToImageViewPreference(
            @Nullable ManagedPreferenceDelegate delegate,
            ChromeImageViewPreference preference,
            View view) {
        if (delegate == null) return;

        onBindViewToPreference(delegate, preference, view);

        if (!delegate.isPreferenceControlledByPolicy(preference)
                && !delegate.isPreferenceControlledByCustodian(preference)) {
            return;
        }

        ImageView button = view.findViewById(R.id.image_view_widget);
        button.setImageDrawable(getManagedIconDrawable(delegate, preference));
        if (delegate.isPreferenceControlledByPolicy(preference)) {
            button.setContentDescription(
                    preference
                            .getContext()
                            .getResources()
                            .getString(R.string.managed_by_your_organization));
        }
        button.setOnClickListener(
                (View v) -> {
                    if (delegate.isPreferenceControlledByPolicy(preference)) {
                        showManagedByAdministratorToast(preference.getContext());
                    } else if (delegate.isPreferenceControlledByCustodian(preference)) {
                        showManagedByParentToast(preference.getContext(), delegate);
                    }
                });
    }

    /**
     * Intercepts the click event if the given Preference is managed and shows a toast in that case.
     *
     * This should be called from the Preference's onClick() method.
     *
     * @param delegate The delegate that controls whether the preference is managed. May be null,
     *         then this method does nothing and returns false.
     * @param preference The Preference that was clicked.
     * @return true if the click event was handled by this helper and shouldn't be further
     *         propagated; false otherwise.
     */
    public static boolean onClickPreference(
            @Nullable ManagedPreferenceDelegate delegate, Preference preference) {
        if (delegate == null || !delegate.isPreferenceClickDisabled(preference)) {
            return false;
        }

        if (delegate.isPreferenceControlledByPolicy(preference)) {
            showManagedByAdministratorToast(preference.getContext());
        } else if (delegate.isPreferenceControlledByCustodian(preference)) {
            showManagedByParentToast(preference.getContext(), delegate);
        } else {
            // If the preference is disabled, it should be either because it's managed by enterprise
            // policy or by the custodian.
            assert false;
        }
        return true;
    }

    /**
     * Checks if a custom layout was defined for the preference. For example, Sync and Google
     * service preferences in the Main Settings menu define their own layouts and use managed
     * preference classes to leverage icon tinting. Also, those preferences don't need to be
     * managed, so there is no need to change their layouts to include the managed disclaimer.
     * @param context The context for a given preference.
     * @param attrs The attributes of the XML tag that is inflating the view.
     * @return Whether a custom layout was defined.
     */
    public static boolean isCustomLayoutApplied(Context context, AttributeSet attrs) {
        final TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.Preference);

        // Take the custom layout defined via either {@code Preference_layout} or
        // {@code Preference_android_layout}.
        return a.getResourceId(R.styleable.Preference_android_layout, 0) != 0
                || a.getResourceId(R.styleable.Preference_layout, 0) != 0;
    }

    /**
     * @param descriptionText A description or a state for a given preference.
     * @param managedDisclaimerText The text the indicates that a preference is managed.
     * @param view The view corresponding to a given preference.
     */
    private static void setSummaryWithManagedInfo(
            @Nullable CharSequence descriptionText,
            @Nullable CharSequence managedDisclaimerText,
            View view) {
        boolean emptyDescription = TextUtils.isEmpty(descriptionText);
        boolean emptyManagedDisclaimer = TextUtils.isEmpty(managedDisclaimerText);

        if (emptyDescription && emptyManagedDisclaimer) {
            hideSummaryView(view);
        } else if (emptyDescription) {
            showSummaryViewWithText(managedDisclaimerText, view);
        } else if (emptyManagedDisclaimer) {
            showSummaryViewWithText(descriptionText, view);
        } else {
            showSummaryViewWithText(
                    String.format(
                            Locale.getDefault(), "%s\n%s", descriptionText, managedDisclaimerText),
                    view);
        }

        // Explicitly hide the disclaimer view in case it was previously visible in a {@link View}
        // recycled by a {@link RecyclerView}.
        hideManagedDisclaimerView(view);
    }

    /**
     * @param descriptionText A description or a state for a given preference.
     * @param view The view corresponding to a given preference.
     */
    private static void setSummaryWithHighlightedManagedInfo(
            @Nullable CharSequence descriptionText, View view) {
        if (TextUtils.isEmpty(descriptionText)) {
            hideSummaryView(view);
        } else {
            showSummaryViewWithText(descriptionText, view);
        }

        showManagedDisclaimerView(view);
    }

    /**
     * Hide the managed icon, to be used when the preference defines a custom layout and is managed
     * by policy. In that case, the icon will be shown on the managed disclaimer view.
     *
     * @param view The view corresponding to a given preference.
     */
    private static void hideManagedIcon(View view) {
        final ImageView imageView = (ImageView) view.findViewById(android.R.id.icon);
        if (imageView != null) {
            imageView.setVisibility(View.GONE);
        }
        final View imageFrame = view.findViewById(R.id.icon_frame);
        if (imageFrame != null) {
            imageFrame.setVisibility(View.GONE);
        }
    }

    /**
     * @param delegate The delegate that controls whether the preference is managed. If null,
     *         then the preference is not managed.
     * @param preference The {@link Preference} that is being shown to the user.
     * @return Text indicating that a preference is managed by an administrator or custodian, or
     *         null if the preference is not managed.
     */
    private static @Nullable CharSequence getManagedDisclaimerText(
            @Nullable ManagedPreferenceDelegate delegate, Preference preference) {
        if (delegate == null) return null;
        if (delegate.isPreferenceControlledByPolicy(preference)) {
            return preference.getContext().getString(R.string.managed_by_your_organization);
        }
        if (delegate.isPreferenceControlledByCustodian(preference)) {
            return preference.getContext().getString(getManagedByParentStringRes(delegate));
        }
        return null;
    }

    /**
     * @param delegate The delegate that controls whether the preference is managed.
     * @return The resource ID for the disclaimer text for a preference managed by a custodian.
     */
    private static @StringRes int getManagedByParentStringRes(
            @Nullable ManagedPreferenceDelegate delegate) {
        boolean hasMultipleCustodians = false;
        if (delegate != null) {
            hasMultipleCustodians = delegate.doesProfileHaveMultipleCustodians();
        }
        return hasMultipleCustodians
                ? R.string.managed_by_your_parents
                : R.string.managed_by_your_parent;
    }

    /**
     * Hides the summary view for a preference.
     * @param view The view corresponding to the preference.
     */
    private static void hideSummaryView(View view) {
        TextView summaryView = view.findViewById(android.R.id.summary);
        summaryView.setVisibility(View.GONE);
    }

    /**
     * Sets the text to be shown in the summary view for a preference, and makes the summary view
     * visible.
     * @param summary The text to show in the {@code summary} view.
     * @param view The view corresponding to the preference.
     */
    private static void showSummaryViewWithText(@Nullable CharSequence summary, View view) {
        TextView summaryView = view.findViewById(android.R.id.summary);
        summaryView.setText(summary);
        summaryView.setVisibility(View.VISIBLE);
    }

    /**
     * Removes the disclaimer view from the preference's view, if it exists.
     * @param view The view corresponding to the preference.
     */
    private static void hideManagedDisclaimerView(View view) {
        View managedDisclaimerView = view.findViewById(R.id.managed_disclaimer_text);
        if (managedDisclaimerView != null) {
            managedDisclaimerView.setVisibility(View.GONE);
        }
    }

    /**
     * Sets the text to be shown in the managed disclaimer view for a preference.
     * @param view The view corresponding to the preference.
     */
    private static void showManagedDisclaimerView(View view) {
        TextViewWithCompoundDrawables managedDisclaimerView =
                view.findViewById(R.id.managed_disclaimer_text);
        assert managedDisclaimerView != null
                : "Missing managed disclaimer view; custom layout for a new preference?";
        managedDisclaimerView.setVisibility(View.VISIBLE);
        managedDisclaimerView.setEnabled(true);
    }
}
