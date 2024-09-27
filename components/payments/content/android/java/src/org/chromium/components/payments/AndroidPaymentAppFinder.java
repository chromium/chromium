// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.payments.PaymentManifestVerifier.ManifestVerifyCallback;
import org.chromium.components.payments.intent.WebPaymentIntentHelper;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Finds installed native Android payment apps and verifies their signatures according to the
 * payment method manifests. The manifests are located based on the payment method name, which is a
 * URL that starts with "https://" (localhosts can be "http://", however). The W3C-published non-URL
 * payment method names are exceptions: these are common payment method names that do not have a
 * manifest and can be used by any payment app.
 */
public class AndroidPaymentAppFinder implements ManifestVerifyCallback {
    private static final String TAG = "PaymentAppFinder";

    private static final String PLAY_STORE_PACKAGE_NAME = "com.android.vending";

    /** The maximum number of payment method manifests to download. */
    private static final int MAX_NUMBER_OF_MANIFESTS = 10;

    /** The name of the intent for the service to check whether an app is ready to pay. */
    public static final String ACTION_IS_READY_TO_PAY =
            "org.chromium.intent.action.IS_READY_TO_PAY";

    /** Meta data name of an app's supported payment method names. */
    public static final String META_DATA_NAME_OF_PAYMENT_METHOD_NAMES =
            "org.chromium.payment_method_names";

    /** Meta data name of an app's supported default payment method name. */
    public static final String META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME =
            "org.chromium.default_payment_method_name";

    /** Meta data name of an app's supported delegations' list. */
    public static final String META_DATA_NAME_OF_SUPPORTED_DELEGATIONS =
            "org.chromium.payment_supported_delegations";

    private final Set<GURL> mUrlPaymentMethods = new HashSet<>();
    private final PaymentManifestDownloader mDownloader;
    private final PaymentManifestWebDataService mWebDataService;
    private final PaymentManifestParser mParser;
    private final PackageManagerDelegate mPackageManagerDelegate;
    private final PaymentAppFactoryDelegate mFactoryDelegate;
    private final PaymentAppFactoryInterface mFactory;
    private final boolean mIsOffTheRecord;

    /**
     * The app stores that supports app-store billing methods.
     *
     * key: the app-store app's package name, e.g., "com.google.vendor" (Google Play Store).
     * value: the app-store app's billing method identifier, e.g.,
     * "https://play.google.com/billing". Only valid GURLs are allowed.
     */
    private final Map<String, GURL> mAppStores = new HashMap();

    /**
     * A mapping from an Android package name to the payment app with that package name. The apps
     * will be sent to the <code>PaymentAppFactoryDelegate</code> once all of their payment methods
     * have been validated. The package names are used for identification because they are unique on
     * Android. Example contents:
     *
     * {"com.bobpay.app.v1": androidPaymentApp1, "com.alicepay.app.v1": androidPaymentApp2}
     */
    private final Map<String, AndroidPaymentApp> mValidApps = new HashMap<>();

    /**
     * A mapping from origins of payment apps to the URL payment methods of these apps. Used to look
     * up payment apps in <code>mVerifiedPaymentMethods</code> based on the supported origins that
     * have been verified in <code>PaymentManifestVerifier</code>. Example contents:
     *
     * {"https://bobpay.com": ("https://bobpay.com/personal", "https://bobpay.com/business")}
     */
    private final Map<GURL, Set<GURL>> mOriginToUrlDefaultMethodsMapping = new HashMap<>();

    /**
     * A mapping from URL payment methods to the applications that support this payment method,
     * but not as their default payment method. Used to find all apps that claim support for a given
     * URL payment method when the payment manifest of this method contains
     * "supported_origins": "*". Example contents:
     *
     * {"https://bobpay.com/public-standard": (resolveInfo1, resolveInfo2, resolveInfo3)}
     */
    private final Map<GURL, Set<ResolveInfo>> mMethodToSupportedAppsMapping = new HashMap<>();

    /** Contains information about a URL payment method. */
    private static final class PaymentMethod {
        /** The default applications for this payment method. */
        public final Set<ResolveInfo> defaultApplications = new HashSet<>();

        /** The supported origins of this payment method. */
        public final Set<GURL> supportedOrigins = new HashSet<>();
    }

