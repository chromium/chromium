// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_ANNOTATIONS_MANAGER_H_
#define COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_ANNOTATIONS_MANAGER_H_

#include <fuchsia/element/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>

#include <memory>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"

namespace fuchsia_component_support {

// Constructs an Annotation with the specified `value` set, either directly
// as `text`, or in a VMO `buffer`, depending on its size.
fuchsia::element::Annotation MakeAnnotation(std::string_view key,
                                            std::string_view value);

// Helper functions for constructing numeric and boolean annotations.
fuchsia::element::Annotation MakeBoolAnnotation(std::string_view key,
                                                bool value);
fuchsia::element::Annotation MakeIntAnnotation(std::string_view key, int value);

struct AnnotationKeyCompare {
  bool operator()(const fuchsia::element::AnnotationKey& key1,
                  const fuchsia::element::AnnotationKey& key2) const;
};

// Manages annotations associated with a session Element, or View.
class AnnotationsManager {
 public:
  AnnotationsManager();
  ~AnnotationsManager();

  // Sets, updates, or deletes one or more Annotations.
  // Returns false if the supplied inputs are invalid according to the
  // `fuchsia.element.AnnotationController.UpdateAnnotations()` API
  // specification, e.g. if `to_update` contains duplicate entries.
  bool UpdateAnnotations(
      std::vector<fuchsia::element::Annotation> to_set,
      std::vector<fuchsia::element::AnnotationKey> to_delete = {});

  // Returns a copy of the current set of Annotations.
  std::vector<fuchsia::element::Annotation> GetAnnotations() const;

  // Connects a new `AnnotationController` client to this manager.
  void Connect(
      fidl::InterfaceRequest<fuchsia::element::AnnotationController> request);

 private:
  class ControllerImpl;

  // Holds all currently-active annotations.
  base::flat_map<fuchsia::element::AnnotationKey,
                 fuchsia::element::AnnotationValue,
                 AnnotationKeyCompare>
      annotations_;

  // Bindings to active clients of this instance.
  fidl::BindingSet<fuchsia::element::AnnotationController,
                   std::unique_ptr<ControllerImpl>>
      bindings_;
};

}  // namespace fuchsia_component_support

#endif  // COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_ANNOTATIONS_MANAGER_H_
