// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.commerce.core;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** A central hub for accessing shopping and product information. */
@JNINamespace("commerce")
@NullMarked
public class ShoppingService {
    /** A data container for product info provided by the shopping service. */
    public static final class ProductInfo {
        public final String title;
        public final GURL imageUrl;
        public final @Nullable Long productClusterId;
        public final @Nullable Long offerId;
        public final String currencyCode;
        public final long amountMicros;
        public final @Nullable Long previousAmountMicros;
        public final String countryCode;

        public ProductInfo(
                String title,
                GURL imageUrl,
                @Nullable Long productClusterId,
                @Nullable Long offerId,
                String currencyCode,
                long amountMicros,
                String countryCode,
                @Nullable Long previousAmountMicros) {
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
        public MerchantInfo(
                float starRating,
                int countRating,
                GURL detailsPageUrl,
                boolean hasReturnPolicy,
                float nonPersonalizedFamiliarityScore,
                boolean containsSensitiveContent,
                boolean proactiveMessageDisabled) {
            this.starRating = starRating;
            this.countRating = countRating;
            this.detailsPageUrl = detailsPageUrl;
            this.hasReturnPolicy = hasReturnPolicy;
            this.nonPersonalizedFamiliarityScore = nonPersonalizedFamiliarityScore;
            this.containsSensitiveContent = containsSensitiveContent;
            this.proactiveMessageDisabled = proactiveMessageDisabled;
        }
    }

    /** A price point consisting of a date and the price on it. */
    public static final class PricePoint {
        public final String date;
        public final long price;

        public PricePoint(String date, long price) {
            this.date = date;
            this.price = price;
        }
    }

    /** A data container for price insights info provided by the shopping service. */
    public static final class PriceInsightsInfo {
        public final @Nullable Long productClusterId;
        public final String currencyCode;
        public final @Nullable Long typicalLowPriceMicros;
        public final @Nullable Long typicalHighPriceMicros;
        public final @Nullable String catalogAttributes;
        public final List<PricePoint> catalogHistoryPrices;
        public final @Nullable GURL jackpotUrl;
        public final @PriceBucket int priceBucket;
        public final boolean hasMultipleCatalogs;

        public PriceInsightsInfo(
                @Nullable Long productClusterId,
                String currencyCode,
                @Nullable Long typicalLowPriceMicros,
                @Nullable Long typicalHighPriceMicros,
                @Nullable String catalogAttributes,
                List<PricePoint> catalogHistoryPrices,
                @Nullable GURL jackpotUrl,
                @PriceBucket int priceBucket,
                boolean hasMultipleCatalogs) {
            this.productClusterId = productClusterId;
            this.currencyCode = currencyCode;
            this.typicalLowPriceMicros = typicalLowPriceMicros;
            this.typicalHighPriceMicros = typicalHighPriceMicros;
            this.catalogAttributes = catalogAttributes;
            this.catalogHistoryPrices = catalogHistoryPrices;
            this.jackpotUrl = jackpotUrl;
            this.priceBucket = priceBucket;
            this.hasMultipleCatalogs = hasMultipleCatalogs;
        }
    }

    /** A callback for acquiring product information about a page. */
    public interface ProductInfoCallback {
        /**
         * A notification that fetching product information for the URL has completed.
         * @param url The URL the product info was fetched for.
         * @param info The product info for the URL or {@code null} if none is available.
         */
        void onResult(GURL url, @Nullable ProductInfo info);
    }

    /** A callback for acquiring merchant information about a page. */
    public interface MerchantInfoCallback {
        /**
         * A notification that fetching merchant information for the URL has completed.
         * @param url The URL the merchant info was fetched for.
         * @param info The merchant info for the URL or {@code null} if none is available.
         */
        void onResult(GURL url, @Nullable MerchantInfo info);
    }

    /** A callback for acquiring price insights information about a page. */
    public interface PriceInsightsInfoCallback {
        /**
         * A notification that fetching price insights information for the URL has completed.
         *
         * @param url The URL the price insights info was fetched for.
         * @param info The price insights info for the URL or {@code null} if none is available.
         */
        void onResult(GURL url, @Nullable PriceInsightsInfo info);
    }

    /** A callback for acquiring discounts information about a page. */
    public interface DiscountInfoCallback {
        /**
         * A notification that fetching discounts information for the URL has completed.
         *
         * @param url The URL the discounts info was fetched for.
         * @param info A list of available discounts for the URL or null if none is available.
         */
        void onResult(GURL url, @Nullable List<DiscountInfo> info);
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
        if (mNativeShoppingServiceAndroid == 0) {
            callback.onResult(url, null);
            return;
        }

