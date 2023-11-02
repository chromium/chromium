// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_JS_SNIPPETS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_JS_SNIPPETS_H_

#include <string>
#include <vector>

namespace autofill_assistant {

// A piece of JavaScript code that might by generated dynamically.
class JsSnippet {
 public:
  JsSnippet();
  ~JsSnippet();

  // Return the JavaScript snippet as a string
  std::string ToString() const;

  // Adds a line of JavaScript code to snippet.
  //
  // IMPORTANT: Only pass strings that originate from hardcoded strings to this
  // method.
  void AddLine(const std::string& line);

  // Adds a single line of Javascript code to snippet.
  //
  // The line can be built from multiple string pieces; they'll be concatenated
  // together.
  //
  // IMPORTANT: Only pass strings that originate from hardcoded strings to this
  // method.
  void AddLine(const std::vector<std::string>& line);

 private:
  std::vector<std::string> lines_;
};

// Append JavaScript code to |snippet| that checks if |element_var| is on top.
//
// The JavaScript snippet returns |on_top| if the element is on top,
// |not_on_top| if its center is covered by some other element, or |not_in_view|
// if the element is not in the viewport.
void AddReturnIfOnTop(JsSnippet* out,
                      const std::string& element_var,
                      const std::string& on_top,
                      const std::string& not_on_top,
                      const std::string& not_in_view);

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_JS_SNIPPETS_H_
