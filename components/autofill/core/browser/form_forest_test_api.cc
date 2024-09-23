// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_forest_test_api.h"

#include <iomanip>

namespace autofill::internal {

void FormForestTestApi::ExpandForm(base::stack<FrameForm>& frontier,
                                   FrameForm frame_and_form) {
  for (const FrameTokenWithPredecessor& child :
       frame_and_form.form->child_frames()) {
    std::optional<LocalFrameToken> local_child =
        frame_and_form.frame->driver->Resolve(child.token);
    FrameData* child_frame;
    if (local_child && (child_frame = GetFrameData(*local_child))) {
      for (FormData& child_form : child_frame->child_forms) {
        frontier.push({raw_ref(*child_frame), raw_ref(child_form)});
      }
    }
  }
}

std::ostream& FormForestTestApi::PrintFrames(std::ostream& os) {
  os << "#Frames = " << frame_datas().size() << std::endl;
  for (const std::unique_ptr<FrameData>& frame : frame_datas()) {
    os << "Token = " << frame->frame_token.ToString() << ":" << std::endl;
    os << "Driver = " << frame->driver << std::endl;
    if (frame->driver) {
      os << "Parent = " << frame->driver->GetParent() << std::endl;
    }
    if (frame->parent_form) {
      os << "ParentForm = " << *frame->parent_form << ":" << std::endl;
    }
    os << "#Forms = " << frame->child_forms.size() << std::endl;
    for (const FormData& form : frame->child_forms) {
      os << "  Form = " << form.global_id() << ":" << std::endl;
      os << "  #Frames " << form.child_frames().size() << ":" << std::endl;
      for (const FrameTokenWithPredecessor& child : form.child_frames()) {
        os << "  ChildFrame = "
           << absl::visit([](auto x) { return x.ToString(); }, child.token)
           << " / " << child.predecessor << std::endl;
      }
    }
    os << std::endl;
  }
  return os;
}

std::ostream& FormForestTestApi::PrintForest(std::ostream& os) {
  base::stack<FrameForm> frontier;
  for (const std::unique_ptr<FrameData>& frame : frame_datas()) {
    DCHECK(frame);
    if (frame && !frame->parent_form) {
      for (FormData& form : frame->child_forms) {
        frontier.push({raw_ref(*frame), raw_ref(form)});
      }
    }
  }
  TraverseTrees(frontier, [this, &os](const FormData& form) mutable {
    int level = [this, &form] {
      LocalFrameToken frame = form.host_frame();
      for (int level = 0;; ++level) {
        const FrameData* frame_data = GetFrameData(frame);
        if (!frame_data || !frame_data->parent_form) {
          return level;
        }
        frame = frame_data->parent_form->frame_token;
      }
    }();
    PrintForm(os, form, level);
  });
  return os;
}

std::ostream& FormForestTestApi::PrintForm(std::ostream& os,
                                           const FormData& form,
                                           int level) {
  std::string prefix(2 * level, ' ');
  os << prefix << "Form " << *form.renderer_id() << " at " << form.host_frame()
     << " at " << form.full_url().DeprecatedGetOriginAsURL() << " with "
     << form.fields().size() << " fields" << std::endl;
  os << prefix << "Origin " << form.main_frame_origin().Serialize()
     << std::endl;
  if (!form.name().empty()) {
    os << prefix << "Name " << form.name() << std::endl;
  }
  int i = 0;
  const FrameData* frame = GetFrameData(form.host_frame());
  if (frame) {
    for (const FrameTokenWithPredecessor& child : form.child_frames()) {
      auto local_child = frame->driver->Resolve(child.token);
      os << prefix << std::setfill(' ') << std::setw(2) << ++i << ". Frame "
         << absl::visit([](auto x) { return x.ToString(); }, child.token)
         << " -> " << (local_child ? local_child->ToString() : "") << std::endl;
    }
  }
  i = 0;
  for (const FormFieldData& field : form.fields()) {
    os << prefix << std::setfill(' ') << std::setw(2) << ++i << ". Field "
       << *field.renderer_id() << " at " << field.host_frame() << " at "
       << field.origin().Serialize() << std::endl;
    if (!field.id_attribute().empty()) {
      os << prefix << "    ID " << field.id_attribute() << std::endl;
    }
    if (!field.name_attribute().empty()) {
      os << prefix << "    Name " << field.name_attribute() << std::endl;
    }
    if (!field.value().empty()) {
      os << prefix << "    Value " << field.value() << std::endl;
    }
    if (!field.label().empty()) {
      os << prefix << "    Label "
         << field.label().substr(0, field.label().find('\n')) << std::endl;
    }
  }
  return os;
}

}  // namespace autofill::internal