        ShoppingServiceJni.get().getProductInfoForUrl(mNativeShoppingServiceAndroid, url, callback);
    }

    /**
     * Get the currently available product information for the specified URL. This method may return
     * {@code null} or partial data if the page has not yet been completely processed. This is less
     * reliable than {@link #getProductInfoForUrl(GURL, ProductInfoCallback)}.
     * @param url The URL to fetch product info for.
     */
    public @Nullable ProductInfo getAvailableProductInfoForUrl(GURL url) {
        if (mNativeShoppingServiceAndroid == 0) return null;

        return ShoppingServiceJni.get()
                .getAvailableProductInfoForUrl(mNativeShoppingServiceAndroid, url);
    }

    /**
     * Fetch information about a merchant for a URL.
     * @param url The URL to fetch merchant info for.
     * @param callback The callback that will run after the fetch is completed. The merchant info
     *                 object will be null if there is none available.
     */
    public void getMerchantInfoForUrl(GURL url, MerchantInfoCallback callback) {
        if (mNativeShoppingServiceAndroid == 0) {
            callback.onResult(url, null);
            return;
        }

        ShoppingServiceJni.get()
                .getMerchantInfoForUrl(mNativeShoppingServiceAndroid, url, callback);
    }

    /**
     * Fetch price insights information for a URL.
     *
     * @param url The URL to fetch price insights info for.
     * @param callback The callback that will run after the fetch is completed. The price insights
     *     info object will be null if there is none available.
     */
    public void getPriceInsightsInfoForUrl(GURL url, PriceInsightsInfoCallback callback) {
        if (mNativeShoppingServiceAndroid == 0) {
            callback.onResult(url, null);
            return;
        }

        ShoppingServiceJni.get()
                .getPriceInsightsInfoForUrl(mNativeShoppingServiceAndroid, url, callback);
    }

    /**
     * Fetch discounts information for a URL.
     *
     * @param url The URL to fetch price insights info for.
     * @param callback The callback that will run after the fetch is completed.
     */
    public void getDiscountInfoForUrl(GURL url, DiscountInfoCallback callback) {
        if (mNativeShoppingServiceAndroid == 0) {
            callback.onResult(url, null);
            return;
        }

        ShoppingServiceJni.get()
                .getDiscountInfoForUrl(mNativeShoppingServiceAndroid, url, callback);
    }

    /**
     * Fetch available discounts information for a URL.
     *
     * @param url The URL to fetch discounts info for.
     * @param callback The callback that will run after the fetch is completed.
     */
    public void getAvailableDiscountInfoForUrl(GURL url, DiscountInfoCallback callback) {
        if (mNativeShoppingServiceAndroid == 0) {
            callback.onResult(url, null);
            return;
        }

        ShoppingServiceJni.get()
                .getAvailableDiscountInfoForUrl(mNativeShoppingServiceAndroid, url, callback);
    }

    /**
     * Requests that the service fetch the price notification email preference from the backend.
     * This call will update the preference kept by the pref service directly -- changes to the
     * value should also be observed through the pref service. This method should only be used in
     * the context of settings UI.
     */
    public void fetchPriceEmailPref() {
        if (mNativeShoppingServiceAndroid == 0) return;

        ShoppingServiceJni.get().fetchPriceEmailPref(mNativeShoppingServiceAndroid);
    }

    /** Schedules updates for all products that the user has saved in the bookmarks system. */
    public void scheduleSavedProductUpdate() {
        if (mNativeShoppingServiceAndroid == 0) return;

        ShoppingServiceJni.get().scheduleSavedProductUpdate(mNativeShoppingServiceAndroid);
    }

    /** Create new subscriptions in batch. */
    public void subscribe(CommerceSubscription sub, Callback<Boolean> callback) {
        if (mNativeShoppingServiceAndroid == 0) {
            callback.onResult(false);
            return;
        }

        assert sub.userSeenOffer != null;
        ShoppingServiceJni.get()
                .subscribe(
                        mNativeShoppingServiceAndroid,
                        sub.type,
                        sub.idType,
                        sub.managementType,
                        sub.id,
                        sub.userSeenOffer.offerId,
                        sub.userSeenOffer.userSeenPrice,
                        sub.userSeenOffer.countryCode,
                        sub.userSeenOffer.locale,
                        callback);
    }

    /** Delete existing subscriptions in batch. */
    public void unsubscribe(CommerceSubscription sub, Callback<Boolean> callback) {
        if (mNativeShoppingServiceAndroid == 0) {
            callback.onResult(false);
            return;
        }

        ShoppingServiceJni.get()
                .unsubscribe(
                        mNativeShoppingServiceAndroid,
                        sub.type,
                        sub.idType,
                        sub.managementType,
                        sub.id,
                        callback);
    }

