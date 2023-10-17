// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.commerce.core;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

/** A central hub for accessing shopping and product information. */
@JNINamespace("commerce")
public class ShoppingService {
    /** A data container for product info provided by the shopping service. */
    public static final class ProductInfo {
        public final String title;
        public final GURL imageUrl;
        public final Optional<Long> productClusterId;
        public final Optional<Long> offerId;
        public final String currencyCode;
        public final long amountMicros;
        public final Optional<Long> previousAmountMicros;
        public final String countryCode;

        public ProductInfo(String title, GURL imageUrl, Optional<Long> productClusterId,
                Optional<Long> offerId, String currencyCode, long amountMicros, String countryCode,
                Optional<Long> previousAmountMicros) {
            this.title = title;
            this.imageUrl = imageUrl;
            this.productClusterId = productClusterId;
            this.offerId = offerId;
            this.currencyCode = currencyCode;
            this.amountMicros = amountMicros;
            this.previousAmountMicros = previousAmountMicros;
            this.countryCode = countryCode;
        }
    }

    /** A data container for merchant info provided by the shopping service. */
    public static final class MerchantInfo {
        public final float starRating;
        public final int countRating;
        public final GURL detailsPageUrl;
        public final boolean hasReturnPolicy;
        public final float nonPersonalizedFamiliarityScore;
        public final boolean containsSensitiveContent;
        public final boolean proactiveMessageDisabled;

        @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
        public MerchantInfo(float starRating, int countRating, GURL detailsPageUrl,
                boolean hasReturnPolicy, float nonPersonalizedFamiliarityScore,
                boolean containsSensitiveContent, boolean proactiveMessageDisabled) {
            this.starRating = starRating;
            this.countRating = countRating;
            this.detailsPageUrl = detailsPageUrl;
            this.hasReturnPolicy = hasReturnPolicy;
            this.nonPersonalizedFamiliarityScore = nonPersonalizedFamiliarityScore;
            this.containsSensitiveContent = containsSensitiveContent;
            this.proactiveMessageDisabled = proactiveMessageDisabled;
        }
    }

    /** A callback for acquiring product information about a page. */
    public interface ProductInfoCallback {
        /**
         * A notification that fetching product information for the URL has completed.
         * @param url The URL the product info was fetched for.
         * @param info The product info for the URL or {@code null} if none is available.
         */
        void onResult(GURL url, ProductInfo info);
    }

    /** A callback for acquiring merchant information about a page. */
    public interface MerchantInfoCallback {
        /**
         * A notification that fetching merchant information for the URL has completed.
         * @param url The URL the merchant info was fetched for.
         * @param info The merchant info for the URL or {@code null} if none is available.
         */
        void onResult(GURL url, MerchantInfo info);
    }

    /** A pointer to the native side of the object. */
    private long mNativeShoppingServiceAndroid;

    private final ObserverList<SubscriptionsObserver> mSubscriptionsObservers =
            new ObserverList<>();

    /** Private constructor to ensure construction only happens by native. */
    private ShoppingService(long nativePtr) {
        mNativeShoppingServiceAndroid = nativePtr;
    }

    /**
     * Fetch information about a product for a URL.
     * @param url The URL to fetch product info for.
     * @param callback The callback that will run after the fetch is completed. The product info
     *                 object will be null if there is none available.
     */
    public void getProductInfoForUrl(GURL url, ProductInfoCallback callback) {
        if (mNativeShoppingServiceAndroid == 0) return;

        ShoppingServiceJni.get().getProductInfoForUrl(
                mNativeShoppingServiceAndroid, this, url, callback);
    }

    /**
     * Get the currently available product information for the specified URL. This method may return
     * {@code null} or partial data if the page has not yet been completely processed. This is less
     * reliable than {@link #getProductInfoForUrl(GURL, ProductInfoCallback)}.
     * @param url The URL to fetch product info for.
     */
    public ProductInfo getAvailableProductInfoForUrl(GURL url) {
        if (mNativeShoppingServiceAndroid == 0) return null;

        return ShoppingServiceJni.get().getAvailableProductInfoForUrl(
                mNativeShoppingServiceAndroid, this, url);
    }

    /**
     * Fetch information about a merchant for a URL.
     * @param url The URL to fetch merchant info for.
     * @param callback The callback that will run after the fetch is completed. The merchant info
     *                 object will be null if there is none available.
     */
    public void getMerchantInfoForUrl(GURL url, MerchantInfoCallback callback) {
        if (mNativeShoppingServiceAndroid == 0) return;

        ShoppingServiceJni.get().getMerchantInfoForUrl(
                mNativeShoppingServiceAndroid, this, url, callback);
    }

