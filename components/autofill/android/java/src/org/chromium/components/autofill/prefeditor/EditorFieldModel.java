// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.prefeditor;

import android.content.SharedPreferences;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.Pair;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Representation of a single input text field in an editor. Can be used, for example, for a phone
 * input field.
 */
public class EditorFieldModel {
    /**
     * The interface to be implemented by the field validator.
     */
    public interface EditorFieldValidator {
        /**
         * Called to check the validity of the field value.
         *
         * @param value The value of the field to check.
         * @return True if the value is valid.
         */
        boolean isValid(@Nullable CharSequence value);

        /**
         * Called to check whehter the length of the field value is maximum.
         *
         * @param value The value of the field to check.
         * @return True if the field value length is maximum among all the possible valid values in
         *         this field.
         */
        boolean isLengthMaximum(@Nullable CharSequence value);
    }

    /**
     * The interface to be implemented by the field value icon generator.
     */
    public interface EditorValueIconGenerator {
        /**
         * Called to get the field value icon resource Id.
         * @param value The value of the field.
         * @return The resouce Id of the value icon, 0 indicates no icon.
         */
        int getIconResourceId(@Nullable CharSequence value);
    }

    /**
     * A convenience class for displaying keyed values in a dropdown.
     */
    public static class DropdownKeyValue extends Pair<String, CharSequence> {
        public DropdownKeyValue(String key, CharSequence value) {
            super(key, value);
        }

        /** @return The key identifier. */
        public String getKey() {
            return super.first;
        }

        /** @return The human-readable localized display value. */
        public CharSequence getValue() {
            return super.second;
        }

        @Override
        public String toString() {
            return super.second.toString();
        }
    }

    private static final int INPUT_TYPE_HINT_MIN_INCLUSIVE = 0;

    /** Text input with no special formatting rules, e.g., a city, a suburb, or a company name. */
    public static final int INPUT_TYPE_HINT_NONE = 0;

    /** Indicates a phone field. */
    public static final int INPUT_TYPE_HINT_PHONE = 1;

    /** Indicates an email field. */
    public static final int INPUT_TYPE_HINT_EMAIL = 2;

    /** Indicates a multi-line address field that may include numbers. */
    public static final int INPUT_TYPE_HINT_STREET_LINES = 3;

    /** Indicates a person's name. */
    public static final int INPUT_TYPE_HINT_PERSON_NAME = 4;

    /** Indicates a region or an administrative area, e.g., a state or a province. */
    public static final int INPUT_TYPE_HINT_REGION = 5;

    /** Indicates an alpha-numeric value, e.g., postal code or sorting code. */
    public static final int INPUT_TYPE_HINT_ALPHA_NUMERIC = 6;

    /** Indicates a credit card input. */
    public static final int INPUT_TYPE_HINT_CREDIT_CARD = 7;

    /** Indicates a numeric field. */
    public static final int INPUT_TYPE_HINT_NUMERIC = 8;

    /** Indicates a password input. */
    public static final int INPUT_TYPE_HINT_PASSWORD = 9;

    /** Indicates a numeric pin input. */
    public static final int INPUT_TYPE_HINT_NUMERIC_PIN = 10;

    private static final int INPUT_TYPE_HINT_MAX_TEXT_INPUT_EXCLUSIVE = 11;

    /** Indicates a dropdown. */
    public static final int INPUT_TYPE_HINT_DROPDOWN = 12;

    /** Indicates a list of icons. */
    public static final int INPUT_TYPE_HINT_ICONS = 13;

    /** Indicates a checkbox. */
    public static final int INPUT_TYPE_HINT_CHECKBOX = 14;
    /**
     * Indicates a label, e.g., for a server credit card.
     *
     *  TOP_LABEL
     *  MID_LABEL     [ICON]
     *  BOTTOM_LABEL
     *
     *  Example:
     *
     *  Visa***1234
     *  First Last    [VISA]
     *  Exp: 03/2021
     */
    public static final int INPUT_TYPE_HINT_LABEL = 15;

    private static final int INPUT_TYPE_HINT_MAX_EXCLUSIVE = 16;

    /* Indicates that the length counter is disabled. */
    public static final int LENGTH_COUNTER_LIMIT_NONE = 0;