    /**
     * Check if a subscription exists.
     * @param sub The subscription details to check.
     * @param callback A callback executed when the state of the subscription is known.
     */
    public void isSubscribed(CommerceSubscription sub, Callback<Boolean> callback) {
        if (mNativeShoppingServiceAndroid == 0) {
            callback.onResult(false);
            return;
        }

        ShoppingServiceJni.get()
                .isSubscribed(
                        mNativeShoppingServiceAndroid,
                        sub.type,
                        sub.idType,
                        sub.managementType,
                        sub.id,
                        callback);
    }

    /**
     * Check if a subscription exists from cached information. Use of the the callback-based version
     * {@link #isSubscribed(CommerceSubscription, Callback)} is preferred.
     * @param sub The subscription details to check.
     * @return Whether the provided subscription is tracked by the user.
     */
    public boolean isSubscribedFromCache(CommerceSubscription sub) {
        if (mNativeShoppingServiceAndroid == 0) return false;

        return ShoppingServiceJni.get()
                .isSubscribedFromCache(
                        mNativeShoppingServiceAndroid,
                        sub.type,
                        sub.idType,
                        sub.managementType,
                        sub.id);
    }

    public void addSubscriptionsObserver(SubscriptionsObserver observer) {
        mSubscriptionsObservers.addObserver(observer);
    }

    public void removeSubscriptionsObserver(SubscriptionsObserver observer) {
        mSubscriptionsObservers.removeObserver(observer);
    }