    /**
     * Requests that the service fetch the price notification email preference from the backend.
     * This call will update the preference kept by the pref service directly -- changes to the
     * value should also be observed through the pref service. This method should only be used in
     * the context of settings UI.
     */
    public void fetchPriceEmailPref() {
        if (mNativeShoppingServiceAndroid == 0) return;

        ShoppingServiceJni.get().fetchPriceEmailPref(mNativeShoppingServiceAndroid, this);
    }

    /** Schedules updates for all products that the user has saved in the bookmarks system. */
    public void scheduleSavedProductUpdate() {
        if (mNativeShoppingServiceAndroid == 0) return;

        ShoppingServiceJni.get().scheduleSavedProductUpdate(mNativeShoppingServiceAndroid, this);
    }

    /** Create new subscriptions in batch. */
    public void subscribe(CommerceSubscription sub, Callback<Boolean> callback) {
        if (mNativeShoppingServiceAndroid == 0) return;

        assert sub.userSeenOffer != null;
        ShoppingServiceJni.get().subscribe(mNativeShoppingServiceAndroid, this, sub.type,
                sub.idType, sub.managementType, sub.id, sub.userSeenOffer.offerId,
                sub.userSeenOffer.userSeenPrice, sub.userSeenOffer.countryCode, callback);
    }

    /** Delete existing subscriptions in batch. */
    public void unsubscribe(CommerceSubscription sub, Callback<Boolean> callback) {
        if (mNativeShoppingServiceAndroid == 0) return;

        ShoppingServiceJni.get().unsubscribe(mNativeShoppingServiceAndroid, this, sub.type,
                sub.idType, sub.managementType, sub.id, callback);
    }

    /**
     * Check if a subscription exists.
     * @param sub The subscription details to check.
     * @param callback A callback executed when the state of the subscription is known.
     */
    public void isSubscribed(CommerceSubscription sub, Callback<Boolean> callback) {
        if (mNativeShoppingServiceAndroid == 0) return;

        ShoppingServiceJni.get().isSubscribed(mNativeShoppingServiceAndroid, this, sub.type,
                sub.idType, sub.managementType, sub.id, callback);
    }

    /**
     * Check if a subscription exists from cached information. Use of the the callback-based version
     * {@link #isSubscribed(CommerceSubscription, Callback)} is preferred.
     * @param sub The subscription details to check.
     * @return Whether the provided subscription is tracked by the user.
     */
    public boolean isSubscribedFromCache(CommerceSubscription sub) {
        if (mNativeShoppingServiceAndroid == 0) return false;

        return ShoppingServiceJni.get().isSubscribedFromCache(mNativeShoppingServiceAndroid, this,
                sub.type, sub.idType, sub.managementType, sub.id);
    }

    public void addSubscriptionsObserver(SubscriptionsObserver observer) {
        mSubscriptionsObservers.addObserver(observer);
    }

    public void removeSubscriptionsObserver(SubscriptionsObserver observer) {
        mSubscriptionsObservers.removeObserver(observer);
    }

    public void getAllPriceTrackedBookmarks(Callback<List<BookmarkId>> callback) {
        if (mNativeShoppingServiceAndroid == 0) {
            return;
        }
        ShoppingServiceJni.get().getAllPriceTrackedBookmarks(
                mNativeShoppingServiceAndroid, this, callback);
    }

    @CalledByNative
    private static void runGetAllPriceTrackedBookmarksCallback(
            Callback<List<BookmarkId>> callback, long[] trackedBookmarkIds) {
        ArrayList<BookmarkId> bookmarks = new ArrayList<>();
        for (int i = 0; i < trackedBookmarkIds.length; i++) {
            // All product bookmarks will have a "Normal" type.
            bookmarks.add(new BookmarkId(trackedBookmarkIds[i], BookmarkType.NORMAL));
        }
        callback.onResult(bookmarks);
    }

    /**
     * This is a feature check for the "shopping list". This will only return true if the user has
     * the feature flag enabled, is signed-in, has MSBB enabled, has webapp activity enabled, is
     * allowed by enterprise policy, and (if applicable) in an eligible country and locale. The
     * value returned by this method can change at runtime, so it should not be used when deciding
     * whether to create critical, feature-related infrastructure.
     *
     * @return Whether the user is eligible to use the shopping list feature.
     */
    public boolean isShoppingListEligible() {
        if (mNativeShoppingServiceAndroid == 0) return false;

        return ShoppingServiceJni.get().isShoppingListEligible(mNativeShoppingServiceAndroid, this);
    }

    // This is a feature check for the "merchant viewer", which will return true if the user has the
    // feature flag enabled or (if applicable) is in an eligible country and locale.
    public boolean isMerchantViewerEnabled() {
        if (mNativeShoppingServiceAndroid == 0) return false;

        return ShoppingServiceJni.get().isMerchantViewerEnabled(
                mNativeShoppingServiceAndroid, this);
    }

