// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_FOREST_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_FOREST_TEST_API_H_

#include "base/containers/stack.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/form_forest.h"

namespace autofill::internal {

// Exposes some testing (and debugging) operations for FormForest.
class FormForestTestApi {
 public:
  using FrameData = FormForest::FrameData;
  using FrameForm = FormForest::FrameAndForm;

  explicit FormForestTestApi(FormForest* ff) : ff_(*ff) {}

  void Reset() { ff_->frame_datas_.clear(); }

  FrameData* GetFrameData(LocalFrameToken frame) {
    return ff_->GetFrameData(frame);
  }

  FormData* GetFormData(const FormGlobalId& form,
                        FrameData* frame_data = nullptr) {
    return ff_->GetFormData(form, frame_data);
  }

  FormForest& form_forest() { return *ff_; }

  base::flat_set<std::unique_ptr<FrameData>, FrameData::CompareByFrameToken>&
  frame_datas() {
    return ff_->frame_datas_;
  }

  // Prints all frames, in particular their frame token and child frame tokens.
  // Intended for validating properties of the frame/form graph like acyclicity.
  std::ostream& PrintFrames(std::ostream& os);

  // Prints all trees of the forest by calling PrintForm() form each form. The
  // parent/child relation is represented by indenting descendant nodes.
  // Intended for validating that the frame/form trees match the DOMs.
  std::ostream& PrintForest(std::ostream& os);

  // Prints an individual form, in particular all identifiers of the form and
  // its fields.
  std::ostream& PrintForm(std::ostream& os,
                          const FormData& form,
                          int level = 0);

  // Calls `fun(f)` for every form in the subtrees induced by |frontier| in DOM
  // order (pre-order, depth-first).
  template <typename UnaryFunction>
  void TraverseTrees(base::stack<FrameForm>& frontier, UnaryFunction fun = {});

 private:
  // Adds the frame and form children for `frame_and_form.form` to |frontier|.
  void ExpandForm(base::stack<FrameForm>& frontier, FrameForm frame_and_form);

  const raw_ref<FormForest> ff_;
};

template <typename UnaryFunction>
void FormForestTestApi::TraverseTrees(base::stack<FrameForm>& frontier,
                                      UnaryFunction fun) {
  while (!frontier.empty()) {
    FrameForm next = frontier.top();
    frontier.pop();
    fun(*next.form);
    ExpandForm(frontier, next);
  }
}

inline FormForestTestApi test_api(FormForest& form_forest) {
  return FormForestTestApi(&form_forest);
}

}  // namespace autofill::internal

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_FOREST_TEST_API_H_