    /**
     * A mapping from URL payment methods to the verified information about these methods. Used to
     * accumulate the incremental information that arrives from
     * <code>PaymentManifestVerifier</code>s for each of the payment method manifests that need to
     * be downloaded. Example contents:
     *
     * { "https://bobpay.com/business": method1, "https://bobpay.com/personal": method2}
     */
    private final Map<GURL, PaymentMethod> mVerifiedPaymentMethods = new HashMap<>();

    /*
     * A mapping from package names to their IS_READY_TO_PAY service names, e.g.:
     *
     * {"com.bobpay.app": "com.bobpay.app.IsReadyToPayService"}
     */
    private final Map<String, String> mIsReadyToPayServices = new HashMap<>();

    private int mPendingVerifiersCount;
    private int mPendingIsReadyToPayQueries;
    private int mPendingResourceUsersCount;
    private boolean mBypassIsReadyToPayServiceInTest;

    /**
     * Builds a finder for native Android payment apps.
     *
     * @param webDataService The web data service to cache manifest.
     * @param downloader The manifest downloader.
     * @param parser The manifest parser.
     * @param packageManagerDelegate The package information retriever.
     * @param factoryDelegate The merchant requested data and the asynchronous delegate to be
     *         invoked (on the UI thread) when all Android payment apps have been found.
     * @param factory The factory to be used in the delegate.onDoneCreatingPaymentApps(factory)
     *         call.
     */
    public AndroidPaymentAppFinder(
            PaymentManifestWebDataService webDataService,
            PaymentManifestDownloader downloader,
            PaymentManifestParser parser,
            PackageManagerDelegate packageManagerDelegate,
            PaymentAppFactoryDelegate factoryDelegate,
            PaymentAppFactoryInterface factory) {
        mFactoryDelegate = factoryDelegate;

        mAppStores.put(PLAY_STORE_PACKAGE_NAME, new GURL(MethodStrings.GOOGLE_PLAY_BILLING));
        for (GURL method : mAppStores.values()) {
            assert method.isValid();
        }

        mDownloader = downloader;
        mWebDataService = webDataService;
        mParser = parser;
        mPackageManagerDelegate = packageManagerDelegate;
        mFactory = factory;
        assert mFactoryDelegate.getParams() != null;
        mIsOffTheRecord = mFactoryDelegate.getParams().isOffTheRecord();
    }

    private void findAppStoreBillingApp(List<ResolveInfo> allInstalledPaymentApps) {
        assert !mFactoryDelegate.getParams().hasClosed();
        String twaPackageName = mFactoryDelegate.getParams().getTwaPackageName();
        if (TextUtils.isEmpty(twaPackageName)) return;
        ResolveInfo twaApp = findAppWithPackageName(allInstalledPaymentApps, twaPackageName);
        if (twaApp == null) return;
        List<String> agreedAppStoreMethods = new ArrayList<>();
        for (GURL appStoreUriMethod : mAppStores.values()) {
            assert appStoreUriMethod != null;
            assert appStoreUriMethod.isValid();
            String appStoreMethod = removeTrailingSlash(appStoreUriMethod.getSpec());
            assert appStoreMethod != null;
            if (!mFactoryDelegate.getParams().getMethodData().containsKey(appStoreMethod)) continue;
            if (!paymentAppSupportsUriMethod(twaApp, appStoreUriMethod)) continue;
            agreedAppStoreMethods.add(appStoreMethod);
        }

        boolean allowTwaInstalledFromAnySource =
                PaymentFeatureList.isEnabled(
                        PaymentFeatureList.WEB_PAYMENTS_APP_STORE_BILLING_DEBUG);
        if (!allowTwaInstalledFromAnySource) {
            String installerPackageName =
                    mPackageManagerDelegate.getInstallerPackage(twaPackageName);
            if (installerPackageName == null) return;
            GURL appStoreUriMethod = mAppStores.get(installerPackageName);
            if (appStoreUriMethod == null) return;
            assert appStoreUriMethod.isValid();

            String method = appStoreUriMethod.getSpec();
            if (!agreedAppStoreMethods.contains(method)) return;
            onValidPaymentAppForPaymentMethodName(twaApp, method);
        } else {
            for (String appStoreMethod : agreedAppStoreMethods) {
                onValidPaymentAppForPaymentMethodName(twaApp, appStoreMethod);
            }
        }

        AndroidPaymentApp app = mValidApps.get(twaPackageName);
        if (app != null) app.setIsPreferred(true);
    }

