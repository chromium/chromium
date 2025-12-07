// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Tests for the payment app deduplication logic in the PaymentAppService class. The deduplication
 * logic is private in the PaymentAppService class, so the test cases trigger it through the
 * publicly visible API PaymentAppService.create(PaymentAppFactoryDelegate).
 */
@RunWith(BaseRobolectricTestRunner.class)
public class DeduplicatePaymentAppsUnitTest {
    private final TestFactory mFactory = new TestFactory();
    private final TestDelegate mDelegate = new TestDelegate();
    private final PaymentAppService mService = PaymentAppService.getInstance();

    @Before
    public void setUp() {
        mService.addFactory(mFactory);
    }

    @After
    public void tearDown() {
        mService.resetForTest();
    }

    /**
     * If two non-internal Google Pay apps are found, the PaymentAppService returns both of them.
     */
    @Test
    @Feature({"Payments"})
    public void testNonInternalGooglePayAppsIgnoreEachOther() throws Exception {
        TestApp app1 = new TestApp("id-1");
        app1.setMethod("https://google.com/pay");
        mFactory.addApp(app1);
        TestApp app2 = new TestApp("id-2");
        app2.setMethod("https://pay.google.com/authentication");
        mFactory.addApp(app2);

        // Trigger the deduplication logic:
        mService.create(mDelegate);

        Assert.assertTrue(mDelegate.getIsDoneCreatingPaymentApps());
        Assert.assertEquals(2, mDelegate.getApps().size());
        Set<String> appIdentifiers = new HashSet<>();
        for (PaymentApp app : mDelegate.getApps()) {
            appIdentifiers.add(app.getIdentifier());
        }
        Assert.assertTrue(appIdentifiers.contains("id-1"));
        Assert.assertTrue(appIdentifiers.contains("id-2"));
    }

    /**
     * The presence of the Google Pay internal app makes PaymentAppService ignore all payment apps
     * with the "https://google.com/pay" payment method identifier.
     */
    @Test
    @Feature({"Payments"})
    public void testGoogleInternalAppsHidesTheGooglePayMethodApp() throws Exception {
        mFactory.addApp(new TestApp("Google_Pay_Internal"));
        TestApp notGoogleInternalApp = new TestApp("id-2");
        notGoogleInternalApp.setMethod("https://google.com/pay");
        mFactory.addApp(notGoogleInternalApp);

        // Trigger the deduplication logic:
        mService.create(mDelegate);

        Assert.assertTrue(mDelegate.getIsDoneCreatingPaymentApps());
        Assert.assertEquals(1, mDelegate.getApps().size());
        Assert.assertEquals("Google_Pay_Internal", mDelegate.getApps().get(0).getIdentifier());
    }

    /**
     * The presence of the Google Pay internal app makes PaymentAppService ignore all payment apps
     * with the "https://pay.google.com/authentication" payment method identifier.
     */
    @Test
    @Feature({"Payments"})
    public void testGoogleInternalAppsHidesTheSpaMethodApp() throws Exception {
        mFactory.addApp(new TestApp("Google_Pay_Internal"));
        TestApp notGoogleInternalApp = new TestApp("id-2");
        notGoogleInternalApp.setMethod("https://pay.google.com/authentication");
        mFactory.addApp(notGoogleInternalApp);

        // Trigger the deduplication logic:
        mService.create(mDelegate);

        Assert.assertTrue(mDelegate.getIsDoneCreatingPaymentApps());
        Assert.assertEquals(1, mDelegate.getApps().size());
        Assert.assertEquals("Google_Pay_Internal", mDelegate.getApps().get(0).getIdentifier());
    }

    /**
     * The presence of the Google Pay internal app has no affect on other apps that do not have a
     * Google Pay method identifier.
     */
    @Test
    @Feature({"Payments"})
    public void testGoogleInternalAppsDoesNotAffectNonGooglePayApps() throws Exception {
        mFactory.addApp(new TestApp("Google_Pay_Internal"));
        mFactory.addApp(new TestApp("id-2"));

        // Trigger the deduplication logic:
        mService.create(mDelegate);

        Assert.assertTrue(mDelegate.getIsDoneCreatingPaymentApps());
        Assert.assertEquals(2, mDelegate.getApps().size());
        Set<String> appIdentifiers = new HashSet<>();
        for (PaymentApp app : mDelegate.getApps()) {
            appIdentifiers.add(app.getIdentifier());
        }
        Assert.assertTrue(appIdentifiers.contains("Google_Pay_Internal"));
        Assert.assertTrue(appIdentifiers.contains("id-2"));
    }

