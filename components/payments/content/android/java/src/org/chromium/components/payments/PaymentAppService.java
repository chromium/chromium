// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Creates payment apps. */
@NullMarked
public class PaymentAppService implements PaymentAppFactoryInterface {
    /**
     * The identity of the Google Pay internal app.
     * TODO(crbug.com/400531531): Stop special-casing individual payment apps in Chrome.
     */
    public static final String GOOGLE_PAY_INTERNAL_APP_IDENTITY = "Google_Pay_Internal";

    private static final String UNTRACKED_FACTORY_ID_PREFIX = "Untracked factory - ";
    private static @Nullable PaymentAppService sInstance;
    private final Map<String, PaymentAppFactoryInterface> mFactories = new HashMap<>();
    private int mIdMax;

    /** @return The singleton instance of this class. */
    public static PaymentAppService getInstance() {
        if (sInstance == null) {
            sInstance = new PaymentAppService();
        }
        return sInstance;
    }

    private PaymentAppService() {}

    // TODO(crbug.com/40727972): Remove this method after tests and clank switch to use
    // addUniqueFactory.
    /**
     * @param factory The factory to add.
     */
    public void addFactory(PaymentAppFactoryInterface factory) {
        String id = UNTRACKED_FACTORY_ID_PREFIX + mIdMax++;
        mFactories.put(id, factory);
    }

    /** Resets the instance, used by //clank tests. */
    public void resetForTest() {
        sInstance = null;
    }

    // PaymentAppFactoryInterface implementation.
    @Override
    public void create(PaymentAppFactoryDelegate delegate) {
        Collector collector = new Collector(new HashSet<>(mFactories.values()), delegate);
        for (PaymentAppFactoryInterface factory : mFactories.values()) {
            factory.create(/* delegate= */ collector);
        }
    }

    /**
     * @param factoryId The id that is used to identified a factory in this class.
     * @return Whether this contains the factory identified with factoryId.
     */
    public boolean containsFactory(String factoryId) {
        return mFactories.containsKey(factoryId);
    }

    /**
     * Adds a factory with an id if this id is not added already; otherwise, do nothing.
     * @param factory The factory to be added, can be null;
     * @param factoryId The id that the caller uses to identify the given factory.
     */
    public void addUniqueFactory(@Nullable PaymentAppFactoryInterface factory, String factoryId) {
        if (factory == null) return;
        assert !factoryId.startsWith(UNTRACKED_FACTORY_ID_PREFIX);
        if (mFactories.containsKey(factoryId)) return;
        mFactories.put(factoryId, factory);
    }

    /**
     * Collects payment apps from multiple factories and invokes
     * delegate.onDoneCreatingPaymentApps() and delegate.onCanMakePaymentCalculated() only once.
     */
    private final class Collector implements PaymentAppFactoryDelegate {
        private final Set<PaymentAppFactoryInterface> mPendingFactories;
        private final List<PaymentApp> mPossiblyDuplicatePaymentApps = new ArrayList<>();
        private final PaymentAppFactoryDelegate mDelegate;

        /** Whether at least one payment app factory has calculated canMakePayment to be true. */
        private boolean mCanMakePayment;

        private Collector(
                Set<PaymentAppFactoryInterface> pendingTasks, PaymentAppFactoryDelegate delegate) {
            mPendingFactories = pendingTasks;
            mDelegate = delegate;
        }

        @Override
        public PaymentAppFactoryParams getParams() {
            return mDelegate.getParams();
        }

        @Override
        public void onCanMakePaymentCalculated(boolean canMakePayment) {
            // If all payment app factories return false for canMakePayment, then
            // onCanMakePaymentCalculated(false) is called finally in
            // onDoneCreatingPaymentApps(factory).
            if (!canMakePayment || mCanMakePayment) return;
            mCanMakePayment = true;
            mDelegate.onCanMakePaymentCalculated(true);
        }

        @Override
        public void onPaymentAppCreated(PaymentApp paymentApp) {
            mPossiblyDuplicatePaymentApps.add(paymentApp);
        }

        @Override
        public void onPaymentAppCreationError(
                String errorMessage, @AppCreationFailureReason int errorReason) {
            mDelegate.onPaymentAppCreationError(errorMessage, errorReason);
        }

        @Override
        public void setCanMakePaymentEvenWithoutApps() {
            mDelegate.setCanMakePaymentEvenWithoutApps();
        }

