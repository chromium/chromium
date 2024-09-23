// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.res.Resources;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.chrome.browser.consent_auditor.ConsentAuditorBridge;
import org.chromium.chrome.browser.consent_auditor.ConsentAuditorFeature;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.signin.base.CoreAccountId;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

/**
 * Tracks resource IDs for consent texts within TextViews inside the consent screen. The consent
 * screen is an arbitrary set of view hierarchies that are passed to {#link recordConsent()}.
 * Note that all TextView instances within these hierarchies MUST have their text assigned using
 * {@link #setText} and {@link #setTextNonRecordable}. This is verified in {#link recordConsent()}.
 */
public class ConsentTextTracker {
    /**
     * Stores metadata about the text associated with a given TextView in order to extract and
     * validate the consent text.
     */
    private static class TextViewMetadata {
        private final String mString;
        private final @StringRes int mId;

        /**
         * @param text The text which was programmatically assigned to the associated TextView.
         * @param id The ID of the string resource assigned to the associated TextView that
         *         will be used for consent recording, or 0 if this string should not be recorded
         *         as a part of the consent.
         */
        public TextViewMetadata(String text, @StringRes int id) {
            mString = text;
            mId = id;
        }

        public String getString() {
            return mString;
        }

        public int getId() {
            return mId;
        }
    }

    /** A CharSequence -> CharSequence transformation. */
    public interface TextTransformation {
        CharSequence transform(CharSequence input);
    }

    private final Resources mResources;
    private final Map<TextView, TextViewMetadata> mTextViewToMetadataMap = new HashMap<>();

    /**
     * Creates an instance of ConsentTextTracker.
     * @param resources Resources object to be used for converting IDs into strings.
     */
    public ConsentTextTracker(Resources resources) {
        mResources = resources;
    }

    /**
     * Applies a |transformation| on the string resource with given |id|, assigns the resulting
     * text to |view|, and caches the string resource |id| which will later be needed for consent
     * recording.
     * @param view The TextView to which the text should be assigned.
     * @param id The id of the string resource with the text.
     * @param transformation The transformation to be applied on the text. Can be null to indicate
     *         no transformation (i.e. identity).
     */
    public void setText(
            TextView view, @StringRes int id, @Nullable TextTransformation transformation) {
        CharSequence text = mResources.getText(id);
        if (transformation != null) text = transformation.transform(text);
        view.setText(text);
        mTextViewToMetadataMap.put(view, new TextViewMetadata(text.toString(), id));
    }

    /**
     * Like {@link #setText(TextView, int, TextTransformation)}, but with
     * no transformation applied on the assigned text.
     * @see #setText(TextView, int, TextTransformation)
     * @param view The TextView to which the text should be assigned.
     * @param id The id of the string resource with the text.
     */
    public void setText(TextView view, @StringRes int id) {
        setText(view, id, null /* no text transformation */);
    }

    /**
     * Assigns a |text| to the given |view| and remembers that this text should be out of scope for
     * consent recording.
     *
     * @see #setText(TextView, int, TextTransformation)
     * @param view The TextView to which the text should be assigned.
     * @param text The text to be assigned.
     */
    public void setTextNonRecordable(TextView view, CharSequence text) {
        // TODO(crbug.com/41376544): The selected account name, which is assigned to its |view|
        // using
        // this method, can be null in rare circumstances.
        CharSequence textSanitized = text != null ? text : "";

        view.setText(textSanitized);
        mTextViewToMetadataMap.put(
                view, new TextViewMetadata(textSanitized.toString(), 0 /* no resource id */));
    }

    /**
     * Retrieves the string resource id from a given TextView while verifying that it corresponds
     * to the text of that TextView. This can be only done if the text was previously set by
     * {#link setText()} or {#link setTextNonRecordable()}.
     * @param view The TextView whose string resource id should be retrieved.
     * @return The string resource id of the |view|'s text. Can be 0 if this |view|'s text shouldn't
     *         be part of the consent record (Note: 0 is not a valid resource id).
     */
    private @StringRes int getConsentStringResource(TextView view) {
        TextViewMetadata metadata = mTextViewToMetadataMap.get(view);

        // Ensure that setText() was used to assign this text.
        assert metadata != null
                : "The text '"
                        + view.getText().toString()
                        + "' was not assigned "
                        + "by setText() or setTextNonRecordable().";

        // Ensure that the text hasn't changed since the assignment.
        assert view.getText().toString().equals(metadata.getString())
                : "The text '"
                        + view.getText().toString()
                        + "' has been modified after it was assigned by setText() "
                        + "or setTextNonRecordable().";
        return metadata.getId();
    }

    /**
     * @param view The root View where to start scanning.
     * @param outViews The output list to which |view| and all its transitive
     *         children, if visible, will be appended.
     */
    private void getAllVisibleViews(View view, ArrayList<View> outViews) {
        if (view.getVisibility() != View.VISIBLE) return;
        outViews.add(view);
        if (!(view instanceof ViewGroup)) return;
        ViewGroup group = (ViewGroup) view;
        for (int i = 0; i < group.getChildCount(); ++i) {
            getAllVisibleViews(group.getChildAt(i), outViews);
        }
    }

    /**
     * Records the consent.
     *
     * @param profile The {@link Profile} associated with this consent record.
     * @param accountId The account for which the consent is valid
     * @param feature {@link ConsentAuditorFeature} that user has consented to
     * @param confirmationView The view that the user clicked when consenting
     * @param consentViews View hierarchies that implement the consent screen
     */
    public void recordConsent(
            Profile profile,
            CoreAccountId accountId,
            @ConsentAuditorFeature int feature,
            TextView confirmationView,
            View... consentViews) {
        int consentConfirmation = getConsentStringResource(confirmationView);

        ArrayList<Integer> consentDescription = new ArrayList<>();
        ArrayList<View> visibleViews = new ArrayList<>();
        for (View view : consentViews) {
            getAllVisibleViews(view, visibleViews);
        }

        for (View view : visibleViews) {
            if (!(view instanceof TextView)) continue; // This element doesn't hold any text.
            @StringRes int id = getConsentStringResource((TextView) view);
            if (id == 0) continue; // This text is not relevant for consent recording.
            consentDescription.add(id);
        }

        ConsentAuditorBridge.getInstance()
                .recordConsent(
                        profile, accountId, feature, consentDescription, consentConfirmation);
    }
}
