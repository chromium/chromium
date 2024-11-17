// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_ANCHOR_ELEMENT_PROVIDER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_ANCHOR_ELEMENT_PROVIDER_H_

#include "base/functional/callback_forward.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"

namespace user_education {

// Abstract interface for an object that can locate and return a
// `ui::TrackedElement`.
class AnchorElementProvider {
 public:
  AnchorElementProvider() = default;
  virtual ~AnchorElementProvider() = default;

  // Returns the target element, using `default_context`.
  virtual ui::TrackedElement* GetAnchorElement(
      ui::ElementContext default_context) const = 0;
};

// Common implementation of AnchorElementProvider.
class AnchorElementProviderCommon : public AnchorElementProvider {
 public:
  AnchorElementProviderCommon();
  explicit AnchorElementProviderCommon(ui::ElementIdentifier anchor_element_id);
  AnchorElementProviderCommon(AnchorElementProviderCommon&&) noexcept;
  AnchorElementProviderCommon& operator=(
      AnchorElementProviderCommon&&) noexcept;
  ~AnchorElementProviderCommon() override;

  // Optional method that filters a set of potential `elements` to choose and
  // return the anchor element, or null if none of the inputs is appropriate.
  // This method can return an element different from the input list, or null
  // if no valid element is found.
  using AnchorElementFilter = base::RepeatingCallback<ui::TrackedElement*(
      const ui::ElementTracker::ElementList& elements)>;

  ui::ElementIdentifier anchor_element_id() const { return anchor_element_id_; }
  bool in_any_context() const { return in_any_context_; }
  const AnchorElementFilter& anchor_element_filter() const {
    return anchor_element_filter_;
  }

  // Get the anchor element based on `anchor_element_id`,
  // `anchor_element_filter`, and `context`.
  ui::TrackedElement* GetAnchorElement(
      ui::ElementContext context) const override;

 protected:
  void set_in_any_context(bool in_any_context) {
    in_any_context_ = in_any_context;
  }
  void set_anchor_element_filter(AnchorElementFilter anchor_element_filter) {
    anchor_element_filter_ = std::move(anchor_element_filter);
  }

 private:
  // The element identifier of the anchor element.
  ui::ElementIdentifier anchor_element_id_;

  // Whether we are allowed to search for the anchor element in any context.
  bool in_any_context_ = false;

  // The filter to use if there is more than one matching element, or
  // additional processing is needed (default is to always use the first
  // matching element).
  AnchorElementFilter anchor_element_filter_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_ANCHOR_ELEMENT_PROVIDER_H_
