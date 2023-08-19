// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/unique_ids.h"

#include "components/autofill/core/common/logging/log_buffer.h"

namespace autofill {

std::ostream& operator<<(std::ostream& os, const FormRendererId& form) {
  return os << form.value();
}

std::ostream& operator<<(std::ostream& os, const FieldRendererId& field) {
  return os << field.value();
}

std::ostream& operator<<(std::ostream& os, const FormGlobalId& form) {
  return os << form.frame_token.ToString() << "_" << form.renderer_id;
}

std::ostream& operator<<(std::ostream& os, const FieldGlobalId& field) {
  return os << field.frame_token.ToString() << "_" << field.renderer_id;
}

LogBuffer& operator<<(LogBuffer& buffer, const FormRendererId& form) {
  return buffer << form.value();
}

LogBuffer& operator<<(LogBuffer& buffer, const FieldRendererId& field) {
  return buffer << field.value();
}

LogBuffer& operator<<(LogBuffer& buffer, const FormGlobalId& form) {
  return buffer << form.frame_token.ToString() << "_" << form.renderer_id;
}

LogBuffer& operator<<(LogBuffer& buffer, const FieldGlobalId& field) {
  return buffer << field.frame_token.ToString() << "_" << field.renderer_id;
}

}  // namespace autofill
