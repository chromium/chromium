// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/proto_conversions.h"

#include "base/strings/utf_string_conversions.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"

namespace send_tab_to_self {

namespace {

sync_pb::FormField FormFieldToProto(const PageContext::FormField& field) {
  sync_pb::FormField pb_field;
  pb_field.set_id_attribute(base::UTF16ToUTF8(field.id_attribute));
  pb_field.set_name_attribute(base::UTF16ToUTF8(field.name_attribute));
  pb_field.set_label(base::UTF16ToUTF8(field.label));
  pb_field.set_form_control_type(field.form_control_type);
  pb_field.set_value(base::UTF16ToUTF8(field.value));
  return pb_field;
}

PageContext::FormField FormFieldFromProto(const sync_pb::FormField& pb_field) {
  PageContext::FormField field;
  field.id_attribute = base::UTF8ToUTF16(pb_field.id_attribute());
  field.name_attribute = base::UTF8ToUTF16(pb_field.name_attribute());
  field.label = base::UTF8ToUTF16(pb_field.label());
  field.form_control_type = pb_field.form_control_type();
  field.value = base::UTF8ToUTF16(pb_field.value());
  return field;
}

sync_pb::FormFieldInfo FormFieldInfoToProto(
    const PageContext::FormFieldInfo& info) {
  sync_pb::FormFieldInfo pb_info;
  for (const auto& field : info.fields) {
    *pb_info.add_fields() = FormFieldToProto(field);
  }
  return pb_info;
}

PageContext::FormFieldInfo FormFieldInfoFromProto(
    const sync_pb::FormFieldInfo& pb_info) {
  PageContext::FormFieldInfo info;
  for (const auto& pb_field : pb_info.fields()) {
    info.fields.push_back(FormFieldFromProto(pb_field));
  }
  return info;
}

}  // namespace

sync_pb::PageContext PageContextToProto(const PageContext& context) {
  sync_pb::PageContext pb_page_context;
  if (!context.form_field_info.fields.empty()) {
    *pb_page_context.mutable_form_field_info() =
        FormFieldInfoToProto(context.form_field_info);
  }
  return pb_page_context;
}

PageContext PageContextFromProto(const sync_pb::PageContext& pb_page_context) {
  PageContext page_context;
  page_context.form_field_info =
      FormFieldInfoFromProto(pb_page_context.form_field_info());
  return page_context;
}

}  // namespace send_tab_to_self
