// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_HANDLE_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_HANDLE_H_

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace user_education {

class FeaturePromoController;

// Represents a promo that has been continued after its bubble has been
// hidden, as a result of calling CloseBubbleAndContinuePromo().
//
// The promo is considered still active until the handle is released or
// destroyed and no other promos will be allowed to show.
//
// FeaturePromoHandle is a value-typed, movable smart reference; default
// constructed instances are falsy (i.e. operator bool and is_valid() return
// false), as are any instances that have been moved or released.
class [[nodiscard]] FeaturePromoHandle {
 public:
  FeaturePromoHandle();
  FeaturePromoHandle(base::WeakPtr<FeaturePromoController> controller,
                     const base::Feature* feature);
  FeaturePromoHandle(FeaturePromoHandle&&) noexcept;
  FeaturePromoHandle& operator=(FeaturePromoHandle&&) noexcept;
  ~FeaturePromoHandle();

  explicit operator bool() const { return is_valid(); }
  bool operator!() const { return !is_valid(); }

  // Returns whether the handle refers to a valid promo. Returns null for
  // default-constructed objects and after being moved or released.
  bool is_valid() const { return feature_; }

  // Releases the promo and resets the handle. After release, operator bool
  // will return false regardless of the previous state.
  void Release();

 private:
  base::WeakPtr<FeaturePromoController> controller_;
  raw_ptr<const base::Feature> feature_ = nullptr;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_HANDLE_H_
