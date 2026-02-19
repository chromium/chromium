// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_PAGE_CONTEXT_H_
#define COMPONENTS_SEND_TAB_TO_SELF_PAGE_CONTEXT_H_

#include <string>
#include <vector>

namespace send_tab_to_self {

struct PageContext {
  // Represents a single form field and its value.
  struct FormField {
    FormField();
    FormField(const FormField&);
    FormField(FormField&&);
    FormField& operator=(const FormField&);
    FormField& operator=(FormField&&);
    ~FormField();

    std::u16string id_attribute;
    std::u16string name_attribute;
    std::u16string label;
    std::string form_control_type;
    std::u16string value;
  };

  // Contains information about form fields in a page.
  struct FormFieldInfo {
    FormFieldInfo();
    FormFieldInfo(const FormFieldInfo&);
    FormFieldInfo(FormFieldInfo&&);
    FormFieldInfo& operator=(const FormFieldInfo&);
    FormFieldInfo& operator=(FormFieldInfo&&);
    ~FormFieldInfo();

    std::vector<FormField> fields;
  };

  PageContext();
  PageContext(const PageContext&);
  PageContext(PageContext&&);
  PageContext& operator=(const PageContext&);
  PageContext& operator=(PageContext&&);
  ~PageContext();

  FormFieldInfo form_field_info;
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_PAGE_CONTEXT_H_
