// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_PAGE_CONTEXT_H_
#define COMPONENTS_SEND_TAB_TO_SELF_PAGE_CONTEXT_H_

#include <string>
#include <vector>

namespace send_tab_to_self {

// Text fragment data used for scroll position restoration.
struct TextFragmentData {
  TextFragmentData();
  TextFragmentData(std::string text_start,
                   std::string text_end,
                   std::string prefix,
                   std::string suffix);
  TextFragmentData(const TextFragmentData& other);
  TextFragmentData(TextFragmentData&& other);
  TextFragmentData& operator=(const TextFragmentData& other);
  TextFragmentData& operator=(TextFragmentData&& other);
  ~TextFragmentData();

  bool IsEmpty() const;

  // The exact text to match.
  // Corresponds to the 'textStart' parameter in the Text Fragment
  // specification.
  std::string text_start;

  // Optional: The end text for a range selection.
  // Corresponds to 'textEnd'.
  std::string text_end;

  // Optional: Prefix to ensure uniqueness of the match.
  // Corresponds to 'prefix'.
  std::string prefix;

  // Optional: Suffix to ensure uniqueness of the match.
  // Corresponds to 'suffix'.
  std::string suffix;

  bool operator==(const TextFragmentData& other) const;
};

// Information about the scroll position on the page.
struct ScrollPosition {
  ScrollPosition();
  ScrollPosition(const ScrollPosition&);
  ScrollPosition(ScrollPosition&&);
  ScrollPosition& operator=(const ScrollPosition&);
  ScrollPosition& operator=(ScrollPosition&&);
  ~ScrollPosition();

  TextFragmentData text_fragment;

  bool IsEmpty() const;
  bool operator==(const ScrollPosition& other) const;
};

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
    std::string form_control_type;
    std::u16string value;

    bool operator==(const FormField& other) const;
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

    bool operator==(const FormFieldInfo& other) const;
  };

  PageContext();
  PageContext(const PageContext&);
  PageContext(PageContext&&);
  PageContext& operator=(const PageContext&);
  PageContext& operator=(PageContext&&);
  ~PageContext();

  FormFieldInfo form_field_info;
  ScrollPosition scroll_position;

  bool operator==(const PageContext& other) const;
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_PAGE_CONTEXT_H_
