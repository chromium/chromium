// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/logging/text_log_receiver.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"

namespace autofill {

namespace {

std::vector<std::string> RenderEntries(const base::Value::List& entries);

// Renders an HTML element to text at a best effort basis.
// This is a bit like calling Element.textContent on a DOM node and not super
// fancy but sufficient for debugging.
std::vector<std::string> RenderElement(const base::Value::Dict& entry) {
  const std::string* type = entry.FindString("type");
  DCHECK(type && *type == "element");

  const std::string* value = entry.FindString("value");
  DCHECK(value);

  std::vector<std::string> result;
  if (const base::Value::List* children = entry.FindList("children"))
    result = RenderEntries(*children);

  // Elements that should cause line wrapping.
  if (*value == "br" || *value == "div" || *value == "tr")
    result.push_back("\n");

  // Elements that should cause horizontal separation (may lead to pending
  // whitespace, which should be ok given that this is just for debugging.)
  if (*value == "td")
    result.push_back(" ");

  return result;
}

// Returns a text node to a vector with a single element representing the text.
std::vector<std::string> RenderText(const base::Value::Dict& entry) {
  const std::string* type = entry.FindString("type");
  DCHECK(type && *type == "text");

  const std::string* value = entry.FindString("value");
  DCHECK(value);

  return {*value};
}

// Concatenates the rendered contents of a document fragment into a vector of
// strings.
std::vector<std::string> RenderFragment(const base::Value::Dict& entry) {
  const std::string* type = entry.FindString("type");
  DCHECK(type && *type == "fragment");

  DCHECK(!entry.FindString("value"));

  const base::Value::List* children = entry.FindList("children");
  DCHECK(children);

  return RenderEntries(*children);
}

// Entry point into the rendering, you can pass any json object generated
// by the log buffer and it will dispatch the rendering to the correct
// functions.
// The output is a vector of strings that can be concatenated.
std::vector<std::string> RenderEntry(const base::Value::Dict& entry) {
  const std::string* type = entry.FindString("type");
  if (!type) {
    NOTREACHED_IN_MIGRATION();
  } else if (*type == "element") {
    return RenderElement(entry);
  } else if (*type == "text") {
    return RenderText(entry);
  } else if (*type == "fragment") {
    return RenderFragment(entry);
  } else {
    NOTREACHED_IN_MIGRATION();
  }
  return {};
}

// Concatenates the rendered contents of a list of log entries.
std::vector<std::string> RenderEntries(const base::Value::List& entries) {
  std::vector<std::string> result;
  for (const base::Value& entry : entries) {
    DCHECK(entry.is_dict());
    std::vector<std::string> rendered_entry = RenderEntry(entry.GetDict());
    base::ranges::move(rendered_entry, std::back_inserter(result));
  }
  return result;
}

}  // namespace

std::string TextLogReceiver::LogEntryToText(
    const base::Value::Dict& entry) const {
  return base::StrCat(RenderEntry(entry));
}

void TextLogReceiver::LogEntry(const base::Value::Dict& entry) {
  LOG(ERROR) << LogEntryToText(entry);
}

}  // namespace autofill