    private boolean paymentAppSupportsUriMethod(ResolveInfo app, GURL urlMethod) {
        String defaultMethod =
                app.activityInfo.metaData == null
                        ? null
                        : app.activityInfo.metaData.getString(
                                META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME);
        GURL defaultUrlMethod = new GURL(defaultMethod);
        assert urlMethod.isValid();
        return getSupportedPaymentMethods(app.activityInfo).contains(urlMethod.getSpec())
                || urlMethod.equals(defaultUrlMethod);
    }

    private ResolveInfo findAppWithPackageName(List<ResolveInfo> apps, String packageName) {
        assert packageName != null;
        for (int i = 0; i < apps.size(); i++) {
            ResolveInfo app = apps.get(i);
            String appPackageName = app.activityInfo.packageName;
            if (packageName.equals(appPackageName)) return app;
        }
        return null;
    }

    /**
     * Finds and validates the installed android payment apps that support the payment method names
     * that the merchant is using.
     */
    public void findAndroidPaymentApps() {
        if (mFactoryDelegate.getParams().hasClosed()) return;
        for (String method : mFactoryDelegate.getParams().getMethodData().keySet()) {
            assert !TextUtils.isEmpty(method);
            GURL url = new GURL(method); // Only URL payment method names are supported.
            if (mAppStores.containsValue(url)) continue;
            if (UrlUtil.isValidUrlBasedPaymentMethodIdentifier(url)) {
                mUrlPaymentMethods.add(url);
            }
        }

        List<ResolveInfo> allInstalledPaymentApps =
                mPackageManagerDelegate.getActivitiesThatCanRespondToIntentWithMetaData(
                        new Intent(WebPaymentIntentHelper.ACTION_PAY));
        if (allInstalledPaymentApps.isEmpty()) {
            onAllAppsFoundAndValidated();
            return;
        }

        if (!mIsOffTheRecord) {
            List<ResolveInfo> services =
                    mPackageManagerDelegate.getServicesThatCanRespondToIntent(
                            new Intent(ACTION_IS_READY_TO_PAY));
            int numberOfServices = services.size();
            for (int i = 0; i < numberOfServices; i++) {
                ServiceInfo service = services.get(i).serviceInfo;
                mIsReadyToPayServices.put(service.packageName, service.name);
            }
        }

        if (!PaymentOptionsUtils.requestAnyInformation(
                        mFactoryDelegate.getParams().getPaymentOptions())
                && PaymentFeatureList.isEnabled(
                        PaymentFeatureList.WEB_PAYMENTS_APP_STORE_BILLING)) {
            findAppStoreBillingApp(allInstalledPaymentApps);
        }

        // All URL methods for which manifests should be downloaded. For example, if merchant
        // supports "https://bobpay.com/personal" payment method, but user also has Alice Pay app
        // that has the default payment method name of "https://alicepay.com/webpay" that claims to
        // support "https://bobpay.com/personal" method as well, then both of these methods will be
        // in this set:
        //
        // ("https://bobpay.com/personal", "https://alicepay.com/webpay")
        Set<GURL> urlMethods = new HashSet<>(mUrlPaymentMethods);

        // A mapping from all known payment method names to the corresponding payment apps that
        // claim to support these payment methods. Example contents:
        //
        // {"basic-card": (bobPay, alicePay), "https://alicepay.com/webpay": (alicePay)}
        //
        // In case of non-URL payment methods, such as "basic-card", all apps that claim to support
        // it are considered valid. In case of URL payment methods, if no apps claim to support a
        // URL method, then no information will be downloaded for this method.
        Map<String, Set<ResolveInfo>> methodToAppsMapping = new HashMap<>();

        // A mapping from URL payment method names to the corresponding default payment apps. The
        // payment manifest verifiers compare these apps against the information in
        // "default_applications" of the payment method manifests to determine the validity of these
        // apps. Example contents:
        //
        // {"https://bobpay.com/personal": (bobPay), "https://alicepay.com/webpay": (alicePay)}
        Map<GURL, Set<ResolveInfo>> urlMethodToDefaultAppsMapping = new HashMap<>();

        // A mapping from URL payment method names to the origins of the payment apps that support
        // that method name. The payment manifest verifiers compare these origins against the
        // information in "supported_origins" of the payment method manifests to determine validity
        // of these origins. Example contents:
        //
        // {"https://bobpay.com/personal": ("https://alicepay.com")}
        Map<GURL, Set<GURL>> urlMethodToSupportedOriginsMapping = new HashMap<>();

        for (int i = 0; i < allInstalledPaymentApps.size(); i++) {
            ResolveInfo app = allInstalledPaymentApps.get(i);

            String defaultMethod =
                    app.activityInfo.metaData == null
                            ? null
                            : app.activityInfo.metaData.getString(
                                    META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME);

            GURL appOrigin = null;
            GURL defaultUrlMethod = null;
            if (!TextUtils.isEmpty(defaultMethod)) {
                defaultUrlMethod = new GURL(defaultMethod);

                // Do not download any manifests for the app whose default payment method identifier
                // is an app store payment method identifier, because app store method URLs are used
                // only for identification and do not host manifest files.
                if (mAppStores.values().contains(defaultUrlMethod)) {
                    continue;
                }

                if (UrlUtil.isURLValid(defaultUrlMethod)) {
                    defaultMethod = urlToStringWithoutTrailingSlash(defaultUrlMethod);
                }
                if (!methodToAppsMapping.containsKey(defaultMethod)) {
                    methodToAppsMapping.put(defaultMethod, new HashSet<ResolveInfo>());
                }
                methodToAppsMapping.get(defaultMethod).add(app);

                if (UrlUtil.isURLValid(defaultUrlMethod)) {
                    urlMethods.add(defaultUrlMethod);

                    if (!urlMethodToDefaultAppsMapping.containsKey(defaultUrlMethod)) {
                        urlMethodToDefaultAppsMapping.put(
                                defaultUrlMethod, new HashSet<ResolveInfo>());
                    }
                    urlMethodToDefaultAppsMapping.get(defaultUrlMethod).add(app);

                    appOrigin = defaultUrlMethod.getOrigin();
                    if (!mOriginToUrlDefaultMethodsMapping.containsKey(appOrigin)) {
                        mOriginToUrlDefaultMethodsMapping.put(appOrigin, new HashSet<GURL>());
                    }
                    mOriginToUrlDefaultMethodsMapping.get(appOrigin).add(defaultUrlMethod);
                }
            }

            // Note that a payment app with non-URL default payment method (e.g., "basic-card")
            // can support URL payment methods (e.g., "https://bobpay.com/public-standard").
            Set<String> supportedMethods = getSupportedPaymentMethods(app.activityInfo);
            for (String supportedMethod : supportedMethods) {
                GURL supportedUrlMethod = new GURL(supportedMethod);
                if (!UrlUtil.isURLValid(supportedUrlMethod)) supportedUrlMethod = null;
                if (supportedUrlMethod != null && supportedUrlMethod.equals(defaultUrlMethod)) {
                    continue;
                }

                // Ignore payment method identifiers of app stores, because app store method URLs
                // are used only for identification and do not host manifest files.
                if (mAppStores.values().contains(supportedUrlMethod)) {
                    continue;
                }

                if (!methodToAppsMapping.containsKey(supportedMethod)) {
                    methodToAppsMapping.put(supportedMethod, new HashSet<ResolveInfo>());
                }
                methodToAppsMapping.get(supportedMethod).add(app);

                if (supportedUrlMethod == null) continue;

                if (!mMethodToSupportedAppsMapping.containsKey(supportedUrlMethod)) {
                    mMethodToSupportedAppsMapping.put(
                            supportedUrlMethod, new HashSet<ResolveInfo>());
                }
                mMethodToSupportedAppsMapping.get(supportedUrlMethod).add(app);

                if (appOrigin == null) continue;

                if (!urlMethodToSupportedOriginsMapping.containsKey(supportedUrlMethod)) {
                    urlMethodToSupportedOriginsMapping.put(supportedUrlMethod, new HashSet<GURL>());
                }
                urlMethodToSupportedOriginsMapping.get(supportedUrlMethod).add(appOrigin);
            }

            // Record the total number of payment methods that this activity `ResolveInfo app`
            // declares to support in its metadata.
            if (!TextUtils.isEmpty(defaultMethod)) supportedMethods.add(defaultMethod);
            RecordHistogram.recordCustomCountHistogram(
                    /* name= */ "PaymentRequest.NumberOfSupportedMethods.AndroidApp",
                    /* sample= */ supportedMethods.size(),
                    /* min= */ 1,
                    /* max= */ 10,
                    /* numBuckets= */ 10);
        }

        List<PaymentManifestVerifier> manifestVerifiers = new ArrayList<>();
        for (GURL urlMethodName : urlMethods) {
            if (!methodToAppsMapping.containsKey(urlToStringWithoutTrailingSlash(urlMethodName))) {
                continue;
            }

            if (!mParser.isNativeInitialized()) {
                mParser.createNative(mFactoryDelegate.getParams().getWebContents());
            }

            // Initialize the native side of the downloader, once we know that a manifest file needs
            // to be downloaded.
            if (!mDownloader.isInitialized()) {
                mDownloader.initialize(
                        mFactoryDelegate.getParams().getWebContents(),
                        mFactoryDelegate.getCSPChecker());
            }

            manifestVerifiers.add(
                    new PaymentManifestVerifier(
                            mFactoryDelegate.getParams().getPaymentRequestSecurityOrigin(),
                            urlMethodName,
                            urlMethodToDefaultAppsMapping.get(urlMethodName),
                            urlMethodToSupportedOriginsMapping.get(urlMethodName),
                            mWebDataService,
                            mDownloader,
                            mParser,
                            mPackageManagerDelegate,
                            /* callback= */ this));

            if (manifestVerifiers.size() == MAX_NUMBER_OF_MANIFESTS) {
                Log.e(TAG, "Reached maximum number of allowed payment app manifests.");
                break;
            }
        }

        if (manifestVerifiers.isEmpty()) {
            onAllAppsFoundAndValidated();
            return;
        }

        mPendingVerifiersCount = mPendingResourceUsersCount = manifestVerifiers.size();
        for (PaymentManifestVerifier manifestVerifier : manifestVerifiers) {
            manifestVerifier.verify();
        }
    }

