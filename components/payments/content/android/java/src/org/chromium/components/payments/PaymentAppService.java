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
public class PaymentAppService {
    /**
     * The identity of the Google Pay internal app.
     *
     * <p>TODO(crbug.com/400531531): Stop special-casing individual payment apps in Chrome.
     */
    public static final String GOOGLE_PAY_INTERNAL_APP_IDENTITY = "Google_Pay_Internal";

    private static @Nullable PaymentAppService sInstance;
    private final Map<String, PaymentAppFactoryInterface> mFactories = new HashMap<>();

    /** @return The singleton instance of this class. */
    public static PaymentAppService getInstance() {
        if (sInstance == null) {
            sInstance = new PaymentAppService();
        }
        return sInstance;
    }

    private PaymentAppService() {}

    /** Resets the instance, used by //clank tests. */
    public void resetForTest() {
        sInstance = null;
    }

    /** Trigger creation of payment apps by the factories owned by this class. */
    public void createPaymentApps(PaymentAppServiceDelegate delegate) {
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
        // TODO(crbug.com/474398434): We should be able to assert that the factory does not already
        // exist here, but too many tests (especially under batching) reuse the same factoryId.
        if (mFactories.containsKey(factoryId)) return;
        mFactories.put(factoryId, factory);
    }

    /**
     * Collects payment apps from multiple factories and invokes
     * delegate.onDoneCreatingPaymentApps() and delegate.onCanMakePaymentCalculated() only once.
     */
    private static final class Collector implements PaymentAppFactoryDelegate {
        private final Set<PaymentAppFactoryInterface> mPendingFactories;
        private final List<PaymentApp> mPossiblyDuplicatePaymentApps = new ArrayList<>();
        private final PaymentAppServiceDelegate mDelegate;

        /** Whether at least one payment app factory has calculated canMakePayment to be true. */
        private boolean mCanMakePayment;

        private boolean mHasInternalFactory;

        private Collector(
                Set<PaymentAppFactoryInterface> pendingTasks, PaymentAppServiceDelegate delegate) {
            mPendingFactories = pendingTasks;
            mDelegate = delegate;

            for (PaymentAppFactoryInterface factory : mPendingFactories) {
                if (factory.isInternal()) {
                    mHasInternalFactory = true;
                    break;
                }
            }
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

            // At this point all factories have created any apps that they are going to create. We
            // can now proceed to let our delegate know we are done.

            // If all payment app factories returned false for canMakePayment, then we can now let
            // the delegate know that the answer is definitely false.
            if (!mCanMakePayment) mDelegate.onCanMakePaymentCalculated(false);

            Set<PaymentApp> uniquePaymentApps =
                    deduplicatePaymentApps(mPossiblyDuplicatePaymentApps);
            mPossiblyDuplicatePaymentApps.clear();

            mDelegate.onDoneCreatingPaymentApps(new ArrayList<>(uniquePaymentApps));
        }

        @Override
        public void setOptOutOffered() {
            mDelegate.setOptOutOffered();
        }

        @Override
        public boolean internalPaymentAppFactoryPresent() {
            return mHasInternalFactory;
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