        @Override
        public void onDoneCreatingPaymentApps(PaymentAppFactoryInterface factory) {
            mPendingFactories.remove(factory);
            if (!mPendingFactories.isEmpty()) return;

            if (!mCanMakePayment) mDelegate.onCanMakePaymentCalculated(false);

            Set<PaymentApp> uniquePaymentApps =
                    deduplicatePaymentApps(mPossiblyDuplicatePaymentApps);
            mPossiblyDuplicatePaymentApps.clear();

            for (PaymentApp app : uniquePaymentApps) {
                mDelegate.onPaymentAppCreated(app);
            }

            mDelegate.onDoneCreatingPaymentApps(PaymentAppService.this);
        }

        @Override
        public void setOptOutOffered() {
            mDelegate.setOptOutOffered();
        }

        @Override
        public CSPChecker getCSPChecker() {
            return mDelegate.getCSPChecker();
        }

        @Override
        public @Nullable DialogController getDialogController() {
            return mDelegate.getDialogController();
        }

        @Override
        public @Nullable AndroidIntentLauncher getAndroidIntentLauncher() {
            return mDelegate.getAndroidIntentLauncher();
        }

        @Override
        public boolean isFullDelegationRequired() {
            return mDelegate.isFullDelegationRequired();
        }
    }

    private static Set<PaymentApp> deduplicatePaymentApps(List<PaymentApp> apps) {
        // TODO(crbug.com/400531531): Stop special-casing individual payment apps in Chrome.
        apps = maybeRemoveNonInternalGooglePayApps(apps);

        Map<String, PaymentApp> identifierToAppMapping = new HashMap<>();
        int numberOfApps = apps.size();
        for (int i = 0; i < numberOfApps; i++) {
            identifierToAppMapping.put(apps.get(i).getIdentifier(), apps.get(i));
        }

        for (int i = 0; i < numberOfApps; i++) {
            // Used by built-in native payment apps (such as Google Pay) to hide the service worker
            // based payment handler that should be used only on desktop.
            identifierToAppMapping.remove(apps.get(i).getApplicationIdentifierToHide());
        }

        Set<PaymentApp> uniquePaymentApps = new HashSet<>(identifierToAppMapping.values());
        for (PaymentApp app : identifierToAppMapping.values()) {
            // If a preferred payment app is present (e.g. Play Billing within a TWA), all other
            // payment apps are ignored.
            if (app.isPreferred()) {
                uniquePaymentApps.clear();
                uniquePaymentApps.add(app);

                return uniquePaymentApps;
            }

            // The list of native applications from the web app manifest's "related_applications"
            // section. If "prefer_related_applications" is true in the manifest and any one of the
            // related application is installed on the device, then the corresponding service worker
            // will be hidden.
            Set<String> identifiersOfAppsThatHidesThisApp =
                    app.getApplicationIdentifiersThatHideThisApp();
            if (identifiersOfAppsThatHidesThisApp == null) continue;
            for (String identifier : identifiersOfAppsThatHidesThisApp) {
                if (identifierToAppMapping.containsKey(identifier)) uniquePaymentApps.remove(app);
            }
        }

        return uniquePaymentApps;
    }

    /**
     * Removes non-internal versions of the Google Pay app, if the internal version of Google Pay
     * app is present.
     * TODO(crbug.com/400531531): Stop special-casing individual payment apps in Chrome.
     *
     * @param apps The apps to filter.
     * @return The apps without Google Pay duplicates.
     */
    private static List<PaymentApp> maybeRemoveNonInternalGooglePayApps(List<PaymentApp> apps) {
        PaymentApp googlePayInternalApp = null;
        for (PaymentApp app : apps) {
            if (GOOGLE_PAY_INTERNAL_APP_IDENTITY.equals(app.getIdentifier())) {
                googlePayInternalApp = app;
                break;
            }
        }

        if (googlePayInternalApp == null) {
            return apps;
        }

        List<PaymentApp> result = new ArrayList<>();
        for (PaymentApp app : apps) {
            Set<String> methodNames = app.getInstrumentMethodNames();
            boolean isGooglePayApp =
                    methodNames.contains(MethodStrings.GOOGLE_PAY)
                            || methodNames.contains(MethodStrings.GOOGLE_PAY_AUTHENTICATION);
            if (app == googlePayInternalApp || !isGooglePayApp) {
                result.add(app);
            }
        }

        return result;
    }
}
