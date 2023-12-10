// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;

import android.content.Context;
import android.content.DialogInterface;
import android.content.res.Resources;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.os.Vibrator;
import android.provider.Settings;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.CheckBoxWithDescription;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.text.EmptyTextWatcher;

/** A utility class for the UI recording exceptions to the blocked list for site settings. */
public class AddExceptionPreference extends Preference
        implements Preference.OnPreferenceClickListener {
    // The callback to notify when the user adds a site.
    private SiteAddedCallback mSiteAddedCallback;

    // The accent color to use for the icon and title view.
    private int mPrefAccentColor;

    // The custom message to show in the dialog.
    private String mDialogMessage;

    // The Site Settings Category of the exception we are adding.
    private final SiteSettingsCategory mCategory;

    // The colors for the site URL EditText
    private int mErrorColor;
    private int mDefaultColor;

    /** An interface to implement to get a callback when a site exception needs to be added. */
    public interface SiteAddedCallback {
        /**
         * The callback for the site exception that needs to be added.
         *
         * @param primaryPattern The primary pattern for the exception, usually the hostname to add,
         *     or the wildcard indicating all hosts
         * @param secondaryPattern The secondary pattern for the exception, indicating on which
         *     sites the primary pattern is affected. Usually the wildcard or a specific host (for
         *     third-party cookies).
         */
        public void onAddSite(String primaryPattern, String secondaryPattern);
    }

    /**
     * Construct a AddException preference.
     *
     * @param context The current context.
     * @param key The key to use for the preference.
     * @param message The custom message to show in the dialog.
     * @param callback A callback to receive notifications that an exception has been added.
     */
    public AddExceptionPreference(
            Context context,
            String key,
            String message,
            SiteSettingsCategory category,
            SiteAddedCallback callback) {
        super(context);
        mDialogMessage = message;
        mCategory = category;
        mSiteAddedCallback = callback;
        setOnPreferenceClickListener(this);

        setKey(key);
        Resources resources = context.getResources();
        mPrefAccentColor = SemanticColorUtils.getDefaultControlColorActive(context);
        mErrorColor = context.getColor(R.color.default_red);
        mDefaultColor =
                AppCompatResources.getColorStateList(context, R.color.default_text_color_list)
                        .getDefaultColor();

        Drawable plusIcon = ApiCompatibilityUtils.getDrawable(resources, R.drawable.plus);
        plusIcon.mutate();
        plusIcon.setColorFilter(mPrefAccentColor, PorterDuff.Mode.SRC_IN);
        setIcon(plusIcon);

        setTitle(resources.getString(R.string.website_settings_add_site));
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        TextView titleView = (TextView) holder.findViewById(android.R.id.title);
        titleView.setTextColor(mPrefAccentColor);
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        showAddExceptionDialog();
        return true;
    }

    /** Show the dialog allowing the user to add a new website as an exception. */
    private void showAddExceptionDialog() {
        LayoutInflater inflater =
                (LayoutInflater) getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View view = inflater.inflate(R.layout.add_site_dialog, null);
        final EditText input = view.findViewById(R.id.site);
        final CheckBoxWithDescription checkBox = view.findViewById(R.id.add_site_dialog_checkbox);

        if (mCategory.getType() == SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE) {
            // Default to domain level setting for Request Desktop Site.
            checkBox.setChecked(true);
            checkBox.setVisibility(View.VISIBLE);
            int primary = R.string.website_settings_domain_desktop_site_exception_checkbox_primary;
            int description =
                    R.string.website_settings_domain_desktop_site_exception_checkbox_description;
            checkBox.setPrimaryText(getContext().getString(primary));
            checkBox.setDescriptionText(getContext().getString(description));
        }

        DialogInterface.OnClickListener onClickListener =
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int button) {
                        if (button == AlertDialog.BUTTON_POSITIVE) {
                            int categoryType = mCategory.getType();
                            boolean isChecked = checkBox.isChecked();
                            String pattern = input.getText().toString().trim();
                            pattern = updatePatternIfNeeded(pattern, categoryType, isChecked);
                            String primary = getPrimaryPattern(pattern, categoryType, isChecked);
                            String secondary =
                                    getSecondaryPattern(pattern, categoryType, isChecked);
                            mSiteAddedCallback.onAddSite(primary, secondary);
                        } else {
                            dialog.dismiss();
                        }
                    }
                };

        AlertDialog.Builder alert =
                new AlertDialog.Builder(getContext(), R.style.ThemeOverlay_BrowserUI_AlertDialog);
        AlertDialog alertDialog =
                alert.setTitle(R.string.website_settings_add_site_dialog_title)
                        .setMessage(mDialogMessage)
                        .setView(view)
                        .setPositiveButton(
                                R.string.website_settings_add_site_add_button, onClickListener)
                        .setNegativeButton(R.string.cancel, onClickListener)
                        .create();
        alertDialog.getDelegate().setHandleNativeActionModesEnabled(false);
        alertDialog.setOnShowListener(
                new DialogInterface.OnShowListener() {
                    @Override
                    public void onShow(DialogInterface dialog) {
                        KeyboardVisibilityDelegate.getInstance().showKeyboard(input);
                    }
                });
        alertDialog.show();
        final Button okButton = alertDialog.getButton(AlertDialog.BUTTON_POSITIVE);
        okButton.setEnabled(false);

        input.addTextChangedListener(
                new EmptyTextWatcher() {
                    @Override
                    public void onTextChanged(CharSequence s, int start, int before, int count) {
                        // The intent is to capture a url pattern and register it as an exception.
                        // But a pattern can be used to express things that are not supported, such
                        // as domains, schemes and ports. Therefore we need to filter out invalid
                        // values before passing them on to the validity checker for patterns.
                        String pattern = s.toString().trim();
                        boolean isValid = isPatternValid(pattern, mCategory.getType());

                        // Vibrate when adding characters only, not when deleting them.
                        if (!isValid && count != 0) {
                            if (Settings.System.getInt(
                                            getContext().getContentResolver(),
                                            Settings.System.HAPTIC_FEEDBACK_ENABLED,
                                            1)
                                    == 1) {
                                ((Vibrator) getContext().getSystemService(Context.VIBRATOR_SERVICE))
                                        .vibrate(50);
                            }
                        }

                        okButton.setEnabled(isValid && pattern.length() > 0);
                        input.setTextColor(isValid ? mDefaultColor : mErrorColor);
                    }
                });
    }

    @VisibleForTesting
    static String updatePatternIfNeeded(@NonNull String pattern, int type, boolean isChecked) {
        if (type == SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE) {
            if (isChecked) {
                return WebsitePreferenceBridge.toDomainWildcardPattern(pattern);
            } else {
                return WebsitePreferenceBridge.toHostOnlyPattern(pattern);
            }
        }
        return pattern;
    }

    @VisibleForTesting
    static String getPrimaryPattern(@NonNull String pattern, int type, boolean isChecked) {
        if (type == SiteSettingsCategory.Type.THIRD_PARTY_COOKIES) {
            return SITE_WILDCARD;
        }
        return pattern;
    }

    @VisibleForTesting
    static String getSecondaryPattern(@NonNull String pattern, int type, boolean isChecked) {
        if (type == SiteSettingsCategory.Type.THIRD_PARTY_COOKIES) {
            return pattern;
        }
        return SITE_WILDCARD;
    }

    @VisibleForTesting
    static boolean isPatternValid(@NonNull String pattern, int type) {
        if (pattern.length() == 0) {
            return true;
        }
        if (pattern.contains(":") && !isColonAllowed(type)) {
            return false;
        }
        if (pattern.contains(" ") || pattern.startsWith(".")) {
            return false;
        }
        return WebsitePreferenceBridgeJni.get().isContentSettingsPatternValid(pattern);
    }

    private static boolean isColonAllowed(int type) {
        return type == SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE;
    }
}
