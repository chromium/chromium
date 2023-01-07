// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/databinding/vector_binding.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/cxx20_erase.h"
#include "chrome/browser/vr/databinding/binding.h"
#include "chrome/browser/vr/databinding/vector_element_binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

namespace {

struct View;

class ViewRegistry {
 public:
  void AddView(std::unique_ptr<View> view) {
    views_.push_back(std::move(view));
  }
  void RemoveView(View* view) {
    base::EraseIf(views_, [view](const std::unique_ptr<View>& v) {
      return v.get() == view;
    });
  }

  const std::vector<std::unique_ptr<View>>& views() { return views_; }

 private:
  std::vector<std::unique_ptr<View>> views_;
};

struct View {
  int value;
};

struct Model {
  int value;
};

typedef VectorBinding<Model, View> TestVectorBinding;
typedef typename TestVectorBinding::ElementBinding ElemBinding;

int GetValue(ElemBinding* element) {
  return element->model()->value;
}

void SetValue(ElemBinding* element, const int& value) {
  element->view()->value = value;
}

void OnModelAdded(ViewRegistry* registry, ElemBinding* element) {
  std::unique_ptr<View> view = std::make_unique<View>();
  element->set_view(view.get());
  element->bindings().push_back(std::make_unique<Binding<int>>(
      VR_BIND_LAMBDA([](ElemBinding* e) { return GetValue(e); },
                     base::Unretained(element)),
      VR_BIND_LAMBDA(
          [](ElemBinding* e, const int& value) { SetValue(e, value); },
          base::Unretained(element))));
  registry->AddView(std::move(view));
}

void OnModelRemoved(ViewRegistry* registry, ElemBinding* binding) {
  registry->RemoveView(binding->view());
}

}  // namespace

TEST(VectorBinding, Basic) {
  std::vector<Model> models;
  ViewRegistry registry;

  TestVectorBinding::ModelAddedCallback added_callback =
      base::BindRepeating(&OnModelAdded, base::Unretained(&registry));

  TestVectorBinding::ModelRemovedCallback removed_callback =
      base::BindRepeating(&OnModelRemoved, base::Unretained(&registry));

  TestVectorBinding binding(&models, added_callback, removed_callback);

  models.push_back({42});
  EXPECT_EQ(0u, registry.views().size());

  binding.Update();
  EXPECT_EQ(1u, registry.views().size());
  EXPECT_EQ(42, registry.views().front()->value);

  models[0].value = 123;
  binding.Update();
  EXPECT_EQ(123, registry.views().front()->value);

  models.clear();
  models.push_back({246});
  // We have the same number of models as before, nevertheless, we should have
  // propagated the values for the models at each index of the vector, even if
  // they are new (since we reference elements by index). This means that we
  // fire neither the model added nor model removed callbacks. Nevertheless, the
  // per-element bindings must fire. This checks that this does, in fact,
  // happen.
  binding.Update();
  EXPECT_EQ(1lu, registry.views().size());
  EXPECT_EQ(246, registry.views().front()->value);

  models.clear();
  binding.Update();
  EXPECT_EQ(0lu, registry.views().size());
}

}  // namespace vr
