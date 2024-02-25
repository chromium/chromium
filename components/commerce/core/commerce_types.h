// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMMERCE_TYPES_H_
#define COMPONENTS_COMMERCE_CORE_COMMERCE_TYPES_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/commerce/core/proto/parcel.pb.h"
#include "url/gurl.h"

namespace commerce {

// Data containers that are provided by the above callbacks:

// Discount cluster types.
enum class DiscountClusterType {
  kUnspecified = 0,
  kOfferLevel = 1,
  kMaxValue = kOfferLevel,
};

// Discount types.
enum class DiscountType {
  kUnspecified = 0,
  kFreeListingWithCode = 1,
  kMaxValue = kFreeListingWithCode,
};

// Information returned by the discount APIs.
struct DiscountInfo {
  DiscountInfo();
  DiscountInfo(const DiscountInfo&);
  DiscountInfo& operator=(const DiscountInfo&);
  ~DiscountInfo();

  DiscountClusterType cluster_type = DiscountClusterType::kUnspecified;
  DiscountType type = DiscountType::kUnspecified;
  std::string language_code;
  std::string description_detail;
  std::optional<std::string> terms_and_conditions;
  std::string value_in_text;
  std::optional<std::string> discount_code;
  uint64_t id = 0;
  bool is_merchant_wide = false;
  double expiry_time_sec = 0;
  uint64_t offer_id = 0;
};

// Information returned by the merchant info APIs.
struct MerchantInfo {
  MerchantInfo();
  MerchantInfo(const MerchantInfo&);
  MerchantInfo& operator=(const MerchantInfo&);
  MerchantInfo(MerchantInfo&&);
  MerchantInfo& operator=(MerchantInfo&&) = default;
  ~MerchantInfo();

  float star_rating = 0;
  uint32_t count_rating = 0;
  GURL details_page_url;
  bool has_return_policy = false;
  float non_personalized_familiarity_score = 0;
  bool contains_sensitive_content = false;
  bool proactive_message_disabled = false;
};

// Position of current price with respect to the typical price range.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.commerce.core
enum class PriceBucket {
  kUnknown = 0,
  kLowPrice = 1,
  kTypicalPrice = 2,
  kHighPrice = 3,
  kMaxValue = kHighPrice,
};

// Information returned by the price insights APIs.
struct PriceInsightsInfo {
  PriceInsightsInfo();
  PriceInsightsInfo(const PriceInsightsInfo&);
  PriceInsightsInfo& operator=(const PriceInsightsInfo&);
  ~PriceInsightsInfo();

  std::optional<uint64_t> product_cluster_id;
  std::string currency_code;
  std::optional<int64_t> typical_low_price_micros;
  std::optional<int64_t> typical_high_price_micros;
  std::optional<std::string> catalog_attributes;
  std::vector<std::tuple<std::string, int64_t>> catalog_history_prices;
  std::optional<GURL> jackpot_url;
  PriceBucket price_bucket = PriceBucket::kUnknown;
  bool has_multiple_catalogs = false;
};

// Information returned by the product info APIs.
struct ProductInfo {
 public:
  ProductInfo();
  ProductInfo(const ProductInfo&);
  ProductInfo& operator=(const ProductInfo&);
  ~ProductInfo();

  std::string title;
  std::string product_cluster_title;
  GURL image_url;
  std::optional<uint64_t> product_cluster_id;
  std::optional<uint64_t> offer_id;
  std::string currency_code;
  int64_t amount_micros{0};
  std::optional<int64_t> previous_amount_micros;
  std::string country_code;

 private:
  friend class ShoppingService;

  // This is used to track whether the server provided an image with the rest
  // of the product info. This value being |true| does not necessarily mean an
  // image is available in the ProductInfo struct (as it is flag gated) and is
  // primarily used for recording metrics.
  bool server_image_available{false};
};

// Information returned by Parcels API.
struct ParcelTrackingStatus {
 public:
  ParcelTrackingStatus();
  explicit ParcelTrackingStatus(const ParcelStatus&);
  ParcelTrackingStatus(const ParcelTrackingStatus&);
  ParcelTrackingStatus& operator=(const ParcelTrackingStatus&);
  ~ParcelTrackingStatus();

  ParcelIdentifier::Carrier carrier = ParcelIdentifier::UNKNOWN;
  std::string tracking_id;
  ParcelStatus::ParcelState state = ParcelStatus::UNKNOWN;
  GURL tracking_url;
  base::Time estimated_delivery_time;
};

// Callbacks and typedefs for various accessors in the shopping service.
using DiscountsMap = std::map<GURL, std::vector<DiscountInfo>>;
using DiscountInfoCallback = base::OnceCallback<void(const DiscountsMap&)>;
using MerchantInfoCallback =
    base::OnceCallback<void(const GURL&, std::optional<MerchantInfo>)>;
using PriceInsightsInfoCallback =
    base::OnceCallback<void(const GURL&,
                            const std::optional<PriceInsightsInfo>&)>;
using ProductInfoCallback =
    base::OnceCallback<void(const GURL&,
                            const std::optional<const ProductInfo>&)>;
using IsShoppingPageCallback =
    base::OnceCallback<void(const GURL&, std::optional<bool>)>;
using GetParcelStatusCallback = base::OnceCallback<
    void(bool /*success*/, std::unique_ptr<std::vector<ParcelTrackingStatus>>)>;
using StopParcelTrackingCallback = base::OnceCallback<void(bool /*success*/)>;
}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMMERCE_TYPES_H_
