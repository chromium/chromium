// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_INPUT_METHOD_SUGGESTION_DETAILS_H_
#define CHROME_BROWSER_UI_ASH_INPUT_METHOD_SUGGESTION_DETAILS_H_

#include <string>

namespace ui {
namespace ime {

struct SuggestionDetails {
  std::u16string text;
  size_t confirmed_length = 0;
  bool show_accept_annotation = false;
  bool show_quick_accept_annotation = false;
  bool show_setting_link = false;

  bool operator==(const SuggestionDetails& other) const {
    return text == other.text && confirmed_length == other.confirmed_length &&
           show_accept_annotation == other.show_accept_annotation &&
           show_quick_accept_annotation == other.show_quick_accept_annotation &&
           show_setting_link == other.show_setting_link;
  }
};

}  // namespace ime
}  // namespace ui

#endif  // CHROME_BROWSER_UI_ASH_INPUT_METHOD_SUGGESTION_DETAILS_H_
