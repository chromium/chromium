// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.google_bottom_bar.proto.IntentParams.GoogleBottomBarIntentParams;

import java.util.List;
import java.util.Set;

/**
 * Coordinator for GoogleBottomBar module. Provides the view, and initializes various components.
 */
public class GoogleBottomBarCoordinator {

    private static final String TAG = "GBBCoordinator";

    /** Returns true if GoogleBottomBar is enabled in the feature flag. */
    public static boolean isFeatureEnabled() {
        return ChromeFeatureList.sCctGoogleBottomBar.isEnabled();
    }

    /** Returns true if the id of the custom button param is supported. */
    public static boolean isSupported(int customButtonParamsId) {
        return BottomBarConfigCreator.shouldAddToGoogleBottomBar(customButtonParamsId);
    }

    private final Context mContext;
    private final GoogleBottomBarViewCreator mGoogleBottomBarViewCreator;

    private boolean mHasNativeInitializationFinished;

    /**
     * Constructor.
     *
     * @param activity The associated {@link Activity}.
     * @param tabProvider Supplier for the current activity tab.
     * @param shareDelegateSupplier Supplier for the the share delegate.
     * @param googleBottomBarIntentParams The encoded button list provided through IntentParams
     * @param customButtonsOnGoogleBottomBar List of {@link CustomButtonParams} provided by the
     *     embedder to be displayed in the Bottom Bar.
     */
    public GoogleBottomBarCoordinator(
            Activity activity,
            Supplier<Tab> tabProvider,
            Supplier<ShareDelegate> shareDelegateSupplier,
            GoogleBottomBarIntentParams googleBottomBarIntentParams,
            List<CustomButtonParams> customButtonsOnGoogleBottomBar) {
        mContext = activity;
        mGoogleBottomBarViewCreator =
                new GoogleBottomBarViewCreator(
                        activity,
                        tabProvider,
                        shareDelegateSupplier,
                        getBottomBarConfig(
                                googleBottomBarIntentParams, customButtonsOnGoogleBottomBar));
    }

    /**
     * Determines which buttons to display in the Google Bottom Bar based on
     * GoogleBottomBarIntentParams.
     *
     * @param intentParams that optionally contains:
     *     <p>Integer list with the following representation [5,1,2,3,4,5], where the first item
     *     represents the spotlight button and the rest of the list the order of the buttons in the
     *     bottom bar.
     *     <p>Variant layout type that specifies variation of the layout that should be used
     * @return A set of integers representing the customButtonParamIds of the buttons that should be
     *     displayed in the Google Bottom Bar.
     */
    public static Set<Integer> getSupportedCustomButtonParamIds(
            GoogleBottomBarIntentParams intentParams) {
        return BottomBarConfigCreator.getSupportedCustomButtonParamIds(intentParams);
    }

    /**
     * Updates the state of a bottom bar button based on the provided parameters.
     *
     * @param params The parameters containing information relevant to the button update.
     * @return {@code true} if the button was successfully updated, {@code false} otherwise.
     */
    public boolean updateBottomBarButton(CustomButtonParams params) {
        return mGoogleBottomBarViewCreator.updateBottomBarButton(
                BottomBarConfigCreator.createButtonConfigFromCustomParams(mContext, params));
    }

    /** Returns a view that contains the Google Bottom bar. */
    public View createGoogleBottomBarView() {
        return mGoogleBottomBarViewCreator.createGoogleBottomBarView();
    }

    /** Returns the height of the Google Bottom bar in pixels. */
    public int getBottomBarHeightInPx() {
        return mGoogleBottomBarViewCreator.getBottomBarHeightInPx();
    }

    /**
     * Indicates the completion of native initialization processes.
     *
     * <p>This method is called when the native components necessary for the operation of Google
     * Bottom Bar View have finished their initialization.
     */
    public void onFinishNativeInitialization() {
        // TODO(b/345129005): Clean this stuff up.
        if (!mHasNativeInitializationFinished) {
            mHasNativeInitializationFinished = true;
            mGoogleBottomBarViewCreator.logButtons();
        }
    }

    /**
     * Stores default search engine information.
     *
     * @param originalProfile The profile to check for default search engine information.
     */
    public void initDefaultSearchEngine(Profile originalProfile) {
        BottomBarConfigCreator.initDefaultSearchEngine(originalProfile);
    }

    @VisibleForTesting
    GoogleBottomBarViewCreator getGoogleBottomBarViewCreatorForTesting() {
        return mGoogleBottomBarViewCreator;
    }

    private BottomBarConfig getBottomBarConfig(
            GoogleBottomBarIntentParams intentParams,
            List<CustomButtonParams> customButtonsOnGoogleBottomBar) {
        BottomBarConfigCreator configCreator = new BottomBarConfigCreator(mContext);
        return configCreator.create(intentParams, customButtonsOnGoogleBottomBar);
    }
}
