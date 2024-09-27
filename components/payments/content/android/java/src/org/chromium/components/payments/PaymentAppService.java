// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Creates payment apps. */
public class PaymentAppService implements PaymentAppFactoryInterface {
    private static final String UNTRACKED_FACTORY_ID_PREFIX = "Untracked factory - ";
    private static PaymentAppService sInstance;
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
    }

    private static Set<PaymentApp> deduplicatePaymentApps(List<PaymentApp> apps) {
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
}
