// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import org.chromium.base.FeatureList;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Handles displaying the voice search button on toolbar depending on several conditions (e.g.
 * device width, whether NTP is shown, whether voice is enabled).
 *
 * TODO(crbug.com/1144976): Move this to ../voice/ along with VoiceRecognitionHandler and the
 * assistant support.
 */
public class VoiceToolbarButtonController
        implements ButtonDataProvider, ConfigurationChangedObserver {
    /**
     * Default minimum width to show the voice search button.
     */
    public static final int DEFAULT_MIN_WIDTH_DP = 360;

    private final Supplier<Tab> mActiveTabSupplier;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    private final ModalDialogManager mModalDialogManager;
    private final ModalDialogManagerObserver mModalDialogManagerObserver;

    private final VoiceSearchDelegate mVoiceSearchDelegate;

    private final ButtonData mButtonData;
    private final ObserverList<ButtonDataObserver> mObservers = new ObserverList<>();

    private Integer mMinimumWidthDp;
    private int mScreenWidthDp;

    /**
     * Delegate interface for interacting with voice search.
     */
    public interface VoiceSearchDelegate {
        /**
         * @return True if voice search is enabled for the current session.
         */
        boolean isVoiceSearchEnabled();

        /**
         * Starts a voice search interaction.
         */
        void startVoiceRecognition();
    }

    /**
     * Creates a VoiceToolbarButtonController object.
     * @param context The Context for retrieving resources, etc.
     * @param activeTabSupplier Provides the currently displayed {@link Tab}.
     * @param activityLifecycleDispatcher Dispatcher for activity lifecycle events, e.g.
     *                                    configuration changes.
     * @param modalDialogManager Dispatcher for modal lifecycle events
     * @param voiceSearchDelegate Provides interaction with voice search.
     */
    public VoiceToolbarButtonController(Context context, Drawable buttonDrawable,
            Supplier<Tab> activeTabSupplier,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            ModalDialogManager modalDialogManager, VoiceSearchDelegate voiceSearchDelegate) {
        mActiveTabSupplier = activeTabSupplier;

        // Register for onConfigurationChanged events, which notify on changes to screen width.
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);

        mModalDialogManagerObserver = new ModalDialogManagerObserver() {
            @Override
            public void onDialogAdded(PropertyModel model) {
                mButtonData.isEnabled = false;
                notifyObservers(mButtonData.canShow);
            }

            @Override
            public void onLastDialogDismissed() {
                mButtonData.isEnabled = true;
                notifyObservers(mButtonData.canShow);
            }
        };
        mModalDialogManager = modalDialogManager;
        mModalDialogManager.addObserver(mModalDialogManagerObserver);

        mVoiceSearchDelegate = voiceSearchDelegate;

        OnClickListener onClickListener = (view) -> {
            RecordUserAction.record("MobileTopToolbarVoiceButton");
            mVoiceSearchDelegate.startVoiceRecognition();
        };

        mButtonData = new ButtonData(/*canShow=*/false, buttonDrawable, onClickListener,
                R.string.accessibility_toolbar_btn_mic,
                /*supportsTinting=*/true, /*iphCommandBuilder=*/null, /*isEnabled=*/true);

        mScreenWidthDp = context.getResources().getConfiguration().screenWidthDp;
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {
        if (mScreenWidthDp == configuration.screenWidthDp) {
            return;
        }
        mScreenWidthDp = configuration.screenWidthDp;
        mButtonData.canShow = shouldShowVoiceButton(mActiveTabSupplier.get());
        notifyObservers(mButtonData.canShow);
    }

    @Override
    public void destroy() {
        mActivityLifecycleDispatcher.unregister(this);
        mModalDialogManager.removeObserver(mModalDialogManagerObserver);
        mObservers.clear();
    }

    @Override
    public void addObserver(ButtonDataObserver obs) {
        mObservers.addObserver(obs);
    }

    @Override
    public void removeObserver(ButtonDataObserver obs) {
        mObservers.removeObserver(obs);
    }

    @Override
    public ButtonData get(Tab tab) {
        mButtonData.canShow = shouldShowVoiceButton(tab);
        return mButtonData;
    }

    private boolean shouldShowVoiceButton(Tab tab) {
        if (!FeatureList.isInitialized()
                || !ChromeFeatureList.isEnabled(ChromeFeatureList.VOICE_BUTTON_IN_TOP_TOOLBAR)
                || tab == null || tab.isIncognito()
                || !mVoiceSearchDelegate.isVoiceSearchEnabled()) {
            return false;
        }

        if (mMinimumWidthDp == null) {
            mMinimumWidthDp = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.VOICE_BUTTON_IN_TOP_TOOLBAR, "minimum_width_dp",
                    DEFAULT_MIN_WIDTH_DP);
        }

        boolean isDeviceWideEnough = mScreenWidthDp >= mMinimumWidthDp;
        if (!isDeviceWideEnough) return false;

        return UrlUtilities.isHttpOrHttps(tab.getUrl());
    }

    private void notifyObservers(boolean hint) {
        for (ButtonDataObserver observer : mObservers) {
            observer.buttonDataChanged(hint);
        }
    }
}
