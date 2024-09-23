// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.messages.snackbar;

import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.ui.messages.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A snackbar shows a message at the bottom of the screen and optionally contains an action button.
 * To show a snackbar, create the snackbar using {@link #make}, configure it using the various
 * set*() methods, and show it using {@link SnackbarManager#showSnackbar(Snackbar)}. Example:
 *
 *   SnackbarManager.showSnackbar(
 *           Snackbar.make("Closed example.com", controller, Snackbar.UMA_TAB_CLOSE_UNDO)
 *           .setAction("undo", actionData));
 */
public class Snackbar {
    /**
     * Snackbars that are created as an immediate response to user's action. These snackbars are
     * managed in a stack and will be swiped away altogether after timeout.
     */
    public static final int TYPE_ACTION = 0;

    /**
     * Snackbars that are for notification purposes. These snackbars are stored in a queue and thus
     * are of lower priority, compared to {@link #TYPE_ACTION}. Notification snackbars are dismissed
     * one by one.
     */
    public static final int TYPE_NOTIFICATION = 1;

    /**
     * Snackbars that need to persist until acknowledged. These snackbars are stored in a queue and
     * are lower priority than both {@link #TYPE_ACTION}, and {@link #TYPE_NOTIFICATION}. These must
     * be dismissed one by one via a click. As such, snackbars of this type MUST call
     * {@link #setAction(String, Object)} so that there is a way to remove them.
     */
    public static final int TYPE_PERSISTENT = 2;

    /** UMA Identifiers of features using snackbar. See SnackbarIdentifier enum in histograms. */
    public static final int UMA_TEST_SNACKBAR = -2;

    public static final int UMA_UNKNOWN = -1;
    public static final int UMA_BOOKMARK_ADDED = 0;
    public static final int UMA_BOOKMARK_DELETE_UNDO = 1;
    public static final int UMA_NTP_MOST_VISITED_DELETE_UNDO = 2;
    public static final int UMA_OFFLINE_PAGE_RELOAD = 3;
    public static final int UMA_AUTO_LOGIN = 4;
    public static final int UMA_OMNIBOX_GEOLOCATION = 5;
    public static final int UMA_LOFI = 6;
    public static final int UMA_DATA_USE_STARTED = 7;
    public static final int UMA_DATA_USE_ENDED = 8;
    public static final int UMA_DOWNLOAD_SUCCEEDED = 9;
    public static final int UMA_DOWNLOAD_FAILED = 10;
    public static final int UMA_TAB_CLOSE_UNDO = 11;
    public static final int UMA_TAB_CLOSE_ALL_UNDO = 12;
    public static final int UMA_DOWNLOAD_DELETE_UNDO = 13;
    public static final int UMA_SPECIAL_LOCALE = 14;
    // Obsolete; don't use: UMA_BLIMP = 15;
    public static final int UMA_DATA_REDUCTION_PROMO = 16;
    public static final int UMA_HISTORY_LINK_COPIED = 17;
    public static final int UMA_TRANSLATE_ALWAYS = 18;
    public static final int UMA_TRANSLATE_NEVER = 19;
    public static final int UMA_TRANSLATE_NEVER_SITE = 20;
    public static final int UMA_SNIPPET_FETCH_FAILED = 21;
    // Obsolete; don't use: UMA_CHROME_HOME_OPT_OUT_SURVEY = 22;
    public static final int UMA_SNIPPET_FETCH_NO_NEW_SUGGESTIONS = 23;
    public static final int UMA_MISSING_FILES_NO_SD_CARD = 24;
    public static final int UMA_OFFLINE_INDICATOR = 25;
    public static final int UMA_FEED_NTP_STREAM = 26;
    public static final int UMA_WEBAPK_PRIVACY_DISCLOSURE = 27;
    public static final int UMA_TWA_PRIVACY_DISCLOSURE = 28;
    public static final int UMA_AUTOFILL_ASSISTANT_STOP_UNDO = 29;
    public static final int UMA_TAB_CLOSE_MULTIPLE_UNDO = 30;
    public static final int UMA_SEARCH_ENGINE_CHOICE_NOTIFICATION = 31;
    public static final int UMA_TAB_GROUP_MANUAL_CREATION_UNDO = 32;
    public static final int UMA_TWA_PRIVACY_DISCLOSURE_V2 = 33;
    public static final int UMA_HOMEPAGE_PROMO_CHANGED_UNDO = 34;
    // Obsolete; don't use: UMA_CONDITIONAL_TAB_STRIP_DISMISS_UNDO = 35;
    public static final int UMA_PAINT_PREVIEW_UPGRADE_NOTIFICATION = 36;
    public static final int UMA_READING_LIST_BOOKMARK_ADDED = 37;
    public static final int UMA_PRIVACY_SANDBOX_PAGE_OPEN = 38;
    public static final int UMA_WEB_FEED_FOLLOW_SUCCESS = 39;
    public static final int UMA_WEB_FEED_FOLLOW_FAILURE = 40;
    public static final int UMA_WEB_FEED_UNFOLLOW_SUCCESS = 41;
    public static final int UMA_WEB_FEED_UNFOLLOW_FAILURE = 42;
    public static final int UMA_LANGUAGE_SPLIT_RESTART = 43;
    public static final int UMA_AUTOFILL_VIRTUAL_CARD_FILLED = 44;
    public static final int UMA_WINDOW_ERROR = 45;
    public static final int UMA_MODULE_INSTALL_FAILURE = 46;
    public static final int UMA_PRICE_TRACKING_SUCCESS = 47;
    public static final int UMA_PRICE_TRACKING_FAILURE = 48;
    public static final int UMA_PRIVACY_SANDBOX_ADD_INTEREST = 49;
    public static final int UMA_PRIVACY_SANDBOX_REMOVE_INTEREST = 50;
    public static final int UMA_BAD_FLAGS = 51;
    public static final int UMA_DOWNLOAD_INTERSTITIAL_DOWNLOAD_DELETED = 52;
    public static final int UMA_INCOGNITO_REAUTH_ENABLED_FROM_PROMO = 53;
    public static final int UMA_PRIVACY_SANDBOX_ADD_SITE = 54;
    public static final int UMA_PRIVACY_SANDBOX_REMOVE_SITE = 55;
    public static final int UMA_CREATOR_FOLLOW_SUCCESS = 56;
    public static final int UMA_CREATOR_FOLLOW_FAILURE = 57;
    public static final int UMA_CREATOR_UNFOLLOW_SUCCESS = 58;
    public static final int UMA_CREATOR_UNFOLLOW_FAILURE = 59;
    public static final int UMA_QUICK_DELETE = 60;
    public static final int UMA_AUTO_TRANSLATE = 61;
    public static final int UMA_BOOKMARK_MOVED = 62;
    public static final int UMA_CLEAR_BROWSING_DATA = 63;
    public static final int UMA_SIGN_OUT = 64;
    public static final int UMA_TAB_GROUP_DELETE_UNDO = 65;
    public static final int UMA_SINGLE_TAB_GROUP_DELETE_UNDO = 66;
    public static final int UMA_SAFETY_HUB_REGRANT_SINGLE_PERMISSION = 67;
    public static final int UMA_SAFETY_HUB_REGRANT_MULTIPLE_PERMISSIONS = 68;
    public static final int UMA_SAFETY_HUB_SINGLE_SITE_NOTIFICATIONS = 69;
    public static final int UMA_SAFETY_HUB_MULTIPLE_SITE_NOTIFICATIONS = 70;
    public static final int UMA_SETTINGS_BATCH_UPLOAD = 71;

    private @Nullable SnackbarController mController;
    private CharSequence mText;
    private String mTemplateText;
    private String mActionText;
    private Object mActionData;
    private String mAccessibilityActionAnnouncement;
    private int mBackgroundColor;
    private int mTextApperanceResId;
    private boolean mSingleLine = true;
    private int mDurationMs;
    private Drawable mProfileImage;
    private int mType;
    private int mIdentifier = UMA_UNKNOWN;
    private @Theme int mTheme = Theme.BASIC;

    @IntDef({Theme.BASIC, Theme.GOOGLE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Theme {
        int BASIC = 0;
        int GOOGLE = 1;
    }

    // Prevent instantiation.
    private Snackbar() {}

    /**
     * Creates and returns a snackbar to display the given text. If this is a snackbar for a new
     * feature shown to the user, please add the feature name to SnackbarIdentifier in histograms.
     *
     * @param text The text to show on the snackbar.
     * @param controller The SnackbarController to receive callbacks about the snackbar's state. The
     *         controller can be null when no callbacks are required for a snackbar.
     * @param type Type of the snackbar. Either {@link #TYPE_ACTION} or {@link #TYPE_NOTIFICATION}.
     * @param identifier The feature code of the snackbar. Should be one of the UMA* constants above
     */
    public static Snackbar make(
            CharSequence text, @Nullable SnackbarController controller, int type, int identifier) {
        Snackbar s = new Snackbar();
        s.mText = text;
        s.mController = controller;
        s.mType = type;
        s.mIdentifier = identifier;
        if (type == TYPE_PERSISTENT) {
            // For persistent snackbars we set a default action text to ensure the snackbar can be
            // closed.
            s.mActionText =
                    ContextUtils.getApplicationContext().getResources().getString(R.string.ok);
        }
        return s;
    }

    /**
     * Sets the template text to show on the snackbar, e.g. "Closed %s". See
     * {@link TemplatePreservingTextView} for details on how the template text is used.
     */
    public Snackbar setTemplateText(String templateText) {
        mTemplateText = templateText;
        return this;
    }

    /**
     * Sets the action button to show on the snackbar.
     * @param actionText The text to show on the button. If null, the button will not be shown.
     * @param actionData An object to be passed to {@link SnackbarController#onAction} or
     *        {@link SnackbarController#onDismissNoAction} when the button is pressed or the
     *        snackbar is dismissed.
     */
    public Snackbar setAction(String actionText, Object actionData) {
        mActionText = actionText;
        mActionData = actionData;
        return this;
    }

    /**
     * Sets the text to accessibility announce when the action button is pressed.
     * @param accessibilityActionAnnouncement An optional string to be announced when the action
     *        button is pressed.
     */
    public Snackbar setActionAccessibilityAnnouncement(String accessibilityActionAnnouncement) {
        mAccessibilityActionAnnouncement = accessibilityActionAnnouncement;
        return this;
    }

    /**
     * Sets the identity profile image that will be displayed at the beginning of the snackbar.
     * If null, there won't be a profile image. The ability to have an icon is exclusive to
     * identity snackbars.
     */
    public Snackbar setProfileImage(Drawable profileImage) {
        mProfileImage = profileImage;
        return this;
    }

    /**
     * Sets whether the snackbar text should be limited to a single line and ellipsized if needed.
     */
    public Snackbar setSingleLine(boolean singleLine) {
        mSingleLine = singleLine;
        return this;
    }

    /**
     * Sets the number of milliseconds that the snackbar will appear for. If 0, the snackbar will
     * use the default duration.
     */
    public Snackbar setDuration(int durationMs) {
        assert !isTypePersistent() : "Persistent snackbars do not timeout.";
        mDurationMs = durationMs;
        return this;
    }

    /** Sets the background color for the snackbar. If 0, the snackbar will use default color. */
    // TODO(fgorski): Clean up background color and text appearance -- transition all the consumers
    // to the Theme based styling.
    public Snackbar setBackgroundColor(int color) {
        mBackgroundColor = color;
        return this;
    }

    /**
     * Sets the text appearance for the snackbar. If 0, the snackbar will use default text
     * appearance.
     */
    public Snackbar setTextAppearance(int resId) {
        mTextApperanceResId = resId;
        return this;
    }

    /**
     * Sets the theme for the snackbar. If not set, or BASIC, the snackbar will use provided text
     * appearance and background color. Otherwise it will apply selected theme.
     */
    public Snackbar setTheme(@Theme int theme) {
        mTheme = theme;
        return this;
    }

    /**
     * @return The {@link SnackbarController} that controls this snackbar.
     */
    public @Nullable SnackbarController getController() {
        return mController;
    }

    CharSequence getText() {
        return mText;
    }

    String getTemplateText() {
        return mTemplateText;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public String getActionText() {
        return mActionText;
    }

    Object getActionData() {
        return mActionData;
    }

    String getActionAccessibilityAnnouncement() {
        return mAccessibilityActionAnnouncement;
    }

    boolean getSingleLine() {
        return mSingleLine;
    }

    public int getDuration() {
        return mDurationMs;
    }

    int getIdentifier() {
        return mIdentifier;
    }

    /** If method returns zero, then default color for snackbar will be used. */
    int getBackgroundColor() {
        return mBackgroundColor;
    }

    /** If method returns zero, then default text appearance for snackbar will be used. */
    int getTextAppearance() {
        return mTextApperanceResId;
    }

    /**
     * If method returns BASIC, them background color and text appearance is used, otherwise a
     * requested theme will be applied to style the Snackbar.
     */
    @Theme
    int getTheme() {
        return mTheme;
    }

    /** If method returns null, then no profileImage will be shown in snackbar. */
    Drawable getProfileImage() {
        return mProfileImage;
    }

    /**
     * @return Whether the snackbar is of {@link #TYPE_ACTION}.
     */
    boolean isTypeAction() {
        return mType == TYPE_ACTION;
    }

    /**
     * @return Whether the snackbar is of {@link #TYPE_PERSISTENT}.
     */
    boolean isTypePersistent() {
        return mType == TYPE_PERSISTENT;
    }

    /** So tests can trigger a press on a Snackbar. */
    public Object getActionDataForTesting() {
        return mActionData;
    }

    public int getIdentifierForTesting() {
        return mIdentifier;
    }

    public CharSequence getTextForTesting() {
        return mText;
    }
}
