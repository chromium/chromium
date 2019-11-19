// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_LEGAL_MESSAGE_LINE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_LEGAL_MESSAGE_LINE_H_

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"

namespace autofill {

using Link = LegalMessageLine::Link;

// A legal message line that allows for modifications.
class TestLegalMessageLine : public LegalMessageLine {
 public:
  TestLegalMessageLine() {}

  TestLegalMessageLine(const std::string& ascii_text) { set_text(ascii_text); }

  TestLegalMessageLine(const std::string& ascii_text,
                       const std::vector<Link>& links) {
    set_text(ascii_text);
    set_links(links);
  }

  ~TestLegalMessageLine() override {}

  void set_text(const std::string& ascii_text) {
    text_ = base::ASCIIToUTF16(ascii_text);
  }

  void set_links(const std::vector<Link>& links) { links_ = links; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLegalMessageLine);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_LEGAL_MESSAGE_LINE_H_
