// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.activity.OnBackPressedCallback;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableLongPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Shared model properties for an {@link AdaptiveUiEntry} presented as a bottom sheet or a modal
 * dialog.
 */
@NullMarked
public class AdaptiveUiProperties {
    /** Optional icon displayed in the header. */
    public static final WritableObjectPropertyKey<@Nullable Drawable> HEADER_ICON =
            new WritableObjectPropertyKey<>();

    /** Title text displayed in the header. */
    public static final WritableObjectPropertyKey<@Nullable CharSequence> TITLE =
            new WritableObjectPropertyKey<>();

    /**
     * Accessibility content description for the dialog when {@link #TITLE} is not set (used by
     * dialogs only). Read-only because the dialog applies it once, when the dialog is created.
     */
    public static final ReadableObjectPropertyKey<@Nullable String> DIALOG_CONTENT_DESCRIPTION =
            new ReadableObjectPropertyKey<>();

    /**
     * Optional {@code @StringRes} string TalkBack announces when the bottom sheet opens (at half or
     * full height), typically the name of your feature followed by 'opened'. Leaving this unset
     * ({@code 0}) keeps the generic bottom sheet description. Ignored by dialogs (which use {@link
     * #TITLE} or {@link #DIALOG_CONTENT_DESCRIPTION}).
     */
    public static final ReadableIntPropertyKey SHEET_OPENED_ACCESSIBILITY_STRING_ID =
            new ReadableIntPropertyKey();

    /**
     * Optional {@code @StringRes} string TalkBack announces when the bottom sheet closes or hides,
     * typically the name of your feature followed by 'closed'. Leaving this unset ({@code 0}) keeps
     * the generic bottom sheet description. Ignored by dialogs (which use {@link #TITLE} or {@link
     * #DIALOG_CONTENT_DESCRIPTION}).
     */
    public static final ReadableIntPropertyKey SHEET_CLOSED_ACCESSIBILITY_STRING_ID =
            new ReadableIntPropertyKey();

    /** Body description text displayed below the title. */
    public static final WritableObjectPropertyKey<@Nullable CharSequence> DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /** Whether to show a close ('X') button in the header. Defaults to {@code false}. */
    public static final WritableBooleanPropertyKey SHOW_CLOSE_BUTTON =
            new WritableBooleanPropertyKey();

    /** Optional footer text displayed at the bottom of the surface. */
    public static final WritableObjectPropertyKey<@Nullable CharSequence> FOOTER_MESSAGE =
            new WritableObjectPropertyKey<>();

    /**
     * Optional custom back-press handler. When unset or disabled, a back press closes the entry
     * with {@link DismissReason#BACK_PRESS}.
     */
    public static final WritableObjectPropertyKey<@Nullable OnBackPressedCallback>
            BACK_PRESS_HANDLER = new WritableObjectPropertyKey<>();

    /** Optional custom body {@link View} (such as inputs or lists) shown below the description. */
    public static final WritableObjectPropertyKey<@Nullable View> CUSTOM_VIEW =
            new WritableObjectPropertyKey<>();

    /** Text for the primary action button. Leaving this unset hides the primary button. */
    public static final WritableObjectPropertyKey<@Nullable String> PRIMARY_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    /**
     * Callback invoked when the primary button is clicked, before the framework closes the entry
     * (unless {@link #PRIMARY_BUTTON_KEEPS_OPEN} is {@code true}).
     */
    public static final WritableObjectPropertyKey<@Nullable Runnable> ON_PRIMARY_BUTTON_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Whether the primary action button is disabled. Defaults to {@code false} (enabled). */
    public static final WritableBooleanPropertyKey PRIMARY_BUTTON_DISABLED =
            new WritableBooleanPropertyKey();

    /**
     * Whether clicking the primary button keeps the entry open instead of automatically closing it
     * with {@link DismissReason#PRIMARY_ACTION}. Defaults to {@code false}.
     */
    public static final WritableBooleanPropertyKey PRIMARY_BUTTON_KEEPS_OPEN =
            new WritableBooleanPropertyKey();

    /** Text for the secondary action button. Leaving this unset hides the secondary button. */
    public static final WritableObjectPropertyKey<@Nullable String> SECONDARY_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    /**
     * Callback invoked when the secondary button is clicked, before the framework closes the entry
     * (unless {@link #SECONDARY_BUTTON_KEEPS_OPEN} is {@code true}).
     */
    public static final WritableObjectPropertyKey<@Nullable Runnable> ON_SECONDARY_BUTTON_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Whether the secondary action button is disabled. Defaults to {@code false} (enabled). */
    public static final WritableBooleanPropertyKey SECONDARY_BUTTON_DISABLED =
            new WritableBooleanPropertyKey();

    /**
     * Whether clicking the secondary button keeps the entry open instead of automatically closing
     * it with {@link DismissReason#SECONDARY_ACTION}. Defaults to {@code false}.
     */
    public static final WritableBooleanPropertyKey SECONDARY_BUTTON_KEEPS_OPEN =
            new WritableBooleanPropertyKey();

    /**
     * Time in milliseconds after the entry is shown during which primary and secondary button
     * clicks are ignored to prevent unintended taps. For sheets, the period starts once the sheet
     * has finished opening. Each tap on these buttons restarts the period. Must not be negative.
     * Defaults to {@code 0} (off).
     */
    public static final WritableLongPropertyKey BUTTON_TAP_PROTECTION_PERIOD_MS =
            new WritableLongPropertyKey();

    /**
     * Whether to filter touches on the primary and secondary buttons when another visible window
     * wholly or partially obscures this window. Read-only because dialogs cannot turn touch
     * filtering off once enabled. Dialogs apply it only if the entry has an action button when
     * shown: a button added later is not filtered, and removing every action button while shown
     * trips a ModalDialog assert. Defaults to {@code false} (off).
     */
    public static final ReadableBooleanPropertyKey FILTER_TOUCH_FOR_SECURITY =
            new ReadableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                HEADER_ICON,
                TITLE,
                DIALOG_CONTENT_DESCRIPTION,
                SHEET_OPENED_ACCESSIBILITY_STRING_ID,
                SHEET_CLOSED_ACCESSIBILITY_STRING_ID,
                DESCRIPTION,
                SHOW_CLOSE_BUTTON,
                FOOTER_MESSAGE,
                BACK_PRESS_HANDLER,
                CUSTOM_VIEW,
                PRIMARY_BUTTON_TEXT,
                ON_PRIMARY_BUTTON_CLICKED,
                PRIMARY_BUTTON_DISABLED,
                PRIMARY_BUTTON_KEEPS_OPEN,
                SECONDARY_BUTTON_TEXT,
                ON_SECONDARY_BUTTON_CLICKED,
                SECONDARY_BUTTON_DISABLED,
                SECONDARY_BUTTON_KEEPS_OPEN,
                BUTTON_TAP_PROTECTION_PERIOD_MS,
                FILTER_TOUCH_FOR_SECURITY,
            };

    private AdaptiveUiProperties() {}
}