    /**
     * If the two payment apps have no information about each other, then PaymentAppService returns
     * both of them.
     */
    @Test
    @Feature({"Payments"})
    public void testNoDeduplicationForAppsThatAreNotAwareOfEachOther() throws Exception {
        mFactory.addApp(new TestApp("id-1"));
        mFactory.addApp(new TestApp("id-2"));

        // Trigger the deduplication logic:
        mService.create(mDelegate);

        Assert.assertTrue(mDelegate.getIsDoneCreatingPaymentApps());
        Assert.assertEquals(2, mDelegate.getApps().size());
    }

    /**
     * If one app specifies that it always hides another app (e.g., an Android payment app may hide
     * the service worker based payment app), then PaymentAppService ignores the other app.
     */
    @Test
    @Feature({"Payments"})
    public void testOneAppCanHideAnother() throws Exception {
        TestApp primaryApp = new TestApp("id-1");
        primaryApp.hideOtherAppWithId("id-2");
        mFactory.addApp(primaryApp);
        mFactory.addApp(new TestApp("id-2"));

        // Trigger the deduplication logic:
        mService.create(mDelegate);

        Assert.assertTrue(mDelegate.getIsDoneCreatingPaymentApps());
        Assert.assertEquals(1, mDelegate.getApps().size());
        Assert.assertEquals("id-1", mDelegate.getApps().get(0).getIdentifier());
    }

    /**
     * If one app specifies that it is always hidden by other apps (e.g., a service worker based
     * payment app being hidden by Android payment apps), then PaymentAppService ignores this app.
     */
    @Test
    @Feature({"Payments"})
    public void testOtherAppsCanHideThisApp() throws Exception {
        TestApp secondaryApp = new TestApp("id-1");
        secondaryApp.hideIfFindOtherAppWithId("id-2");
        mFactory.addApp(secondaryApp);
        mFactory.addApp(new TestApp("id-2"));

        // Trigger the deduplication logic:
        mService.create(mDelegate);

        Assert.assertTrue(mDelegate.getIsDoneCreatingPaymentApps());
        Assert.assertEquals(1, mDelegate.getApps().size());
        Assert.assertEquals("id-2", mDelegate.getApps().get(0).getIdentifier());
    }

    /** If an app is marked "preferred", then PaymentAppService returns only this app. */
    @Test
    @Feature({"Payments"})
    public void testPreferredApp() throws Exception {
        mFactory.addApp(new TestApp("id-1"));
        TestApp preferredApp = new TestApp("id-2");
        preferredApp.setPreferred();
        mFactory.addApp(preferredApp);
        mFactory.addApp(new TestApp("id-3"));

        // Trigger the deduplication logic:
        mService.create(mDelegate);

        Assert.assertTrue(mDelegate.getIsDoneCreatingPaymentApps());
        Assert.assertEquals(1, mDelegate.getApps().size());
        Assert.assertEquals("id-2", mDelegate.getApps().get(0).getIdentifier());
    }

    /** If the factory does not create any apps, then PaymentAppService does not return any apps. */
    @Test
    @Feature({"Payments"})
    public void testNoApps() throws Exception {
        // Trigger the deduplication logic:
        mService.create(mDelegate);

        Assert.assertTrue(mDelegate.getIsDoneCreatingPaymentApps());
        Assert.assertTrue(mDelegate.getApps().isEmpty());
    }

    /** A payment app for testing deduplication. */
    private static final class TestApp extends PaymentApp {
        private final Set<String> mMethods = new HashSet<>();
        private Set<String> mIdsThatHideThisApp;
        private String mIdToHide;
        private boolean mIsPreferred;

        /**
         * Create an instance of a testing payment app with the given identifier.
         *
         * @param id The identifier for this payment app.
         */
        TestApp(String id) {
            super(id, "Label", "Sub-label", /* icon= */ null);
            mMethods.add("https://test.example");
        }