    private final int mInputTypeHint;

    @Nullable
    private List<Integer> mIconResourceIds;
    @Nullable
    private List<Integer> mIconDescriptionsForAccessibility;
    @Nullable
    private List<DropdownKeyValue> mDropdownKeyValues;
    @Nullable
    private HashMap<String, CharSequence> mDropdownKeyToValueMap;
    @Nullable
    private HashMap<String, String> mDropdownValueToKeyMap;
    @Nullable
    private Set<String> mDropdownKeys;
    @Nullable
    private List<CharSequence> mSuggestions;
    @Nullable
    private TextWatcher mFormatter;
    @Nullable
    private EditorFieldValidator mValidator;
    @Nullable
    private EditorValueIconGenerator mValueIconGenerator;
    @Nullable
    private CharSequence mRequiredErrorMessage;
    @Nullable
    private CharSequence mInvalidErrorMessage;
    @Nullable
    private CharSequence mCustomErrorMessage;
    @Nullable
    private CharSequence mErrorMessage;
    @Nullable
    private CharSequence mLabel;
    @Nullable
    private CharSequence mMidLabel;
    @Nullable
    private CharSequence mBottomLabel;
    @Nullable
    private CharSequence mValue;
    @Nullable
    private CharSequence mHint;
    @Nullable
    private Callback<Pair<String, Runnable>> mDropdownCallback;
    @Nullable
    private Runnable mActionIconAction;
    private int mLabelIconResourceId;
    private int mActionIconResourceId;
    private int mActionIconDescriptionForAccessibility;
    private boolean mIsFullLine = true;
    private boolean mPlusIconIsDisplayed;
    private int mLengthCounterLimit = LENGTH_COUNTER_LIMIT_NONE;

    /**
     * Constructs a label to show in the editor. This can be, for example, description of a server
     * credit card and its icon. Layout:
     *
     *  topLabel
     *  midLabel      iconId
     *  bottomLabel
     *
     * @param topLabel    Top label.
     * @param midLabel    Middle label.
     * @param bottomLabel Bottom label.
     * @param iconId      Icon.
     */
    public static EditorFieldModel createLabel(
            CharSequence topLabel, CharSequence midLabel, CharSequence bottomLabel, int iconId) {
        assert topLabel != null;
        assert midLabel != null;
        assert bottomLabel != null;
        EditorFieldModel result = new EditorFieldModel(INPUT_TYPE_HINT_LABEL);
        result.mLabel = topLabel;
        result.mMidLabel = midLabel;
        result.mBottomLabel = bottomLabel;
        result.mLabelIconResourceId = iconId;
        return result;
    }

    /**
     * Constructs a checkbox to show in the editor. It's checked by default.
     *
     * @param checkboxLabel      The label for the checkbox.
     * @param checkboxPreference The shared preference key for the checkbox status. It must be in
     *                           ChromePreferenceKeys.
     */
    public static EditorFieldModel createCheckbox(
            CharSequence checkboxLabel, CharSequence checkboxPreference) {
        assert checkboxLabel != null;
        assert checkboxPreference != null;
        EditorFieldModel result = new EditorFieldModel(INPUT_TYPE_HINT_CHECKBOX);
        result.mLabel = checkboxLabel;
        result.mValue = checkboxPreference;
        return result;
    }

    /**
     * Constructs a list of icons to show in the editor. This can be, for example, the list of
     * accepted credit cards.
     *
     * @param label   The label for the icons.
     * @param iconIds The list of drawable resources to display, in this order.
     * @param descIds The list of string identifiers for descriptions of the icons. This is for
     *                accessibility.
     */
    public static EditorFieldModel createIconList(
            CharSequence label, List<Integer> iconIds, List<Integer> descIds) {
        assert label != null;
        assert iconIds != null;
        assert descIds != null;
        EditorFieldModel result = new EditorFieldModel(INPUT_TYPE_HINT_ICONS);
        result.mLabel = label;
        result.mIconResourceIds = iconIds;
        result.mIconDescriptionsForAccessibility = descIds;
        return result;
    }

