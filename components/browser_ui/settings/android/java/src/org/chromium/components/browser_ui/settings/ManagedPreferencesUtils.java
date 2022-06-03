// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.widget.Toast;

import java.util.Locale;

/**
 * Utilities and common methods to handle settings managed by policies.
 */
public class ManagedPreferencesUtils {
    /**
     * Shows a toast indicating that the previous action is managed by the system administrator.
     *
     * This is usually used to explain to the user why a given control is disabled in the settings.
     *
     * @param context The context where the Toast will be shown.
     */
    public static void showManagedByAdministratorToast(Context context) {
        Toast.makeText(context, context.getString(R.string.managed_by_your_organization),
                     Toast.LENGTH_LONG)
                .show();
    }

    /**
     * Shows a toast indicating that the previous action is managed by the parent(s) of the
     * supervised user.
     * This is usually used to explain to the user why a given control is disabled in the settings.
     *
     * @param context The context where the Toast will be shown.
     */
    public static void showManagedByParentToast(
            Context context, @Nullable ManagedPreferenceDelegate delegate) {
        Toast.makeText(context, context.getString(getManagedByParentStringRes(delegate)),
                     Toast.LENGTH_LONG)
                .show();
    }

    /**
     * Shows a toast indicating that some of the preferences in the list of preferences to reset are
     * managed by the system administrator.
     *
     * @param context The context where the Toast will be shown.
     */
    public static void showManagedSettingsCannotBeResetToast(Context context) {
        Toast.makeText(context, context.getString(R.string.managed_settings_cannot_be_reset),
                     Toast.LENGTH_LONG)
                .show();
    }

    /**
     * @return The resource ID for the Managed By Enterprise icon.
     */
    public static @DrawableRes int getManagedByEnterpriseIconId() {
        return R.drawable.ic_business_small;
    }

    /**
     * @return The resource ID for the Managed by Custodian icon.
     */
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
        return resId == 0 ? preference.getIcon()
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
     * @param preference The Preference that is being initialized
     */
    public static void initPreference(
            @Nullable ManagedPreferenceDelegate delegate, Preference preference) {
        if (delegate == null) return;

        if (!(preference instanceof ChromeImageViewPreference)) {
            preference.setIcon(getManagedIconDrawable(delegate, preference));
        }

        if (delegate.isPreferenceClickDisabledByPolicy(preference)) {
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
     * Disables the Preference's views if the preference is not clickable.
     *
     * Note: this disables the View instead of disabling the Preference, so that the Preference
     * still receives click events, which will trigger a "Managed by your administrator" toast.
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

        if (delegate.isPreferenceClickDisabledByPolicy(preference)) {
            ViewUtils.setEnabledRecursive(view, false);
        }

        // Append managed information to summary if necessary.
        TextView summaryView = view.findViewById(android.R.id.summary);
        CharSequence summary =
                ManagedPreferencesUtils.getSummaryWithManagedInfo(delegate, preference,
                        summaryView.getVisibility() == View.VISIBLE ? summaryView.getText() : null);
        if (!TextUtils.isEmpty(summary)) {
            summaryView.setText(summary);
            summaryView.setVisibility(View.VISIBLE);
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
    public static void onBindViewToImageViewPreference(@Nullable ManagedPreferenceDelegate delegate,
            ChromeImageViewPreference preference, View view) {
        if (delegate == null) return;

        onBindViewToPreference(delegate, preference, view);

        if (!delegate.isPreferenceControlledByPolicy(preference)
                && !delegate.isPreferenceControlledByCustodian(preference)) {
            return;
        }

        ImageView button = view.findViewById(R.id.image_view_widget);
        button.setImageDrawable(getManagedIconDrawable(delegate, preference));
        button.setOnClickListener((View v) -> {
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
        if (delegate == null || !delegate.isPreferenceClickDisabledByPolicy(preference)) {
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
     * @param delegate The {@link ManagedPreferenceDelegate} that controls whether the
     *         preference is
     *        managed.
     * @param preference The {@link Preference} that the summary
     *         should be used for.
     * @param summary The original summary without the managed information.
     * @return The summary appended with information about whether the specified preference is
     *         managed.
     */
    private static CharSequence getSummaryWithManagedInfo(
            @Nullable ManagedPreferenceDelegate delegate, Preference preference,
            @Nullable CharSequence summary) {
        if (delegate == null) return summary;

        String extraSummary = null;
        if (delegate.isPreferenceControlledByPolicy(preference)) {
            extraSummary = preference.getContext().getString(R.string.managed_by_your_organization);
        } else if (delegate.isPreferenceControlledByCustodian(preference)) {
            extraSummary = preference.getContext().getString(getManagedByParentStringRes(delegate));
        }

        if (TextUtils.isEmpty(extraSummary)) return summary;
        if (TextUtils.isEmpty(summary)) return extraSummary;
        return String.format(Locale.getDefault(), "%s\n%s", summary, extraSummary);
    }

    private static @StringRes int getManagedByParentStringRes(
            @Nullable ManagedPreferenceDelegate delegate) {
        boolean hasMultipleCustodians = false;
        if (delegate != null) {
            hasMultipleCustodians = delegate.doesProfileHaveMultipleCustodians();
        }
        return hasMultipleCustodians ? R.string.managed_by_your_parents
                                     : R.string.managed_by_your_parent;
    }
}
