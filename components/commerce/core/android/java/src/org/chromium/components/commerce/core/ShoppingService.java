// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.commerce.core;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.url.GURL;

/** A central hub for accessing shopping and product infomration. */
@JNINamespace("commerce")
public class ShoppingService {
    /** A data container for product info provided by the shopping service. */
    public static final class ProductInfo {
        public final String title;
        public final GURL imageUrl;
        public final long productClusterId;
        public final long offerId;
        public final String currencyCode;
        public final long amountMicros;
        public final String countryCode;

        ProductInfo(String title, GURL imageUrl, long productClusterId, long offerId,
                String currencyCode, long amountMicros, String countryCode) {
            this.title = title;
            this.imageUrl = imageUrl;
            this.productClusterId = productClusterId;
            this.offerId = offerId;
            this.currencyCode = currencyCode;
            this.amountMicros = amountMicros;
            this.countryCode = countryCode;
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

    /** A pointer to the native side of the object. */
    private long mNativeShoppingServiceAndroid;

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

    @CalledByNative
    private void destroy() {
        mNativeShoppingServiceAndroid = 0;
    }

    @CalledByNative
    private static ShoppingService create(long nativePtr) {
        return new ShoppingService(nativePtr);
    }

    @CalledByNative
    private static ProductInfo createProductInfo(String title, GURL imageUrl, long productClusterId,
            long offerId, String currencyCode, long amountMicros, String countryCode) {
        return new ProductInfo(title, imageUrl, productClusterId, offerId, currencyCode,
                amountMicros, countryCode);
    }

    @CalledByNative
    private static void runProductInfoCallback(
            ProductInfoCallback callback, GURL url, ProductInfo info) {
        callback.onResult(url, info);
    }

    @NativeMethods
    interface Natives {
        void getProductInfoForUrl(long nativeShoppingServiceAndroid, ShoppingService caller,
                GURL url, ProductInfoCallback callback);
    }
}