    /**
     * Constructs a dropdown field model.
     *
     * @param label             The human-readable label for user to understand the type of data
     *                          that should be entered into this field.
     * @param dropdownKeyValues The keyed values to display in the dropdown.
     * @param hint              The optional hint to be displayed when no value is selected.
     */
    public static EditorFieldModel createDropdown(@Nullable CharSequence label,
            List<DropdownKeyValue> dropdownKeyValues, @Nullable CharSequence hint) {
        assert dropdownKeyValues != null;
        EditorFieldModel result = new EditorFieldModel(INPUT_TYPE_HINT_DROPDOWN);
        result.mLabel = label;
        result.mHint = hint;
        result.setDropdownKeyValues(dropdownKeyValues);
        return result;
    }

    /**
     * Constructs a dropdown field model with a validator.
     *
     * @param label                The human-readable label for user to understand the type of data
     *                             that should be entered into this field.
     * @param dropdownKeyValues    The keyed values to display in the dropdown.
     * @param validator            The validator for the values in this field.
     * @param requiredErrorMessage The error message that indicates to the user that they
     *                             cannot leave this field empty.
     */
    public static EditorFieldModel createDropdown(@Nullable CharSequence label,
            List<DropdownKeyValue> dropdownKeyValues, EditorFieldValidator validator,
            CharSequence invalidErrorMessage) {
        assert dropdownKeyValues != null;
        assert validator != null;
        assert invalidErrorMessage != null;
        EditorFieldModel result = createDropdown(label, dropdownKeyValues, null /* hint */);
        result.mValidator = validator;
        result.mInvalidErrorMessage = invalidErrorMessage;
        return result;
    }

    /** Constructs a text input field model without any special text formatting hints. */
    public static EditorFieldModel createTextInput() {
        return new EditorFieldModel(INPUT_TYPE_HINT_NONE);
    }

    /**
     * Constructs a text input field model.
     *
     * @param inputTypeHint The type of input. For example, INPUT_TYPE_HINT_PHONE.
     */
    public static EditorFieldModel createTextInput(int inputTypeHint) {
        EditorFieldModel result = new EditorFieldModel(inputTypeHint);
        assert result.isTextField();
        return result;
    }

    /**
     * Constructs a text input field model.
     *
     * @param inputTypeHint        The type of input. For example, {@link INPUT_TYPE_HINT_PHONE}.
     * @param label                The human-readable label for user to understand the type of data
     *                             that should be entered into this field.
     * @param suggestions          Optional set of values to suggest to the user.
     * @param formatter            Optional formatter for the values in this field.
     * @param validator            Optional validator for the values in this field.
     * @param valueIconGenerator   Optional icon generator for the values in this field.
     * @param requiredErrorMessage The optional error message that indicates to the user that they
     *                             cannot leave this field empty.
     * @param invalidErrorMessage  The optional error message that indicates to the user that the
     *                             value they have entered is not valid.
     * @param lengthCounterLimit   The maximum number of characters to be allowed by the length
     *                             counter ("cur/max") shown below the focused field. Use
     *                             {@link LENGTH_COUNTER_LIMIT_NONE} to disable the counter.
     * @param value                Optional initial value of this field.
     */
    public static EditorFieldModel createTextInput(int inputTypeHint, CharSequence label,
            @Nullable Set<CharSequence> suggestions, @Nullable TextWatcher formatter,
            @Nullable EditorFieldValidator validator,
            @Nullable EditorValueIconGenerator valueIconGenerator,
            @Nullable CharSequence requiredErrorMessage, @Nullable CharSequence invalidErrorMessage,
            int lengthCounterLimit, @Nullable CharSequence value) {
        assert label != null;
        EditorFieldModel result = new EditorFieldModel(inputTypeHint);
        assert result.isTextField();
        result.mSuggestions = suggestions == null ? null : new ArrayList<CharSequence>(suggestions);
        result.mFormatter = formatter;
        result.mValidator = validator;
        result.mValueIconGenerator = valueIconGenerator;
        result.mInvalidErrorMessage = invalidErrorMessage;
        result.mRequiredErrorMessage = requiredErrorMessage;
        result.mLabel = label;
        result.mValue = value;
        result.mLengthCounterLimit = Math.max(lengthCounterLimit, LENGTH_COUNTER_LIMIT_NONE);
        return result;
    }

