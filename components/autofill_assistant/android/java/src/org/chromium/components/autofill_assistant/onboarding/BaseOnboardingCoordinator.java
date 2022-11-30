// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.onboarding;

import android.content.Context;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.LocaleUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.autofill_assistant.AssistantInfoPageUtil;
import org.chromium.components.autofill_assistant.AutofillAssistantPreferenceManager;
import org.chromium.components.autofill_assistant.R;
import org.chromium.components.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

/**
 * Base Coordinator class responsible for showing the onboarding screen when the user is using the
 * Autofill Assistant for the first time.
 */
@JNINamespace("autofill_assistant")
public abstract class BaseOnboardingCoordinator implements OnboardingView {
    public static final String INTENT_IDENTFIER = "INTENT";
    private static final String FETCH_TIMEOUT_IDENTIFIER = "ONBOARDING_FETCH_TIMEOUT_MS";
    private static final String BUY_MOVIE_TICKETS_INTENT = "BUY_MOVIE_TICKET";
    private static final String RENT_CAR_INTENT = "RENT_CAR";
    private static final String FLIGHTS_INTENT = "FLIGHTS_CHECKIN";
    private static final String PASSWORD_CHANGE_INTENT = "PASSWORD_CHANGE";
    private static final String FOOD_ORDERING_INTENT = "FOOD_ORDERING";
    private static final String FOOD_ORDERING_PICKUP_INTENT = "FOOD_ORDERING_PICKUP";
    private static final String FOOD_ORDERING_DELIVERY_INTENT = "FOOD_ORDERING_DELIVERY";
    private static final String VOICE_SEARCH_INTENT = "TELEPORT";
    private static final String SHOPPING_INTENT = "SHOPPING";
    private static final String SHOPPING_ASSISTED_CHECKOUT_INTENT = "SHOPPING_ASSISTED_CHECKOUT";
    private static final String BUY_MOVIE_TICKETS_EXPERIMENT_ID = "4363482";
    private static final String ONBOARDING_TITLE_KEY = "onboarding_title";
    private static final String ONBOARDING_SUBTITLE_KEY = "onboarding_text";
    private static final String TERMS_AND_CONDITIONS_KEY = "terms_and_conditions";
    private static final String TERMS_AND_CONDITIONS_URL_KEY = "terms_and_conditions_url";

    private final BrowserContextHandle mBrowserContext;
    private final AssistantInfoPageUtil mInfoPageUtil;
    private final String mExperimentIds;
    private final Map<String, String> mParameters;
    protected final Map<String, String> mStringMap = new HashMap<>();

    private boolean mOnboardingShown;

    final Context mContext;
    boolean mAnimate = true;
    @Nullable
    ScrollView mView;

    public BaseOnboardingCoordinator(BrowserContextHandle browserContext,
            AssistantInfoPageUtil infoPageUtil, String experimentIds,
            Map<String, String> parameters, Context context) {
        mBrowserContext = browserContext;
        mInfoPageUtil = infoPageUtil;
        mExperimentIds = experimentIds;
        mParameters = parameters;
        mContext = context;

        mView = createViewImpl();
    }

    /**
     * Shows onboarding and provides the result to the given callback.
     *
     * <p>The {@code callback} will be called when the user accepts, cancels or dismisses the
     * onboarding.
     *
     * <p>Note that the onboarding screen will be hidden after the callback returns. Call, from the
     * callback, {@link #hide} to hide it earlier or {@link #transferControls} to take ownership of
     * it and possibly keep it past the end of the callback.
     */
    @Override
    public void show(Callback<Integer> callback) {
        mOnboardingShown = true;

        initViewImpl(callback);
        setupSharedView(callback);

        int fetchTimeoutMs = 1000;
        if (mParameters.containsKey(FETCH_TIMEOUT_IDENTIFIER)) {
            fetchTimeoutMs = Integer.parseInt(mParameters.get(FETCH_TIMEOUT_IDENTIFIER));
        }
        if (!mParameters.containsKey(INTENT_IDENTFIER) || fetchTimeoutMs == 0) {
            updateAndShowView();
        } else {
            BaseOnboardingCoordinatorJni.get().fetchOnboardingDefinition(this,

                    mParameters.get(INTENT_IDENTFIER), LocaleUtils.getDefaultLocaleString(),
                    mBrowserContext.getNativeBrowserContextPointer(), fetchTimeoutMs);
        }
    }

