// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_DATABINDING_VECTOR_ELEMENT_BINDING_H_
#define CHROME_BROWSER_VR_DATABINDING_VECTOR_ELEMENT_BINDING_H_

#include <sstream>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/vr/databinding/binding_base.h"

namespace vr {

// This class manages databinding for a vector of objects.
template <typename M, typename V>
class VectorElementBinding : public BindingBase {
 public:
  VectorElementBinding(std::vector<M>* models, size_t index)
      : models_(models), index_(index) {}
  ~VectorElementBinding() override {}

  // This function will check if the getter is producing a different value than
  // when it was last polled. If so, it will pass that value to the provided
  // setter.
  bool Update() override {
    bool updated = false;
    for (auto& binding : bindings_) {
      if (binding->Update())
        updated = true;
    }
    return updated;
  }

  M* model() { return &(*models_)[index_]; }

  V* view() const { return view_; }
  void set_view(V* view) { view_ = view; }

  std::vector<std::unique_ptr<BindingBase>>& bindings() { return bindings_; }

  std::string ToString() override {
    std::ostringstream os;
    for (auto& binding : bindings_) {
      os << std::endl << "  " << binding->ToString();
    }
    return os.str();
  }

 private:
  std::vector<M>* models_ = nullptr;
  size_t index_ = 0;
  V* view_;
  std::vector<std::unique_ptr<BindingBase>> bindings_;

  DISALLOW_COPY_AND_ASSIGN(VectorElementBinding);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_DATABINDING_VECTOR_ELEMENT_BINDING_H_