    /**
     * Adds an icon to a text input field. The icon can be tapped to perform an action, e.g., launch
     * a credit card scanner.
     *
     * @param icon        The drawable resource for the icon.
     * @param description The string resource for the human readable description of the action.
     * @param action      The callback to invoke when the icon is tapped.
     */
    public void addActionIcon(int icon, int description, Runnable action) {
        assert isTextField();
        mActionIconResourceId = icon;
        mActionIconDescriptionForAccessibility = description;
        mActionIconAction = action;
    }

    private EditorFieldModel(int inputTypeHint) {
        assert isTextField();
        mInputTypeHint = inputTypeHint;
    }

    /** @return The action icon resource identifier, for example, R.drawable.ocr_card. */
    public int getActionIconResourceId() {
        assert isTextField();
        return mActionIconResourceId;
    }

    /** @return The string resource for the human readable description of the action icon. */
    public int getActionIconDescriptionForAccessibility() {
        assert isTextField();
        return mActionIconDescriptionForAccessibility;
    }

    /** @return The action to invoke when the action icon has been tapped. */
    public Runnable getActionIconAction() {
        assert isTextField();
        return mActionIconAction;
    }

    /** @return The value formatter or null if not exist. */
    @Nullable
    public TextWatcher getFormatter() {
        assert isTextField();
        return mFormatter;
    }

    /** @return The value icon generator or null if not exist. */
    public EditorValueIconGenerator getValueIconGenerator() {
        assert isTextField();
        return mValueIconGenerator;
    }

    /** @return Whether the input is a text field. */
    public boolean isTextField() {
        return mInputTypeHint >= INPUT_TYPE_HINT_MIN_INCLUSIVE
                && mInputTypeHint < INPUT_TYPE_HINT_MAX_TEXT_INPUT_EXCLUSIVE;
    }

    /** @return Whether the input is a dropdown field. */
    public boolean isDropdownField() {
        return mInputTypeHint == INPUT_TYPE_HINT_DROPDOWN;
    }

    /** @return The type of input, for example, INPUT_TYPE_HINT_PHONE. */
    public int getInputTypeHint() {
        return mInputTypeHint;
    }