    /**
     * Returns {@code true} if the onboarding has been shown at the beginning when this
     * autofill assistant flow got triggered.
     */
    @Override
    public boolean getOnboardingShown() {
        return mOnboardingShown;
    }

    /**
     * Transfers the overlay coordinator used by the onboarding to the caller. This is intended to
     * be used to facilitate a smooth transition between onboarding and regular script, i.e., to
     * avoid flickering of the overlay. Not all onboarding implementations show an overlay, so this
     * may return null.
     */
    @Nullable
    public AssistantOverlayCoordinator transferControls() {
        return null;
    }

    /**
     * Setup the shared |mView|
     */
    protected void setupSharedView(Callback<Integer> callback) {
        // Set focusable for accessibility.
        mView.setFocusable(true);
        mView.findViewById(R.id.button_init_ok)
                .setOnClickListener(unusedView
                        -> onUserAction(
                                /* result= */ AssistantOnboardingResult.ACCEPTED, callback));
        mView.findViewById(R.id.button_init_not_ok)
                .setOnClickListener(unusedView
                        -> onUserAction(
                                /* result= */ AssistantOnboardingResult.REJECTED, callback));
    }

    void onUserAction(@AssistantOnboardingResult Integer result, Callback<Integer> callback) {
        AutofillAssistantPreferenceManager prefManager =
                new AutofillAssistantPreferenceManager(UserPrefs.get(mBrowserContext));
        switch (result) {
            case AssistantOnboardingResult.DISMISSED:
                break;
            case AssistantOnboardingResult.REJECTED:
                prefManager.setOnboardingAccepted(false);
                break;
            case AssistantOnboardingResult.ACCEPTED:
                prefManager.setOnboardingAccepted(true);
                break;
        }
        callback.onResult(result);
        hide();
    }

    Context getContext() {
        return mContext;
    }

    @CalledByNative
    @VisibleForTesting
    public void addEntryToStringMap(String key, String value) {
        mStringMap.put(key, value);
    }

    @CalledByNative
    @VisibleForTesting
    public void updateAndShowView() {
        updateViews();
        showViewImpl();
    }

    /**
     * Updates the given ToC view text based on the current parameters.
     */
    protected void updateTermsAndConditionsView(TextView termsAndConditionsView) {
        // Note: these strings may be null.
        String termsAndConditionsString = mStringMap.get(TERMS_AND_CONDITIONS_KEY);
        String termsAndConditionsUrl = mStringMap.get(TERMS_AND_CONDITIONS_URL_KEY);

        // Note: `SpanApplier.applySpans` will throw an error if the text does not contain
        // <link></link> to replace!
        if (TextUtils.isEmpty(termsAndConditionsString)
                || !termsAndConditionsString.contains("<link>")
                || !termsAndConditionsString.contains("</link>")) {
            termsAndConditionsString = mContext.getApplicationContext().getString(
                    R.string.autofill_assistant_google_terms_description);
        }

        NoUnderlineClickableSpan termsSpan = new NoUnderlineClickableSpan(mContext,
                (widget)
                        -> mInfoPageUtil.showInfoPage(mContext.getApplicationContext(),
                                TextUtils.isEmpty(termsAndConditionsUrl)
                                                || !UrlUtilitiesJni.get().isGoogleSubDomainUrl(
                                                        termsAndConditionsUrl)
                                        ? mContext.getApplicationContext().getString(
                                                R.string.autofill_assistant_google_terms_url)
                                        : termsAndConditionsUrl));
        SpannableString spannableMessage = SpanApplier.applySpans(
                termsAndConditionsString, new SpanApplier.SpanInfo("<link>", "</link>", termsSpan));
        termsAndConditionsView.setText(spannableMessage);
        termsAndConditionsView.setMovementMethod(LinkMovementMethod.getInstance());
    }

