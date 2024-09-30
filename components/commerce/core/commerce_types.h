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
#include "base/tuple.h"
#include "components/commerce/core/proto/parcel.pb.h"
#include "components/commerce/core/proto/product_category.pb.h"
#include "url/gurl.h"

namespace commerce {

// Data containers that are provided by the above callbacks:

// Discount cluster types.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.commerce.core
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(DiscountClusterType)
enum class DiscountClusterType {
  kUnspecified = 0,
  kOfferLevel = 1,
  kPageLevel = 2,
  kMaxValue = kPageLevel,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml:DiscountClusterType)

// Discount types.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.commerce.core
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
  CategoryData category_data;

 private:
  friend class ShoppingService;

  // This is used to track whether the server provided an image with the rest
  // of the product info. This value being |true| does not necessarily mean an
  // image is available in the ProductInfo struct (as it is flag gated) and is
  // primarily used for recording metrics.
  bool server_image_available{false};
};

// Details about a particular URL.
struct UrlInfo {
  UrlInfo();
  UrlInfo(const GURL& url,
          const std::u16string& title,
          const std::optional<GURL> favicon_url = std::nullopt,
          const std::optional<GURL> thumbnail_url = std::nullopt);
  UrlInfo(const UrlInfo&);
  UrlInfo& operator=(const UrlInfo&);
  bool operator==(const UrlInfo& other) const {
    return url == other.url && title == other.title;
  }
  ~UrlInfo();

  GURL url;
  std::u16string title;
  std::optional<GURL> favicon_url;
  std::optional<GURL> thumbnail_url;
};

// Information provided by the product specifications backend.
struct ProductSpecifications {
 public:
  typedef uint64_t ProductDimensionId;

  ProductSpecifications();
  ProductSpecifications(const ProductSpecifications&);
  ~ProductSpecifications();

  // Text content with a URL for more context.
  struct DescriptionText {
   public:
    DescriptionText();
    DescriptionText(const DescriptionText&);
    ~DescriptionText();
    std::string text;
    std::vector<UrlInfo> urls;
  };

  struct Description {
   public:
    Description();
    Description(const Description&);
    ~Description();

    struct Option {
      Option();
      Option(const Option&);
      ~Option();

      // The primary descriptions to display for this option.
      std::vector<DescriptionText> descriptions;
    };

    // A list of options that apply to this description.
    std::vector<Option> options;

    // Optional label or title for the descriptions.
    std::string label;

    // Optional alternative text with additional context for the descriptions.
    std::string alt_text;
  };

  struct Value {
   public:
    Value();
    Value(const Value&);
    ~Value();

    std::vector<Description> descriptions;
    std::vector<DescriptionText> summary;
  };

  struct Product {
   public:
    Product();
    Product(const Product&);
    ~Product();

    uint64_t product_cluster_id;
    std::string mid;
    std::string title;
    GURL image_url;
    std::map<ProductDimensionId, Value> product_dimension_values;
    std::vector<DescriptionText> summary;
    GURL buying_options_url;
  };

  // A map of each product dimension ID to its human readable name.
  std::map<ProductDimensionId, std::string> product_dimension_map;

  // The list of products in the specification group.
  std::vector<Product> products;
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
  std::optional<base::Time> estimated_delivery_time;
};

// Class representing the tap strip entry point.
struct EntryPointInfo {
  EntryPointInfo(const std::string& title,
                 std::map<GURL, uint64_t> similar_candidate_products);
  ~EntryPointInfo();
  EntryPointInfo(const EntryPointInfo&);
  EntryPointInfo& operator=(const EntryPointInfo&);

  // Title of the product group to be clustered.
  std::string title;

  // Map of candidate products that are similar and can
  // be clustered into one product group. Key is the product URL and value is
  // the product cluster ID.
  std::map<GURL, uint64_t> similar_candidate_products;
};

// Callbacks and typedefs for various accessors in the shopping service.
using DiscountInfoCallback =
    base::OnceCallback<void(const GURL&, const std::vector<DiscountInfo>)>;
using MerchantInfoCallback =
    base::OnceCallback<void(const GURL&, std::optional<MerchantInfo>)>;
using PriceInsightsInfoCallback =
    base::OnceCallback<void(const GURL&,
                            const std::optional<PriceInsightsInfo>&)>;
using ProductInfoCallback =
    base::OnceCallback<void(const GURL&,
                            const std::optional<const ProductInfo>&)>;
using ProductSpecificationsCallback =
    base::OnceCallback<void(std::vector<uint64_t>,
                            std::optional<ProductSpecifications>)>;
using IsShoppingPageCallback =
    base::OnceCallback<void(const GURL&, std::optional<bool>)>;
using GetParcelStatusCallback = base::OnceCallback<
    void(bool /*success*/, std::unique_ptr<std::vector<ParcelTrackingStatus>>)>;
using StopParcelTrackingCallback = base::OnceCallback<void(bool /*success*/)>;
}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMMERCE_TYPES_H_
