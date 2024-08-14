// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SET_H_
#define COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SET_H_

#include <vector>

#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/commerce/core/commerce_types.h"
#include "components/sync/protocol/product_comparison_specifics.pb.h"
#include "url/gurl.h"

namespace {
class SingleClientProductSpecificationsSyncTest;
}  // namespace

namespace commerce {

class ProductSpecificationsService;
class ProductSpecificationsSyncBridge;
class ProductSpecificationsSyncBridgeTest;

// Contains a set of product specifications.
class ProductSpecificationsSet {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnProductSpecificationsSetAdded(
        const ProductSpecificationsSet& product_specifications_set) {}

    // Invoked when a ProductSpecificationsSet is updated and provides the
    // current and previous values.
    virtual void OnProductSpecificationsSetUpdate(
        const ProductSpecificationsSet& before,
        const ProductSpecificationsSet& after) {}

    // Invoked when the name of a ProductSpecificationSet is updated and
    // provides the current and previous values.
    virtual void OnProductSpecificationsSetNameUpdate(
        const std::string& before,
        const std::string& after) {}

    virtual void OnProductSpecificationsSetRemoved(
        const ProductSpecificationsSet& product_specifications_set) {}

   private:
    friend commerce::ProductSpecificationsSyncBridge;
  };

  ProductSpecificationsSet(const std::string& uuid,
                           const int64_t creation_time_usec_since_epoch,
                           const int64_t update_time_usec_since_epoch,
                           const std::vector<GURL>& urls,
                           const std::string& name);

  // Title support is being added which necessitates the ProductSpecifications
  // APIs using UrlInfo instead of GURL. These changes are being phased in
  // over several CLs, so the constructors with both std::vector<GURL> and
  // std::vector<UrlInfo> will be used transitionally with the constructor
  // which uses std::vector<GURL> being deprecated when title support is
  // complete.
  ProductSpecificationsSet(const std::string& uuid,
                           const int64_t creation_time_usec_since_epoch,
                           const int64_t update_time_usec_since_epoch,
                           const std::vector<UrlInfo>& url_info,
                           const std::string& name);

  ProductSpecificationsSet(const ProductSpecificationsSet&);
  ProductSpecificationsSet& operator=(const ProductSpecificationsSet&) = delete;

  ~ProductSpecificationsSet();

  // Unique identifier for the set
  const base::Uuid& uuid() const { return uuid_; }

  // Time the set was created
  const base::Time& creation_time() const { return creation_time_; }

  // Time the set was updated
  const base::Time& update_time() const { return update_time_; }

  // Product urls for each item in the set
  const std::vector<GURL> urls() const;

  const std::vector<UrlInfo> url_infos() const { return url_infos_; }

  // Name of the set
  const std::string& name() const { return name_; }

 private:
  friend commerce::ProductSpecificationsService;
  friend commerce::ProductSpecificationsSyncBridge;
  friend commerce::ProductSpecificationsSyncBridgeTest;
  friend ::SingleClientProductSpecificationsSyncTest;

  static ProductSpecificationsSet FromProto(
      const sync_pb::ProductComparisonSpecifics& product_comparison_specifics);

  sync_pb::ProductComparisonSpecifics ToProto() const;

  const base::Uuid uuid_;
  const base::Time creation_time_;
  base::Time update_time_;
  std::vector<UrlInfo> url_infos_;
  std::string name_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SET_H_