    /**
     * Queries the Android app metadata for the names of the non-default payment methods that the
     * given app supports.
     *
     * @param activityInfo The application information to query.
     * @return The set of non-default payment method names that this application supports. Never
     *         null.
     */
    private Set<String> getSupportedPaymentMethods(ActivityInfo activityInfo) {
        Set<String> result = new HashSet<>();
        String[] nonDefaultPaymentMethodNames =
                getStringArrayMetaData(activityInfo, META_DATA_NAME_OF_PAYMENT_METHOD_NAMES);
        if (nonDefaultPaymentMethodNames == null) return result;

        // Normalize methods that look like URLs in the same way they will be normalized in
        // #findAndroidPaymentApps.
        for (String method : nonDefaultPaymentMethodNames) {
            GURL urlMethod = new GURL(method);
            result.add(
                    UrlUtil.isURLValid(urlMethod)
                            ? urlToStringWithoutTrailingSlash(urlMethod)
                            : method);
        }

        return result;
    }

    /**
     * Queries the Android app metadata for a string array.
     * @param activityInfo The application information to query.
     * @param metaDataName The name of the string array meta data to be retrieved.
     * @return The string array.
     */
    @Nullable
    private String[] getStringArrayMetaData(ActivityInfo activityInfo, String metaDataName) {
        if (activityInfo.metaData == null) return null;

        int resId = activityInfo.metaData.getInt(metaDataName);
        if (resId == 0) return null;

        return mPackageManagerDelegate.getStringArrayResourceForApplication(
                activityInfo.applicationInfo, resId);
    }

