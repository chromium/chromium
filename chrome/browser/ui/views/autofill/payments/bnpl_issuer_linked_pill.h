// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_BNPL_ISSUER_LINKED_PILL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_BNPL_ISSUER_LINKED_PILL_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"

namespace autofill::payments {

class BnplLinkedIssuerPill : public views::Label {
  METADATA_HEADER(BnplLinkedIssuerPill, views::Label)

 public:
  BnplLinkedIssuerPill();
  BnplLinkedIssuerPill(const BnplLinkedIssuerPill&) = delete;
  BnplLinkedIssuerPill& operator=(const BnplLinkedIssuerPill&) = delete;
  ~BnplLinkedIssuerPill() override;
};

}  // namespace autofill::payments

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_BNPL_ISSUER_LINKED_PILL_H_
