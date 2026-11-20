// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.UserCriticalFeature;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntDefPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableLongPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Shared model properties for an {@link AdaptiveUiEntry} presented as a bottom sheet or a modal
 * dialog. The model holds what the entry shows, its callbacks (including the lifecycle callbacks
 * {@link #ON_SHOWN} and {@link #ON_DISMISSED}) and its container-specific options; {@link
 * AdaptiveUiEntry} holds where the entry may be shown (scope and allowed containers).
 *
 * <p>Features should handle each user response once. Handle the primary button, and a secondary
 * button with {@link SecondaryButtonBehavior#ACTION_THEN_DISMISS} or {@link
 * SecondaryButtonBehavior#ACTION_THEN_STAY_OPEN}, in {@link #ON_PRIMARY_BUTTON_CLICKED} and {@link
 * #ON_SECONDARY_BUTTON_CLICKED}. Handle closes that run no callback, such as the close ('X') button
 * or a {@link SecondaryButtonBehavior#CLOSE} button (reported as {@link
 * DismissReason#SECONDARY_ACTION}), in {@link #ON_DISMISSED}.
 */
@NullMarked
public class AdaptiveUiProperties {
    /**
     * Optional callback run at most once per {@link AdaptiveUiController#show}, when the container
     * starts showing the entry (a dialog right after it is shown, a sheet once it takes the entry's
     * content). Receives the {@link AdaptiveUiContainer} that presents the entry. Not run if the
     * entry is closed while it still waits in a queue, and not run again if the container
     * temporarily hides the entry and shows it again. Read-only so it stays the same for the whole
     * show.
     */
    public static final ReadableObjectPropertyKey<@Nullable Callback<Integer>> ON_SHOWN =
            new ReadableObjectPropertyKey<>();

    /**
     * Optional callback run exactly once per {@link AdaptiveUiController#show}, when the entry is
     * closed, whether or not it was ever shown. Receives the {@link DismissReason}. Features can
     * rely on it to release what they set up for the entry. Read-only so it stays the same for the
     * whole show.
     */
    public static final ReadableObjectPropertyKey<@Nullable Callback<Integer>> ON_DISMISSED =
            new ReadableObjectPropertyKey<>();

    /** Optional icon displayed in the header. */
    public static final WritableObjectPropertyKey<@Nullable Drawable> HEADER_ICON =
            new WritableObjectPropertyKey<>();

    /**
     * Title text displayed in the header. A dialog needs a title or a {@link
     * #DIALOG_CONTENT_DESCRIPTION} when it is created.
     */
    public static final WritableObjectPropertyKey<@Nullable CharSequence> TITLE =
            new WritableObjectPropertyKey<>();

    /**
     * Accessibility content description for dialogs; sheets ignore it. Required when {@link #TITLE}
     * is empty, which the dialog asserts when it is created. Read-only because the dialog applies
     * it once, at creation.
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

    /**
     * Bottom sheet only: the {@link SheetState} the sheet opens in. Dialogs ignore it. Read-only.
     *
     * <p>{@link SheetState#FULL} (the default when unset) sizes the sheet to its content. {@link
     * SheetState#HALF} opens the sheet at the half height, and the user can drag it to the full
     * height. The half state is skipped on small screens, where the sheet opens at the full height.
     * A half sheet's heights can be set with {@link #SHEET_HALF_HEIGHT_RATIO} and {@link
     * #SHEET_FULL_HEIGHT_RATIO}.
     *
     * <p>{@link AdaptiveUiEntry.Builder} throws {@link IllegalArgumentException} for any other
     * {@link SheetState}, whichever containers the entry allows.
     */
    public static final ReadableIntDefPropertyKey<SheetState> SHEET_INITIAL_STATE =
            new ReadableIntDefPropertyKey<>(SheetState.FULL);

    /**
     * Bottom sheet only: whether the sheet is triggered directly by an explicit user action rather
     * than passively by browser state. Used for bottom sheet precedence. Dialogs ignore it.
     * Read-only. Defaults to {@code false}.
     */
    public static final ReadableBooleanPropertyKey SHEET_USER_INITIATED =
            new ReadableBooleanPropertyKey();

    /**
     * Bottom sheet only: an approved {@link UserCriticalFeature} constant for top bottom sheet
     * precedence. Dialogs ignore it. Read-only. Defaults to {@link UserCriticalFeature#NONE}.
     */
    public static final ReadableIntPropertyKey SHEET_USER_CRITICAL = new ReadableIntPropertyKey();

    /**
     * Optional half height of a bottom sheet opened at {@code SheetState.HALF} (see {@link
     * #SHEET_INITIAL_STATE}), as a ratio of the sheet container height in (0, 1]. Leaving this
     * unset ({@code 0}) keeps the default bottom sheet half height of {@code 0.75}. Must not be
     * above the full height ({@link #SHEET_FULL_HEIGHT_RATIO}, or {@code 1} if unset). Ignored by
     * full-height sheets and dialogs.
     */
    public static final ReadableFloatPropertyKey SHEET_HALF_HEIGHT_RATIO =
            new ReadableFloatPropertyKey();

    /**
     * Optional full height of a bottom sheet opened at {@code SheetState.HALF} (see {@link
     * #SHEET_INITIAL_STATE}), as a ratio of the sheet container height in (0, 1]. Leaving this
     * unset ({@code 0}) keeps the default bottom sheet full height of {@code 1}. Must not be below
     * the half height ({@link #SHEET_HALF_HEIGHT_RATIO}, or {@code 0.75} if unset). Ignored by
     * full-height sheets and dialogs.
     *
     * <p>Also ignored on small screens: the bottom sheet disables the half state there, so the
     * sheet opens at the full container height.
     */
    public static final ReadableFloatPropertyKey SHEET_FULL_HEIGHT_RATIO =
            new ReadableFloatPropertyKey();

    /** Body description text displayed below the title. */
    public static final WritableObjectPropertyKey<@Nullable CharSequence> DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /** Visibility policy for the close ('X') button on bottom sheets and dialogs. */
    @IntDef({
        CloseButtonVisibility.DEFAULT,
        CloseButtonVisibility.SHOWN,
        CloseButtonVisibility.HIDDEN
    })
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    public @interface CloseButtonVisibility {
        /** Bottom sheets hide the close ('X') button; dialogs show it in the title. */
        int DEFAULT = 0;

        /** Both bottom sheets and dialogs show the close ('X') button. */
        int SHOWN = 1;

        /** Neither shows the close ('X') button. */
        int HIDDEN = 2;
    }

    /**
     * Whether to show the close ('X') button, as a {@link CloseButtonVisibility}. Tapping it closes
     * the entry with {@link DismissReason#CLOSE_BUTTON}. Read-only. Defaults to {@link
     * CloseButtonVisibility#DEFAULT}.
     */
    public static final ReadableIntPropertyKey CLOSE_BUTTON_VISIBILITY =
            new ReadableIntPropertyKey();

    /**
     * Bottom sheet only: whether the sheet shows a handlebar at the top, such as to keep a
     * feature's existing sheet unchanged. Dialogs ignore it. Read-only. Defaults to {@code false}.
     */
    public static final ReadableBooleanPropertyKey SHOW_SHEET_HANDLEBAR =
            new ReadableBooleanPropertyKey();

    /** Optional footer text displayed at the bottom of the surface. */
    public static final WritableObjectPropertyKey<@Nullable CharSequence> FOOTER_MESSAGE =
            new WritableObjectPropertyKey<>();

    /**
     * Optional custom back-press handler. When unset or disabled, a back press closes the entry
     * with {@link DismissReason#BACK_PRESS}.
     */
    public static final WritableObjectPropertyKey<@Nullable OnBackPressedCallback>
            BACK_PRESS_HANDLER = new WritableObjectPropertyKey<>();

    /**
     * Whether to disable accidental gesture dismissal. On bottom sheets, disables velocity-assisted
     * swipe dismissal ({@code BottomSheetContent#swipeToDismissEnabled()}); tapping the scrim and
     * dragging past the close threshold still close non-peeking sheets. On modal dialogs, disables
     * outside-touch dismissal ({@code ModalDialogProperties#CANCEL_ON_TOUCH_OUTSIDE}). Back press
     * still closes the entry unless {@link #BACK_PRESS_HANDLER} consumes it. Defaults to {@code
     * false}.
     */
    public static final WritableBooleanPropertyKey GESTURE_DISMISSAL_DISABLED =
            new WritableBooleanPropertyKey();

    /**
     * Optional custom body {@link View} (such as inputs or lists) shown below the description. The
     * view supplies its own padding: neither container adds side padding, and the sheet adds no
     * space above it, or between it and the primary button when that follows directly. A footer or
     * a lone secondary button below it keeps the sheet's spacing.
     */
    public static final WritableObjectPropertyKey<@Nullable View> CUSTOM_VIEW =
            new WritableObjectPropertyKey<>();

    /**
     * Whether {@link #CUSTOM_VIEW} scrolls its own content (such as a list or a web view). When
     * {@code true}, the body is placed outside the container's scroll view and gets the height left
     * between the description and the footer, so it must scroll whatever doesn't fit. A sheet can
     * then only be dragged down while neither the body nor any visible view inside it can scroll
     * up. Set this whenever the body scrolls by itself: a scrolling body left inside the
     * container's scroll view is laid out at its full height and fights that scroll view for
     * touches. Read-only because the containers pick the layout once, when the entry is shown.
     * Defaults to {@code false}.
     */
    public static final ReadableBooleanPropertyKey CUSTOM_VIEW_SCROLLS_ITSELF =
            new ReadableBooleanPropertyKey();

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
     * Callback invoked when the secondary button is clicked for {@link
     * SecondaryButtonBehavior#ACTION_THEN_DISMISS} (before the framework closes the entry) and
     * {@link SecondaryButtonBehavior#ACTION_THEN_STAY_OPEN}. Not invoked for {@link
     * SecondaryButtonBehavior#CLOSE} or when the close ('X') button is clicked.
     */
    public static final WritableObjectPropertyKey<@Nullable Runnable> ON_SECONDARY_BUTTON_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Whether the secondary action button is disabled. Defaults to {@code false} (enabled). */
    public static final WritableBooleanPropertyKey SECONDARY_BUTTON_DISABLED =
            new WritableBooleanPropertyKey();

    /** How the secondary button behaves when clicked, and when dialogs draw it alongside 'X'. */
    @IntDef({
        SecondaryButtonBehavior.ACTION_THEN_DISMISS,
        SecondaryButtonBehavior.ACTION_THEN_STAY_OPEN,
        SecondaryButtonBehavior.CLOSE
    })
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    public @interface SecondaryButtonBehavior {
        /**
         * A real choice or secondary action, such as starting another flow or recording an explicit
         * decision. Drawn on both bottom sheets and dialogs. Runs {@link
         * AdaptiveUiProperties#ON_SECONDARY_BUTTON_CLICKED}, then closes the entry with {@link
         * DismissReason#SECONDARY_ACTION}.
         */
        int ACTION_THEN_DISMISS = 0;

        /**
         * An in-place secondary action. Drawn on both bottom sheets and dialogs. Runs {@link
         * AdaptiveUiProperties#ON_SECONDARY_BUTTON_CLICKED} and keeps the entry open.
         */
        int ACTION_THEN_STAY_OPEN = 1;

        /**
         * Only closes the entry, like the close ('X') button, Esc, back, or a scrim tap (such as
         * "No thanks"). Never runs {@link AdaptiveUiProperties#ON_SECONDARY_BUTTON_CLICKED}; handle
         * the outcome in {@link AdaptiveUiProperties#ON_DISMISSED}. Bottom sheets and dialogs with
         * {@link CloseButtonVisibility#HIDDEN} draw this button and close the entry with {@link
         * DismissReason#SECONDARY_ACTION}; dialogs omit it while the close ('X') button is shown.
         */
        int CLOSE = 2;
    }

    /**
     * How the secondary button behaves, as a {@link SecondaryButtonBehavior}. Read-only. Defaults
     * to {@link SecondaryButtonBehavior#ACTION_THEN_DISMISS}.
     */
    public static final ReadableIntPropertyKey SECONDARY_BUTTON_BEHAVIOR =
            new ReadableIntPropertyKey();

    /**
     * Time in milliseconds after the entry is shown during which primary and secondary button
     * clicks are ignored to prevent unintended taps. For sheets, the period starts once the sheet
     * has finished opening. Each tap on these buttons restarts the period. Does not apply to the
     * header or dialog title close ('X') button (a {@link SecondaryButtonBehavior#CLOSE} secondary
     * is not drawn on dialogs while the close ('X') button is shown). Must not be negative.
     * Defaults to {@code 0} (off).
     */
    public static final WritableLongPropertyKey BUTTON_TAP_PROTECTION_PERIOD_MS =
            new WritableLongPropertyKey();

    /**
     * Whether to filter touches on the primary and secondary buttons when another visible window
     * wholly or partially obscures this window. Read-only because dialogs cannot turn touch
     * filtering off once enabled. Dialogs apply it only if the entry has a drawn button when shown
     * (a {@link SecondaryButtonBehavior#CLOSE} secondary is not drawn on dialogs while the close
     * ('X') button is shown): a button added later is not filtered, and removing every drawn button
     * while shown trips a ModalDialog assert. Does not filter the header or dialog title close
     * ('X') button. Defaults to {@code false} (off).
     */
    public static final ReadableBooleanPropertyKey FILTER_TOUCH_FOR_SECURITY =
            new ReadableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ON_SHOWN,
                ON_DISMISSED,
                HEADER_ICON,
                TITLE,
                DIALOG_CONTENT_DESCRIPTION,
                SHEET_OPENED_ACCESSIBILITY_STRING_ID,
                SHEET_CLOSED_ACCESSIBILITY_STRING_ID,
                SHEET_INITIAL_STATE,
                SHEET_USER_INITIATED,
                SHEET_USER_CRITICAL,
                SHEET_HALF_HEIGHT_RATIO,
                SHEET_FULL_HEIGHT_RATIO,
                DESCRIPTION,
                CLOSE_BUTTON_VISIBILITY,
                SHOW_SHEET_HANDLEBAR,
                FOOTER_MESSAGE,
                BACK_PRESS_HANDLER,
                GESTURE_DISMISSAL_DISABLED,
                CUSTOM_VIEW,
                CUSTOM_VIEW_SCROLLS_ITSELF,
                PRIMARY_BUTTON_TEXT,
                ON_PRIMARY_BUTTON_CLICKED,
                PRIMARY_BUTTON_DISABLED,
                PRIMARY_BUTTON_KEEPS_OPEN,
                SECONDARY_BUTTON_TEXT,
                ON_SECONDARY_BUTTON_CLICKED,
                SECONDARY_BUTTON_DISABLED,
                SECONDARY_BUTTON_BEHAVIOR,
                BUTTON_TAP_PROTECTION_PERIOD_MS,
                FILTER_TOUCH_FOR_SECURITY,
            };

    private AdaptiveUiProperties() {}
}