    @Override
    public void onValidDefaultPaymentApp(GURL methodName, ResolveInfo resolveInfo) {
        getOrCreateVerifiedPaymentMethod(methodName).defaultApplications.add(resolveInfo);
    }

    @Override
    public void onValidSupportedOrigin(GURL methodName, GURL supportedOrigin) {
        getOrCreateVerifiedPaymentMethod(methodName).supportedOrigins.add(supportedOrigin);
    }

    private PaymentMethod getOrCreateVerifiedPaymentMethod(GURL methodName) {
        PaymentMethod verifiedPaymentManifest = mVerifiedPaymentMethods.get(methodName);
        if (verifiedPaymentManifest == null) {
            verifiedPaymentManifest = new PaymentMethod();
            mVerifiedPaymentMethods.put(methodName, verifiedPaymentManifest);
        }
        return verifiedPaymentManifest;
    }

    @Override
    public void onVerificationError(String errorMessage) {
        mFactoryDelegate.onPaymentAppCreationError(errorMessage, AppCreationFailureReason.UNKNOWN);
    }

    @Override
    public void onFinishedVerification() {
        mPendingVerifiersCount--;
        if (mPendingVerifiersCount != 0) return;

        for (Map.Entry<GURL, PaymentMethod> nameAndMethod : mVerifiedPaymentMethods.entrySet()) {
            GURL methodName = nameAndMethod.getKey();
            if (!mUrlPaymentMethods.contains(methodName)) continue;

            PaymentMethod method = nameAndMethod.getValue();
            String methodNameString = urlToStringWithoutTrailingSlash(methodName);
            for (ResolveInfo app : method.defaultApplications) {
                onValidPaymentAppForPaymentMethodName(app, methodNameString);
            }

            for (GURL supportedOrigin : method.supportedOrigins) {
                Set<GURL> supportedAppMethodNames =
                        mOriginToUrlDefaultMethodsMapping.get(supportedOrigin);
                if (supportedAppMethodNames == null) continue;

                for (GURL supportedAppMethodName : supportedAppMethodNames) {
                    PaymentMethod supportedAppMethod =
                            mVerifiedPaymentMethods.get(supportedAppMethodName);
                    if (supportedAppMethod == null) continue;

                    for (ResolveInfo supportedApp : supportedAppMethod.defaultApplications) {
                        onValidPaymentAppForPaymentMethodName(supportedApp, methodNameString);
                    }
                }
            }
        }

        onAllAppsFoundAndValidated();
    }

