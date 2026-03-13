// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_ONE_P_RESOLVER_IMPL_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_ONE_P_RESOLVER_IMPL_H_

#include "components/accessibility_annotator/core/annotation_reducer/one_p_resolver.h"

namespace accessibility_annotator {

// Stub implementation of OnePResolver that returns an empty list.
class OnePResolverImpl : public OnePResolver {
 public:
  OnePResolverImpl();
  OnePResolverImpl(const OnePResolverImpl&) = delete;
  OnePResolverImpl& operator=(const OnePResolverImpl&) = delete;
  ~OnePResolverImpl() override;

  // OnePResolver:
  void Query(const std::u16string& query, QueryCallback callback) override;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_ONE_P_RESOLVER_IMPL_H_