    // This is a feature check for the "price tracking", which will return true if the user has the
    // feature flag enabled or (if applicable) is in an eligible country and locale.
    public boolean isCommercePriceTrackingEnabled() {
        if (mNativeShoppingServiceAndroid == 0) return false;

        return ShoppingServiceJni.get().isCommercePriceTrackingEnabled(
                mNativeShoppingServiceAndroid, this);
    }

    @CalledByNative
    private void destroy() {
        mNativeShoppingServiceAndroid = 0;
        mSubscriptionsObservers.clear();
    }

    @CalledByNative
    private static ShoppingService create(long nativePtr) {
        return new ShoppingService(nativePtr);
    }

    @CalledByNative
    private static ProductInfo createProductInfo(String title, GURL imageUrl,
            boolean hasProductClusterId, long productClusterId, boolean hasOfferId, long offerId,
            String currencyCode, long amountMicros, String countryCode, boolean hasPreviousPrice,
            long previousAmountMicros) {
        Optional<Long> offer = !hasOfferId ? Optional.empty() : Optional.of(offerId);
        Optional<Long> cluster =
                !hasProductClusterId ? Optional.empty() : Optional.of(productClusterId);
        Optional<Long> previousPrice =
                !hasPreviousPrice ? Optional.empty() : Optional.of(previousAmountMicros);
        return new ProductInfo(title, imageUrl, cluster, offer, currencyCode, amountMicros,
                countryCode, previousPrice);
    }

    @CalledByNative
    private static void runProductInfoCallback(
            ProductInfoCallback callback, GURL url, ProductInfo info) {
        callback.onResult(url, info);
    }

    @CalledByNative
    private static MerchantInfo createMerchantInfo(float starRating, int countRating,
            GURL detailsPageUrl, boolean hasReturnPolicy, float nonPersonalizedFamilarityScore,
            boolean containsSensitiveContent, boolean proactiveMessageDisabled) {
        return new MerchantInfo(starRating, countRating, detailsPageUrl, hasReturnPolicy,
                nonPersonalizedFamilarityScore, containsSensitiveContent, proactiveMessageDisabled);
    }

    @CalledByNative
    private static void runMerchantInfoCallback(
            MerchantInfoCallback callback, GURL url, MerchantInfo info) {
        callback.onResult(url, info);
    }

    @CalledByNative
    private static CommerceSubscription createSubscription(
            int type, int idType, int managementType, String id) {
        return new CommerceSubscription(type, idType, id, managementType, null);
    }

    @CalledByNative
    private void onSubscribe(CommerceSubscription sub, boolean succeeded) {
        for (SubscriptionsObserver o : mSubscriptionsObservers) {
            o.onSubscribe(sub, succeeded);
        }
    }

    @CalledByNative
    private void onUnsubscribe(CommerceSubscription sub, boolean succeeded) {
        for (SubscriptionsObserver o : mSubscriptionsObservers) {
            o.onUnsubscribe(sub, succeeded);
        }
    }

    @NativeMethods
    interface Natives {
        void getProductInfoForUrl(long nativeShoppingServiceAndroid, ShoppingService caller,
                GURL url, ProductInfoCallback callback);
        ProductInfo getAvailableProductInfoForUrl(
                long nativeShoppingServiceAndroid, ShoppingService caller, GURL url);
        void getMerchantInfoForUrl(long nativeShoppingServiceAndroid, ShoppingService caller,
                GURL url, MerchantInfoCallback callback);
        void fetchPriceEmailPref(long nativeShoppingServiceAndroid, ShoppingService caller);
        void scheduleSavedProductUpdate(long nativeShoppingServiceAndroid, ShoppingService caller);
        void subscribe(long nativeShoppingServiceAndroid, ShoppingService caller, int type,
                int idType, int managementType, String id, String seenOfferId, long seenPrice,
                String seenCountry, Callback<Boolean> callback);
        void unsubscribe(long nativeShoppingServiceAndroid, ShoppingService caller, int type,
                int idType, int managementType, String id, Callback<Boolean> callback);
        void isSubscribed(long nativeShoppingServiceAndroid, ShoppingService caller, int type,
                int idType, int managementType, String id, Callback<Boolean> callback);
        boolean isSubscribedFromCache(long nativeShoppingServiceAndroid, ShoppingService caller,
                int type, int idType, int managementType, String id);
        void getAllPriceTrackedBookmarks(long nativeShoppingServiceAndroid, ShoppingService caller,
                Callback<List<BookmarkId>> callback);
        boolean isShoppingListEligible(long nativeShoppingServiceAndroid, ShoppingService caller);
        boolean isMerchantViewerEnabled(long nativeShoppingServiceAndroid, ShoppingService caller);
        boolean isCommercePriceTrackingEnabled(
                long nativeShoppingServiceAndroid, ShoppingService caller);
    }
}
