// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_LEGAL_MESSAGE_LINE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_LEGAL_MESSAGE_LINE_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace autofill {

class LegalMessageLine;

using LegalMessageLines = std::vector<LegalMessageLine>;

class LegalMessageLine {
 public:
  struct Link {
    Link(size_t start, size_t end, const std::string& url_spec);
    ~Link();

    gfx::Range range;
    GURL url;
  };

  LegalMessageLine();
  LegalMessageLine(const LegalMessageLine& other);
  virtual ~LegalMessageLine();  // Overridden in TestLegalMessageLine.

  // Parses |legal_message|. Returns false on failure.
  //
  // Example of valid |legal_message| data:
  // {
  //   "line" : [ {
  //     "template" : "The legal documents are: {0} and {1}",
  //     "template_parameter" : [ {
  //       "display_text" : "Terms of Service",
  //       "url": "http://www.example.com/tos"
  //     }, {
  //       "display_text" : "Privacy Policy",
  //       "url": "http://www.example.com/pp"
  //     } ],
  //   }, {
  //     "template" : "This is the second line and it has no parameters"
  //   } ]
  // }
  //
  // Caveats:
  // 1. '{' and '}' may be displayed by escaping them with an apostrophe in the
  //    template string, e.g. template "Here is a literal '{'".
  // 2. Two or more consecutive dollar signs in the template string will not
  //    expand correctly.
  // 3. "${" anywhere in the template string is invalid.
  // 4. "\n" embedded anywhere in the template string, or an empty template
  //    string, can be used to separate paragraphs.
  // 5. Because a single apostrophe before a curly brace starts quoted literal
  //    text in MessageFormat, "'{0}" gets treated as a literal.  To avoid
  //    situations like these, setting |escape_apostrophes| to true will escape
  //    all ASCII apostrophes by doubling them up.
  //
  // |legal_message| must be a base::Value of type DICTIONARY.
  static bool Parse(const base::Value& legal_message,
                    LegalMessageLines* out,
                    bool escape_apostrophes = false);

  const base::string16& text() const { return text_; }
  const std::vector<Link>& links() const { return links_; }

 private:
  friend class TestLegalMessageLine;

  bool ParseLine(const base::Value& line, bool escape_apostrophes);

  base::string16 text_;
  std::vector<Link> links_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_LEGAL_MESSAGE_LINE_H_