    /** @return Whether the checkbox is checked. */
    public boolean isChecked() {
        assert mInputTypeHint == INPUT_TYPE_HINT_CHECKBOX;
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return ContextUtils.getAppSharedPreferences().getBoolean(mValue.toString(), true);
        }
    }

    /** Sets the checkbox state. */
    public void setIsChecked(boolean isChecked) {
        assert mInputTypeHint == INPUT_TYPE_HINT_CHECKBOX;
        SharedPreferences.Editor editor = ContextUtils.getAppSharedPreferences().edit();
        editor.putBoolean(mValue.toString(), isChecked);
        editor.apply();
    }

    /** @return The list of icons resource identifiers to display. */
    public List<Integer> getIconResourceIds() {
        assert mInputTypeHint == INPUT_TYPE_HINT_ICONS;
        return mIconResourceIds;
    }

    /**
     * @return The list of string identifiers of the descriptions of the displayed icons. This is
     * for the screen reader.
     */
    public List<Integer> getIconDescriptionsForAccessibility() {
        assert mInputTypeHint == INPUT_TYPE_HINT_ICONS;
        return mIconDescriptionsForAccessibility;
    }

    /** @return The dropdown key-value pairs. */
    public List<DropdownKeyValue> getDropdownKeyValues() {
        assert mInputTypeHint == INPUT_TYPE_HINT_DROPDOWN;
        return mDropdownKeyValues;
    }

    /** @return The dropdown keys. */
    public Set<String> getDropdownKeys() {
        assert mInputTypeHint == INPUT_TYPE_HINT_DROPDOWN;
        return mDropdownKeys;
    }

    /**
     * In the dropdown list, finds the key that corresponds to the value.
     * If the value is not in the list, returns null.
     * This assumes a one-to-one relation between keys and values.
     *
     * @param value The value to lookup.
     * @return The key that corresponds to the value.
     */
    @Nullable
    public String getDropdownKeyByValue(@Nullable CharSequence value) {
        assert mInputTypeHint == INPUT_TYPE_HINT_DROPDOWN;
        if (value == null) {
            return null;
        }
        return mDropdownValueToKeyMap.get(value.toString());
    }

    /**
     * In the dropdown list, finds the value that corresponds to the key.
     * If the key is not in the list, returns null.
     * This assumes a one-to-one relation between keys and values.
     *
     * @param key The key to lookup.
     * @return The value that corresponds to the key.
     */
    @Nullable
    public CharSequence getDropdownValueByKey(@Nullable String key) {
        assert mInputTypeHint == INPUT_TYPE_HINT_DROPDOWN;
        return mDropdownKeyToValueMap.get(key);
    }

    /** Updates the dropdown key values. */
    public void setDropdownKeyValues(List<DropdownKeyValue> dropdownKeyValues) {
        assert mInputTypeHint == INPUT_TYPE_HINT_DROPDOWN;
        mDropdownKeyValues = dropdownKeyValues;
        mDropdownKeys = new HashSet<>();
        mDropdownKeyToValueMap = new HashMap<>();
        mDropdownValueToKeyMap = new HashMap<>();
        for (int i = 0; i < mDropdownKeyValues.size(); i++) {
            mDropdownKeys.add(mDropdownKeyValues.get(i).getKey());
            mDropdownValueToKeyMap.put(mDropdownKeyValues.get(i).getValue().toString(),
                    mDropdownKeyValues.get(i).getKey());
            mDropdownKeyToValueMap.put(
                    mDropdownKeyValues.get(i).getKey(), mDropdownKeyValues.get(i).getValue());
        }
        assert mDropdownKeyValues.size() == mDropdownKeys.size();
    }

    /**
     * @return True if the last visible item on the dropdown list, needs a '+' icon on the right.
     */
    public boolean isDisplayPlusIcon() {
        assert mInputTypeHint == INPUT_TYPE_HINT_DROPDOWN;
        return mPlusIconIsDisplayed;
    }

    /**
     * Sets whether the last item of this dropdown list needs a '+' icon on the right. By
     * default, the item doesn't need that.
     *
     * @param plusIconIsDisplayed Whether the last item of the this dropdown list needs a '+' icon.
     */
    public void setDisplayPlusIcon(boolean plusIconIsDisplayed) {
        assert mInputTypeHint == INPUT_TYPE_HINT_DROPDOWN;
        mPlusIconIsDisplayed = plusIconIsDisplayed;
    }

    /** @return The human-readable label for this field. */
    public CharSequence getLabel() {
        return mLabel;
    }

    /** @return The human-readable hint for this dropdown field. */
    public CharSequence getHint() {
        assert mInputTypeHint == INPUT_TYPE_HINT_DROPDOWN;
        return mHint;
    }

    /** @return The human-readable mid-level label for this field. */
    public CharSequence getMidLabel() {
        assert mInputTypeHint == INPUT_TYPE_HINT_LABEL;
        return mMidLabel;
    }

    /** @return The human-readable lower-level label for this field. */
    public CharSequence getBottomLabel() {
        assert mInputTypeHint == INPUT_TYPE_HINT_LABEL;
        return mBottomLabel;
    }

    /** @return The icon to show next to the label. */
    public int getLabelIconResourceId() {
        assert mInputTypeHint == INPUT_TYPE_HINT_LABEL;
        return mLabelIconResourceId;
    }

    /**
     * Updates the label.
     *
     * @param label The new label to use.
     */
    public void setLabel(CharSequence label) {
        mLabel = label;
    }

    /** @return Suggested values for this field. Can be null. */
    @Nullable
    public List<CharSequence> getSuggestions() {
        return mSuggestions;
    }

    /** @return The error message for the last validation. Can be null if no error was reported. */
    @Nullable
    public CharSequence getErrorMessage() {
        return mErrorMessage;
    }

    /** Updates the custom error message */
    public void setCustomErrorMessage(@Nullable CharSequence errorMessage) {
        mCustomErrorMessage = errorMessage;
    }

    /**
     * @return The value that the user has typed into the field or the key of the value that the
     *          user has selected in the dropdown. Can be null.
     */
    @Nullable
    public CharSequence getValue() {
        return mValue;
    }

    /**
     * Updates the value of this field. Does not trigger validation or update the last error
     * message. Can be called on a dropdown to initialize it, but will not fire the dropdown
     * callback.
     *
     * @param value The new value that the user has typed in or the initial key for the dropdown.
     */
    public void setValue(@Nullable CharSequence userTypedValueOrInitialDropdownKey) {
        mValue = userTypedValueOrInitialDropdownKey;
    }

    /**
     * Updates the dropdown selection and fires the dropdown callback.
     *
     * @param key      The new dropdown key.
     * @param callback The callback to invoke when the change has been processed.
     */
    public void setDropdownKey(@Nullable String key, Runnable callback) {
        assert mInputTypeHint == INPUT_TYPE_HINT_DROPDOWN;
        // The mValue can only be set to null if there is a hint.
        if (key == null && mHint == null) {
            return;
        }
        mValue = key;
        if (mDropdownCallback != null) {
            mDropdownCallback.onResult(new Pair<String, Runnable>(key, callback));
        }
    }

    /** @return Whether or not the field is required. */
    public boolean isRequired() {
        return !TextUtils.isEmpty(mRequiredErrorMessage);
    }

    /**
     * Updates the required error message.
     *
     * @param message The error message to use if this field is required, but empty. If null, then
     *                this field is optional.
     */
    public void setRequiredErrorMessage(@Nullable CharSequence message) {
        mRequiredErrorMessage = message;
    }

    /**
     * Returns true if the field value is valid. Also updates the error message.
     *
     * @return Whether the field value is valid.
     */
    public boolean isValid() {
        if (!TextUtils.isEmpty(mCustomErrorMessage)) {
            mErrorMessage = mCustomErrorMessage;
            return false;
        }

        if (isRequired()
                && (TextUtils.isEmpty(mValue) || TextUtils.getTrimmedLength(mValue) == 0)) {
            mErrorMessage = mRequiredErrorMessage;
            return false;
        }

        if (mValidator != null && !mValidator.isValid(mValue)) {
            mErrorMessage = mInvalidErrorMessage;
            return false;
        }

        mErrorMessage = null;
        return true;
    }

    /**
     * Returns true if the field value length is maximum among all the possible valid values in this
     * field.
     *
     * @Return Whether the field value length is maximum.
     */
    public boolean isLengthMaximum() {
        return mValidator == null ? false : mValidator.isLengthMaximum(mValue);
    }

    /**
     * Sets the dropdown callback.
     *
     * @param callback The callback to invoke when the dropdown selection has changed. The first
     *                 element in the callback pair is the dropdown key. The second element is the
     *                 callback to invoke after the dropdown change has been processed.
     */
    public void setDropdownCallback(Callback<Pair<String, Runnable>> callback) {
        assert mInputTypeHint == INPUT_TYPE_HINT_DROPDOWN;
        mDropdownCallback = callback;
    }

    /**
     * @return True if the input field should take up the full line, instead of sharing with other
     *          input fields. This is the default.
     */
    public boolean isFullLine() {
        return mIsFullLine;
    }

    /**
     * Sets whether this input field should take up the full line. All fields take up the full line
     * by default.
     *
     * @param isFullLine Whether the input field should take up the full line.
     */
    public void setIsFullLine(boolean isFullLine) {
        mIsFullLine = isFullLine;
    }

    /**
     * @return The maximum number of characters allowed by the length counter ("cur/max") shown
     * below the focused field. If the value equals {@link LENGTH_COUNTER_LIMIT_NONE}, the counter
     * is not shown, which is the default behavior.
     */
    public int getLengthCounterLimit() {
        return mLengthCounterLimit;
    }

    /**
     * Sets the maximum number of characters to be allowed by the length counter ("cur/max") shown
     * below the focused field. If the value equals {@link LENGTH_COUNTER_LIMIT_NONE} or any
     * nonpositive value, the counter won't be shown, which is the default behavior.
     *
     * @param lengthCounterLimit The maximum number of characters for the length counter.
     */
    public void setLengthCounterLimit(int lengthCounterLimit) {
        mLengthCounterLimit = Math.max(lengthCounterLimit, LENGTH_COUNTER_LIMIT_NONE);
    }

    /**
     * @return whether the length counter should be shown below the focused field.
     */
    public boolean hasLengthCounter() {
        return mLengthCounterLimit != EditorFieldModel.LENGTH_COUNTER_LIMIT_NONE;
    }
}
