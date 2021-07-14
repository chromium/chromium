// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_FORM_FOREST_TEST_API_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_FORM_FOREST_TEST_API_H_

#include "base/containers/stack.h"
#include "components/autofill/content/browser/form_forest.h"

namespace autofill {
namespace internal {

// Exposes some testing (and debugging) operations for FormForest.
class FormForestTestApi {
 public:
  using FrameData = FormForest::FrameData;
  using FrameForm = FormForest::FrameAndForm;

  static absl::optional<LocalFrameToken> Resolve(const FrameData& local,
                                                 FrameToken other) {
    return FormForest::Resolve(local, other);
  }

  explicit FormForestTestApi(FormForest* ff) : ff_(ff) { DCHECK(ff_); }

  FrameData* GetFrameData(LocalFrameToken frame) {
    return ff_->GetFrameData(frame);
  }

  FormData* GetFormData(const FormGlobalId& form,
                        FrameData* frame_data = nullptr) {
    return ff_->GetFormData(form, frame_data);
  }

  FrameForm GetRoot(FormGlobalId form) { return ff_->GetRoot(form); }

  FormForest& form_forest() { return *ff_; }

  base::flat_set<std::unique_ptr<FrameData>, FrameData::CompareByFrameToken>&
  frame_datas() {
    return ff_->frame_datas_;
  }

  // Prints debug information.
  std::ostream& PrintTree(std::ostream& os);
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

  // Non-null pointer to wrapped FormForest.
  FormForest* ff_;
};

template <typename UnaryFunction>
void FormForestTestApi::TraverseTrees(base::stack<FrameForm>& frontier,
                                      UnaryFunction fun) {
  while (!frontier.empty()) {
    FrameForm next = frontier.top();
    frontier.pop();
    DCHECK(next);
    if (!next)
      continue;
    fun(*next.form);
    ExpandForm(frontier, next);
  }
}

}  // namespace internal
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_FORM_FOREST_TEST_API_H_
