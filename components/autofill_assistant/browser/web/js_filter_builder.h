// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_JS_FILTER_BUILDER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_JS_FILTER_BUILDER_H_

#include <memory>
#include <string>

#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/js_snippets.h"

namespace autofill_assistant {
// Helper for building JavaScript functions.
//
// TODO(b/213859457): add tests
class JsFilterBuilder {
 public:
  JsFilterBuilder();
  ~JsFilterBuilder();

  // Builds the argument list for the function.
  std::vector<std::unique_ptr<runtime::CallArgument>> BuildArgumentList() const;
  std::unique_ptr<base::Value> BuildArgumentArray() const;

  // Return the JavaScript function.
  std::string BuildFunction() const;
  // Adds a filter, if possible.
  bool AddFilter(const SelectorProto::Filter& filter);
  void ClearResultsIfMoreThanOneResult();

 private:
  std::vector<std::string> arguments_;
  JsSnippet snippet_;
  bool defined_query_all_deduplicated_ = false;

  // A number that's increased by each call to DeclareVariable() to make sure
  // we generate unique variables.
  int variable_counter_ = 0;

  // Adds a regexp filter.
  void AddRegexpFilter(const TextFilter& filter, const std::string& property);

  // Declares and initializes a variable containing a RegExp object that
  // correspond to |filter| and returns the variable name.
  std::string AddRegexpInstance(const TextFilter& filter);

  // Returns the name of a new unique variable.
  std::string DeclareVariable();

  // Adds an argument to the argument list and returns its JavaScript
  // representation.
  //
  // This allows passing strings to the JavaScript code without having to
  // hardcode and escape them - this helps avoid XSS issues.
  std::string AddArgument(const std::string& value);

  // Adds a line of JavaScript code to the function, between the header and
  // footer. At that point, the variable "elements" contains the current set
  // of matches, as an array of nodes. It should be updated to contain the new
  // set of matches.
  //
  // IMPORTANT: Only pass strings that originate from hardcoded strings to
  // this method.
  void AddLine(const std::string& line) { snippet_.AddLine(line); }

  // Adds a line of JavaScript code to the function that's made up of multiple
  // parts to be concatenated together.
  //
  // IMPORTANT: Only pass strings that originate from hardcoded strings to
  // this method.
  void AddLine(const std::vector<std::string>& line) { snippet_.AddLine(line); }

  // Define a |queryAllDeduplicated(roots, selector)| JS function that calls
  // querySelectorAll(selector) on all |roots| (in order) and returns a
  // deduplicated list of the matching elements.
  // Calling this function a second time does not do anything; the function
  // will be defined only once.
  void DefineQueryAllDeduplicated();
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_JS_FILTER_BUILDER_H_