        /**
         * Sets the payment method name (e.g., "https://payments.example") for the this payment app.
         * Each call overrides the previous method name.
         *
         * @param method The payment method name to use for this payment app.
         */
        void setMethod(String methodName) {
            mMethods.clear();
            mMethods.add(methodName);
        }

        /**
         * Hide another app with the given identifier. Should be called at most once, because every
         * call overrides the previous call.
         *
         * @param idToHide The identifier of another payment app that should be hidden, if this app
         *     is present.
         */
        void hideOtherAppWithId(String idToHide) {
            mIdToHide = idToHide;
        }

        /**
         * Hide this app, if an app with the given identifier is found. Can be called multiple
         * times, as each call is additive.
         *
         * @param idThatHidesThisApp An identifier of another payment app that, if found, will hide
         *     this app.
         */
        void hideIfFindOtherAppWithId(String idThatHidesThisApp) {
            if (mIdsThatHideThisApp == null) {
                mIdsThatHideThisApp = new HashSet<>();
            }
            mIdsThatHideThisApp.add(idThatHidesThisApp);
        }

        /** Marks this app a "preferred". */
        void setPreferred() {
            mIsPreferred = true;
        }

        // PaymentApp:
        @Override
        public void dismissInstrument() {}

        // PaymentApp:
        @Override
        public Set<String> getInstrumentMethodNames() {
            return mMethods;
        }

        // PaymentApp:
        @Override
        public @Nullable String getApplicationIdentifierToHide() {
            return mIdToHide;
        }

        // PaymentApp:
        @Override
        public @Nullable Set<String> getApplicationIdentifiersThatHideThisApp() {
            return mIdsThatHideThisApp;
        }

        // PaymentApp:
        @Override
        public boolean isPreferred() {
            return mIsPreferred;
        }
    }

    /** A payment app factory for testing. */
    private static final class TestFactory implements PaymentAppFactoryInterface {
        private final List<PaymentApp> mApps = new ArrayList<>();

        /**
         * Add to the list of payment apps that should be "created" in this factory. Can be called
         * multiple times, as each call is additive.
         *
         * @param app A payment app that should be returned from this factory.
         */
        void addApp(PaymentApp app) {
            mApps.add(app);
        }

        // PaymentAppFactoryInterface:
        @Override
        public void create(PaymentAppFactoryDelegate delegate) {
            delegate.onCanMakePaymentCalculated(true);
            for (PaymentApp app : mApps) {
                delegate.onPaymentAppCreated(app);
            }
            delegate.onDoneCreatingPaymentApps(this);
        }
    }

    /** The class for receiving the list of payment apps from the payment app service. */
    private static final class TestDelegate implements PaymentAppFactoryDelegate {
        private final List<PaymentApp> mApps = new ArrayList<>();
        private boolean mIsDoneCreatingPaymentApps;

        /**
         * @return The list of apps that has been received from the payment app service.
         */
        List<PaymentApp> getApps() {
            return Collections.unmodifiableList(mApps);
        }

        /**
         * @return Whether the payment app service has finished creating payment apps (i.e.,
         *     getApps() returns the finalized list that will not change).
         */
        boolean getIsDoneCreatingPaymentApps() {
            return mIsDoneCreatingPaymentApps;
        }

        // PaymentAppFactoryDelegate:
        @Override
        public void onPaymentAppCreated(PaymentApp paymentApp) {
            mApps.add(paymentApp);
        }

        // PaymentAppFactoryDelegate:
        @Override
        public boolean prefsCanMakePayment() {
            return true;
        }

        // PaymentAppFactoryDelegate:
        @Override
        public void onDoneCreatingPaymentApps(PaymentAppFactoryInterface factory) {
            mIsDoneCreatingPaymentApps = true;
        }

        // PaymentAppFactoryDelegate:
        @Override
        public CSPChecker getCSPChecker() {
            return null;
        }

        // PaymentAppFactoryDelegate:
        @Override
        public PaymentAppFactoryParams getParams() {
            return null;
        }

        // PaymentAppFactoryDelegate:
        @Override
        public boolean isFullDelegationRequired() {
            return false;
        }
    }
}
