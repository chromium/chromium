// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fuchsia_component_support/annotations_manager.h"

#include <lib/fpromise/promise.h>

#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/strings/string_number_conversions.h"

namespace fuchsia_component_support {

namespace {
using AnnotationKeySet =
    std::set<fuchsia::element::AnnotationKey, AnnotationKeyCompare>;
}  // namespace

fuchsia::element::Annotation MakeAnnotation(std::string_view key,
                                            std::string_view value) {
  fuchsia::element::Annotation result;
  result.key.namespace_ = "global";
  result.key.value = std::string(key);

  constexpr size_t kArbitraryTextBufferThreshold = 128;
  if (value.size() <= kArbitraryTextBufferThreshold) {
    result.value.set_text(std::string(value));
  } else {
    result.value.set_buffer(base::MemBufferFromString(value, /*name=*/key));
  }

  return result;
}

fuchsia::element::Annotation MakeBoolAnnotation(std::string_view key,
                                                bool value) {
  static constexpr std::string_view kTrue("true");
  static constexpr std::string_view kFalse("false");
  return MakeAnnotation(key, value ? kTrue : kFalse);
}

fuchsia::element::Annotation MakeIntAnnotation(std::string_view key,
                                               int value) {
  return MakeAnnotation(key, base::NumberToString(value));
}

bool AnnotationKeyCompare::operator()(
    const fuchsia::element::AnnotationKey& key1,
    const fuchsia::element::AnnotationKey& key2) const {
  return std::tie(key1.namespace_, key1.value) <
         std::tie(key2.namespace_, key2.value);
}

class AnnotationsManager::ControllerImpl
    : public fuchsia::element::AnnotationController {
 public:
  explicit ControllerImpl(AnnotationsManager& manager) : manager_(manager) {}
  ~ControllerImpl() override = default;

  // fuchsia::element::AnnotationController implementation.
  void UpdateAnnotations(
      std::vector<fuchsia::element::Annotation> annotations_to_set,
      std::vector<fuchsia::element::AnnotationKey> annotations_to_delete,
      UpdateAnnotationsCallback callback) override {
    DVLOG(2) << __func__;
    if (manager_->UpdateAnnotations(std::move(annotations_to_set),
                                    std::move(annotations_to_delete))) {
      callback(fpromise::ok());
    } else {
      callback(fpromise::error(
          fuchsia::element::UpdateAnnotationsError::INVALID_ARGS));
    }
  }
  void GetAnnotations(GetAnnotationsCallback callback) override {
    DVLOG(2) << __func__;
    callback(fpromise::ok(manager_->GetAnnotations()));
  }
  void WatchAnnotations(WatchAnnotationsCallback callback) override {
    DVLOG(2) << __func__;

    if (have_changes_) {
      have_changes_ = false;
      callback(fpromise::ok(manager_->GetAnnotations()));
      return;
    }

    if (on_annotations_changed_) {
      manager_->bindings_.CloseBinding(this, ZX_ERR_BAD_STATE);
      return;
    }

    on_annotations_changed_ = std::move(callback);
  }

  void OnAnnotationsChanged() {
    have_changes_ = true;
    if (on_annotations_changed_) {
      WatchAnnotations(std::exchange(on_annotations_changed_, {}));
    }
  }

 private:
  const raw_ref<AnnotationsManager> manager_;

  // Initially true so that the first call to WatchAnnotations() will
  // return results immediately.
  bool have_changes_ = true;

  WatchAnnotationsCallback on_annotations_changed_;
};

AnnotationsManager::AnnotationsManager() = default;

AnnotationsManager::~AnnotationsManager() = default;

bool AnnotationsManager::UpdateAnnotations(
    std::vector<fuchsia::element::Annotation> to_set,
    std::vector<fuchsia::element::AnnotationKey> to_delete) {
  AnnotationKeySet changed;
  for (const auto& key : to_delete) {
    annotations_.erase(key);
    auto [it, inserted] = changed.insert(key);
    if (!inserted) {
      return false;
    }
  }
  for (auto& annotation : to_set) {
    annotations_.insert_or_assign(
        fuchsia::element::AnnotationKey(annotation.key),
        std::move(annotation.value));
    auto [it, inserted] = changed.insert(annotation.key);
    if (!inserted) {
      return false;
    }
  }

  if (!changed.empty()) {
    for (auto& binding : bindings_.bindings()) {
      binding->impl()->OnAnnotationsChanged();
    }
  }

  return true;
}

std::vector<fuchsia::element::Annotation> AnnotationsManager::GetAnnotations()
    const {
  std::vector<fuchsia::element::Annotation> result;
  result.reserve(annotations_.size());
  for (const auto& [key, value] : annotations_) {
    result.push_back(fuchsia::element::Annotation{.key = key});
    zx_status_t status = value.Clone(&result.back().value);
    ZX_CHECK(status == ZX_OK, status);
  }
  return result;
}

void AnnotationsManager::Connect(
    fidl::InterfaceRequest<fuchsia::element::AnnotationController> request) {
  bindings_.AddBinding(std::make_unique<ControllerImpl>(*this),
                       std::move(request));
}

}  // namespace fuchsia_component_support