    public void getAllPriceTrackedBookmarks(Callback<List<BookmarkId>> callback) {
        if (mNativeShoppingServiceAndroid == 0) {
            callback.onResult(new ArrayList<>());
            return;
        }
        ShoppingServiceJni.get()
                .getAllPriceTrackedBookmarks(mNativeShoppingServiceAndroid, callback);
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

    // This is a feature check for the "merchant viewer", which will return true if the user has the
    // feature flag enabled or (if applicable) is in an eligible country and locale.
    public boolean isMerchantViewerEnabled() {
        if (mNativeShoppingServiceAndroid == 0) return false;

        return ShoppingServiceJni.get().isMerchantViewerEnabled(mNativeShoppingServiceAndroid);
    }

    // This is a feature check for the "price insights", which will return true
    // if the user has the feature flag enabled, has MSBB enabled, and (if
    // applicable) is in an eligible country and locale.
    public boolean isPriceInsightsEligible() {
        if (mNativeShoppingServiceAndroid == 0) return false;

        return ShoppingServiceJni.get().isPriceInsightsEligible(mNativeShoppingServiceAndroid);
    }

    // This is a feature check for the "discounts on navigation", which will return true
    // if the user has the feature flag enabled, has MSBB enabled, and (if
    // applicable) is in an eligible country and locale.
    public boolean isDiscountEligibleToShowOnNavigation() {
        if (mNativeShoppingServiceAndroid == 0) return false;

        return ShoppingServiceJni.get()
                .isDiscountEligibleToShowOnNavigation(mNativeShoppingServiceAndroid);
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
    private static ProductInfo createProductInfo(
            String title,
            GURL imageUrl,
            boolean hasProductClusterId,
            long productClusterId,
            boolean hasOfferId,
            long offerId,
            String currencyCode,
            long amountMicros,
            String countryCode,
            boolean hasPreviousPrice,
            long previousAmountMicros) {
        Long offer = !hasOfferId ? null : offerId;
        Long cluster = !hasProductClusterId ? null : productClusterId;
        Long previousPrice = !hasPreviousPrice ? null : previousAmountMicros;
        return new ProductInfo(
                title,
                imageUrl,
                cluster,
                offer,
                currencyCode,
                amountMicros,
                countryCode,
                previousPrice);
    }

    @CalledByNative
    private static void runProductInfoCallback(
            ProductInfoCallback callback, GURL url, ProductInfo info) {
        callback.onResult(url, info);
    }

    @CalledByNative
    private static MerchantInfo createMerchantInfo(
            float starRating,
            int countRating,
            GURL detailsPageUrl,
            boolean hasReturnPolicy,
            float nonPersonalizedFamilarityScore,
            boolean containsSensitiveContent,
            boolean proactiveMessageDisabled) {
        return new MerchantInfo(
                starRating,
                countRating,
                detailsPageUrl,
                hasReturnPolicy,
                nonPersonalizedFamilarityScore,
                containsSensitiveContent,
                proactiveMessageDisabled);
    }

    @CalledByNative
    private static void runMerchantInfoCallback(
            MerchantInfoCallback callback, GURL url, MerchantInfo info) {
        callback.onResult(url, info);
    }

    @CalledByNative
    private static List<PricePoint> createPricePointAndAddToList(
            List<PricePoint> points, String date, long price) {
        if (points == null) {
            points = new ArrayList<>();
        }
        PricePoint point = new PricePoint(date, price);
        points.add(point);
        return points;
    }

    @CalledByNative
    private static PriceInsightsInfo createPriceInsightsInfo(
            boolean hasProductClusterId,
            long productClusterId,
            String currencyCode,
            boolean hasTypicalLowPrice,
            long typicalLowPriceMicros,
            boolean hasTypicalHighPrice,
            long typicalHighPriceMicros,
            boolean hasCatalogAttributes,
            String catalogAttributes,
            List<PricePoint> catalogHistoryPrices,
            boolean hasJackpotUrl,
            GURL jackpotUrl,
            int priceBucket,
            boolean hasMultipleCatalogs) {
        Long clusterId = hasProductClusterId ? productClusterId : null;
        Long lowPrice = hasTypicalLowPrice ? typicalLowPriceMicros : null;
        Long highPrice = hasTypicalHighPrice ? typicalHighPriceMicros : null;
        String attributes = hasCatalogAttributes ? catalogAttributes : null;
        GURL jackpot = hasJackpotUrl ? jackpotUrl : null;

        if (catalogHistoryPrices == null) {
            catalogHistoryPrices = new ArrayList<>();
        }

        return new PriceInsightsInfo(
                clusterId,
                currencyCode,
                lowPrice,
                highPrice,
                attributes,
                catalogHistoryPrices,
                jackpot,
                priceBucket,
                hasMultipleCatalogs);
    }

    @CalledByNative
    private static void runPriceInsightsInfoCallback(
            PriceInsightsInfoCallback callback, GURL url, PriceInsightsInfo info) {
        callback.onResult(url, info);
    }

    @CalledByNative
    private static void runDiscountInfoCallback(
            DiscountInfoCallback callback, GURL url, DiscountInfo[] infos) {
        List<DiscountInfo> list = infos == null ? null : List.of(infos);
        callback.onResult(url, list);
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

    long getNativePtr() {
        return mNativeShoppingServiceAndroid;
    }

    @NativeMethods
    interface Natives {
        void getProductInfoForUrl(
                long nativeShoppingServiceAndroid, GURL url, ProductInfoCallback callback);

        ProductInfo getAvailableProductInfoForUrl(long nativeShoppingServiceAndroid, GURL url);

        void getMerchantInfoForUrl(
                long nativeShoppingServiceAndroid, GURL url, MerchantInfoCallback callback);

        void fetchPriceEmailPref(long nativeShoppingServiceAndroid);

        void scheduleSavedProductUpdate(long nativeShoppingServiceAndroid);

        void subscribe(
                long nativeShoppingServiceAndroid,
                int type,
                int idType,
                int managementType,
                String id,
                String seenOfferId,
                long seenPrice,
                String seenCountry,
                String seenLocale,
                Callback<Boolean> callback);

        void unsubscribe(
                long nativeShoppingServiceAndroid,
                int type,
                int idType,
                int managementType,
                String id,
                Callback<Boolean> callback);

        void isSubscribed(
                long nativeShoppingServiceAndroid,
                int type,
                int idType,
                int managementType,
                String id,
                Callback<Boolean> callback);

        boolean isSubscribedFromCache(
                long nativeShoppingServiceAndroid,
                int type,
                int idType,
                int managementType,
                String id);

        void getAllPriceTrackedBookmarks(
                long nativeShoppingServiceAndroid, Callback<List<BookmarkId>> callback);

        boolean isShoppingListEligible(long nativeShoppingServiceAndroid);

        boolean isMerchantViewerEnabled(long nativeShoppingServiceAndroid);

        void getPriceInsightsInfoForUrl(
                long nativeShoppingServiceAndroid, GURL url, PriceInsightsInfoCallback callback);

        boolean isPriceInsightsEligible(long nativeShoppingServiceAndroid);

        void getDiscountInfoForUrl(
                long nativeShoppingServiceAndroid, GURL url, DiscountInfoCallback callback);

        void getAvailableDiscountInfoForUrl(
                long nativeShoppingServiceAndroid, GURL url, DiscountInfoCallback callback);

        boolean isDiscountEligibleToShowOnNavigation(long nativeShoppingServiceAndroid);
    }
}