    /**
     * Queries the IS_READY_TO_PAY service of all valid payment apps. Only valid payment apps
     * receive IS_READY_TO_PAY query to avoid exposing browsing history to malicious apps.
     *
     * Must be done after all verifiers have finished because some manifest files may validate
     * multiple apps and some apps may require multiple manifest file for verification.
     */
    private void onAllAppsFoundAndValidated() {
        assert mPendingVerifiersCount == 0;

        mFactoryDelegate.onCanMakePaymentCalculated(mValidApps.size() > 0);
        if (mValidApps.isEmpty() || mFactoryDelegate.getParams().hasClosed()) {
            mFactoryDelegate.onDoneCreatingPaymentApps(mFactory);
            return;
        }

        mPendingIsReadyToPayQueries = mValidApps.size();
        for (Map.Entry<String, AndroidPaymentApp> entry : mValidApps.entrySet()) {
            AndroidPaymentApp app = entry.getValue();
            if (mBypassIsReadyToPayServiceInTest) app.bypassIsReadyToPayServiceInTest();
            app.maybeQueryIsReadyToPayService(
                    filterMethodDataForApp(
                            mFactoryDelegate.getParams().getMethodData(),
                            app.getInstrumentMethodNames()),
                    mFactoryDelegate.getParams().getTopLevelOrigin(),
                    mFactoryDelegate.getParams().getPaymentRequestOrigin(),
                    mFactoryDelegate.getParams().getCertificateChain(),
                    filterModifiersForApp(
                            mFactoryDelegate.getParams().getUnmodifiableModifiers(),
                            app.getInstrumentMethodNames()),
                    this::onIsReadyToPayResponse);
        }
    }

    @VisibleForTesting
    public void bypassIsReadyToPayServiceInTest() {
        mBypassIsReadyToPayServiceInTest = true;
    }

    private static Map<String, PaymentMethodData> filterMethodDataForApp(
            Map<String, PaymentMethodData> methodData, Set<String> appMethodNames) {
        Map<String, PaymentMethodData> filtered = new HashMap<>();
        for (String methodName : appMethodNames) {
            if (methodData.containsKey(methodName)) {
                filtered.put(methodName, methodData.get(methodName));
            }
        }
        return filtered;
    }

    private static Map<String, PaymentDetailsModifier> filterModifiersForApp(
            Map<String, PaymentDetailsModifier> modifiers, Set<String> appMethodNames) {
        Map<String, PaymentDetailsModifier> filtered = new HashMap<>();
        for (String methodName : appMethodNames) {
            if (modifiers.containsKey(methodName)) {
                filtered.put(methodName, modifiers.get(methodName));
            }
        }
        return filtered;
    }

    private void onIsReadyToPayResponse(AndroidPaymentApp app, boolean isReadyToPay) {
        if (isReadyToPay) mFactoryDelegate.onPaymentAppCreated(app);
        if (--mPendingIsReadyToPayQueries == 0) {
            mFactoryDelegate.onDoneCreatingPaymentApps(mFactory);
        }
    }

