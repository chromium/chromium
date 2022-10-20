// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_DATABINDING_VECTOR_BINDING_H_
#define CHROME_BROWSER_VR_DATABINDING_VECTOR_BINDING_H_

#include <memory>
#include <sstream>
#include <vector>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/vr/databinding/binding_base.h"
#include "chrome/browser/vr/databinding/vector_element_binding.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace vr {

// This class manages databinding for a vector of objects.
template <typename M, typename V>
class VectorBinding : public BindingBase {
 public:
  using ElementBinding = VectorElementBinding<M, V>;
  using ModelAddedCallback = base::RepeatingCallback<void(ElementBinding*)>;
  using ModelRemovedCallback = base::RepeatingCallback<void(ElementBinding*)>;

  VectorBinding(std::vector<M>* models,
                const ModelAddedCallback& added_callback,
                const ModelRemovedCallback& removed_callback)
      : models_(models),
        added_callback_(added_callback),
        removed_callback_(removed_callback) {}

  VectorBinding(const VectorBinding&) = delete;
  VectorBinding& operator=(const VectorBinding&) = delete;

  ~VectorBinding() {}

  // The update function for this collection binding is responsible for two
  // things:
  //  1. Managing the collection of element bindings (in response to changes in
  //  the size of the observed collection).
  //  2. Updating said element bindings.
  bool Update() override {
    bool updated = false;
    size_t current_size = models_->size();
    if (!last_size_ || current_size != last_size_.value()) {
      updated = true;
      size_t last_size = last_size_ ? last_size_.value() : 0lu;
      for (size_t i = current_size; i < last_size; ++i) {
        removed_callback_.Run(bindings_[i].get());
      }
      bindings_.resize(current_size);
      for (size_t i = last_size; i < current_size; ++i) {
        bindings_[i] = std::make_unique<ElementBinding>(models_, i);
        added_callback_.Run(bindings_[i].get());
      }
      last_size_ = current_size;
    }
    for (auto& binding : bindings_) {
      if (binding->Update())
        updated = true;
    }
    return updated;
  }

  std::string ToString() override {
    std::ostringstream os;
    for (size_t i = 0; i < bindings_.size(); ++i) {
      auto& binding = bindings_[i];
      os << i << ": " << binding->ToString() << std::endl;
    }
    return os.str();
  }

 private:
  raw_ptr<std::vector<M>, DanglingUntriaged> models_ = nullptr;
  std::vector<std::unique_ptr<ElementBinding>> bindings_;
  absl::optional<size_t> last_size_;
  ModelAddedCallback added_callback_;
  ModelRemovedCallback removed_callback_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_DATABINDING_VECTOR_BINDING_H_
