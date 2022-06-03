// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_CHROME_PDF_PRINT_CLIENT_H_
#define CHROME_RENDERER_PEPPER_CHROME_PDF_PRINT_CLIENT_H_

#include "components/pdf/renderer/pepper_pdf_host.h"

class ChromePDFPrintClient : public pdf::PepperPDFHost::PrintClient {
 public:
  ChromePDFPrintClient();

  ChromePDFPrintClient(const ChromePDFPrintClient&) = delete;
  ChromePDFPrintClient& operator=(const ChromePDFPrintClient&) = delete;

  ~ChromePDFPrintClient() override;

 private:
  // pdf::PepperPDFHost::PrintClient:
  bool IsPrintingEnabled(PP_Instance instance_id) override;
  bool Print(PP_Instance instance_id) override;
};

#endif  // CHROME_RENDERER_PEPPER_CHROME_PDF_PRINT_CLIENT_H_