    /**
     * Enables the given payment app to use this method name.
     *
     * @param resolveInfo The payment app that's allowed to use the method name.
     * @param methodName  The method name that can be used by the app.
     */
    private void onValidPaymentAppForPaymentMethodName(ResolveInfo resolveInfo, String methodName) {
        if (mFactoryDelegate.getParams().hasClosed()) return;
        String packageName = resolveInfo.activityInfo.packageName;

        SupportedDelegations appSupportedDelegations =
                getAppsSupportedDelegations(resolveInfo.activityInfo);
        // Allow-lists the Play Billing method for this feature in order for the Play Billing case
        // to skip the sheet in this case.
        if (PaymentFeatureList.isEnabled(PaymentFeatureList.ENFORCE_FULL_DELEGATION)
                || methodName.equals(MethodStrings.GOOGLE_PLAY_BILLING)) {
            if (!appSupportedDelegations.providesAll(
                    mFactoryDelegate.getParams().getPaymentOptions())) {
                Log.e(TAG, ErrorStrings.SKIP_APP_FOR_PARTIAL_DELEGATION.replace("$", packageName));
                return;
            }
        }

        AndroidPaymentApp app = mValidApps.get(packageName);
        if (app == null) {
            CharSequence label = mPackageManagerDelegate.getAppLabel(resolveInfo);
            if (TextUtils.isEmpty(label)) {
                Log.e(TAG, "Skipping \"%s\" because of empty label.", packageName);
                return;
            }

            // Dedupe corresponding payment handler which is registered with the default
            // payment method name as the scope and the scope is used as the app Id.
            String webAppIdCanDeduped =
                    resolveInfo.activityInfo.metaData == null
                            ? null
                            : resolveInfo.activityInfo.metaData.getString(
                                    META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME);
            app =
                    new AndroidPaymentApp(
                            new AndroidPaymentApp.LauncherImpl(
                                    mFactoryDelegate.getParams().getWebContents()),
                            packageName,
                            resolveInfo.activityInfo.name,
                            mIsReadyToPayServices.get(packageName),
                            label.toString(),
                            mPackageManagerDelegate.getAppIcon(resolveInfo),
                            mIsOffTheRecord,
                            webAppIdCanDeduped,
                            appSupportedDelegations,
                            PaymentFeatureList.isEnabled(
                                    PaymentFeatureList.SHOW_READY_TO_PAY_DEBUG_INFO));
            mValidApps.put(packageName, app);
        }

        // The same method may be added multiple times.
        app.addMethodName(methodName);
    }

    private SupportedDelegations getAppsSupportedDelegations(ActivityInfo activityInfo) {
        String[] supportedDelegationNames =
                getStringArrayMetaData(activityInfo, META_DATA_NAME_OF_SUPPORTED_DELEGATIONS);
        return SupportedDelegations.createFromStringArray(supportedDelegationNames);
    }

    @Override
    public void onFinishedUsingResources() {
        mPendingResourceUsersCount--;
        if (mPendingResourceUsersCount != 0) return;

        mWebDataService.destroy();
        if (mDownloader.isInitialized()) mDownloader.destroy();
        if (mParser.isNativeInitialized()) mParser.destroyNative();
    }

    /**
     * Converts the given URL to a string without a trailing slash, because payment method
     * identifiers typically omit trailing slashes, e.g., "https://google.com/pay" is correct,
     * whereas "https://google.com/pay/" is incorrect. This is important because matching payment
     * apps to payment requests happens by string equality. Note that GURL.getSpec() can append
     * trailing slashes in some instances.
     * @param url The URL to stringify.
     * @return The URL string without a trailing slash, or null if the input parameter is null.
     */
    @Nullable
    private static String urlToStringWithoutTrailingSlash(@Nullable GURL url) {
        if (url == null) return null;
        return removeTrailingSlash(url.getSpec());
    }

    @Nullable
    private static String removeTrailingSlash(@Nullable String string) {
        if (string == null) return null;
        return string.endsWith("/") ? string.substring(0, string.length() - 1) : string;
    }

    /**
     * Add an app store for testing.
     *
     * @param packageName The package name of the app store.
     * @param paymentMethod The payment method identifier of the app store.
     */
    public void addAppStoreForTest(String packageName, GURL paymentMethod) {
        assert paymentMethod.isValid();
        mAppStores.put(packageName, paymentMethod);
    }
}