    /**
     * Updates the given title view text based on the current parameters.
     */
    protected void updateTitleView(TextView titleView) {
        if (mStringMap.containsKey(ONBOARDING_TITLE_KEY)) {
            titleView.setText(mStringMap.get(ONBOARDING_TITLE_KEY));
            return;
        }

        if (!mParameters.containsKey(INTENT_IDENTFIER)) {
            return;
        }

        switch (mParameters.get(INTENT_IDENTFIER)) {
            case FLIGHTS_INTENT:
                titleView.setText(R.string.autofill_assistant_init_message_flights_checkin);
                return;
            case FOOD_ORDERING_INTENT:
            case FOOD_ORDERING_PICKUP_INTENT:
            case FOOD_ORDERING_DELIVERY_INTENT:
                titleView.setText(R.string.autofill_assistant_init_message_food_ordering);
                return;
            case VOICE_SEARCH_INTENT:
                titleView.setText(R.string.autofill_assistant_init_message_voice_search);
                return;
            case RENT_CAR_INTENT:
                titleView.setText(R.string.autofill_assistant_init_message_rent_car);
                return;
            case PASSWORD_CHANGE_INTENT:
                titleView.setText(R.string.autofill_assistant_init_message_password_change);
                return;
            case SHOPPING_INTENT:
            case SHOPPING_ASSISTED_CHECKOUT_INTENT:
                titleView.setText(R.string.autofill_assistant_init_message_shopping);
                return;
            case BUY_MOVIE_TICKETS_INTENT:
                if (Arrays.asList(mExperimentIds.split(","))
                                .contains(BUY_MOVIE_TICKETS_EXPERIMENT_ID)) {
                    titleView.setText(R.string.autofill_assistant_init_message_buy_movie_tickets);
                }
                return;
        }
    }

    /**
     * Updates the given subtitle view text based on the current parameters.
     */
    protected void updateSubtitleView(TextView subtitleView) {
        if (mStringMap.containsKey(ONBOARDING_SUBTITLE_KEY)) {
            subtitleView.setText(mStringMap.get(ONBOARDING_SUBTITLE_KEY));
            return;
        }

        if (!mParameters.containsKey(INTENT_IDENTFIER)) {
            return;
        }

        switch (mParameters.get(INTENT_IDENTFIER)) {
            case FLIGHTS_INTENT:
            case FOOD_ORDERING_INTENT:
            case FOOD_ORDERING_PICKUP_INTENT:
            case FOOD_ORDERING_DELIVERY_INTENT:
            case VOICE_SEARCH_INTENT:
            case RENT_CAR_INTENT:
            case PASSWORD_CHANGE_INTENT:
            case SHOPPING_INTENT:
            case SHOPPING_ASSISTED_CHECKOUT_INTENT:
                subtitleView.setText(R.string.autofill_assistant_init_message_short);
                return;
            case BUY_MOVIE_TICKETS_INTENT:
                if (Arrays.asList(mExperimentIds.split(","))
                                .contains(BUY_MOVIE_TICKETS_EXPERIMENT_ID)) {
                    subtitleView.setText(R.string.autofill_assistant_init_message_short);
                }
                return;
        }
    }

    /** Don't animate the user interface. */
    @VisibleForTesting
    public void disableAnimationForTesting() {
        mAnimate = false;
    }

    abstract ScrollView createViewImpl();
    abstract void initViewImpl(Callback<Integer> callback);
    abstract void showViewImpl();

    /**
     * Returns {@code true} between the time {@link #show} is called and the time
     * the callback has returned.
     */
    @VisibleForTesting
    public abstract boolean isInProgress();

    @NativeMethods
    interface Natives {
        void fetchOnboardingDefinition(BaseOnboardingCoordinator coordinator, String intent,
                String locale, long nativeBrowserContext, int timeoutMs);
    }
}
