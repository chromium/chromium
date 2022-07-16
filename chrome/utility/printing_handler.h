// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_PRINTING_HANDLER_H_
#define CHROME_UTILITY_PRINTING_HANDLER_H_

#include <string>

#include "build/build_config.h"
#include "chrome/common/cloud_print_utility.mojom.h"
#include "printing/buildflags/buildflags.h"

#if !defined(OS_WIN) || !BUILDFLAG(ENABLE_PRINT_PREVIEW)
#error "Windows printing and print preview must be enabled"
#endif

namespace printing {

// Dispatches IPCs for printing.
class PrintingHandler : public chrome::mojom::CloudPrintUtility {
 public:
  PrintingHandler();
  PrintingHandler(const PrintingHandler&) = delete;
  PrintingHandler& operator=(const PrintingHandler&) = delete;
  ~PrintingHandler() override;

 private:
  // chrome::mojom::CloudPrintUtility:
  void GetPrinterCapsAndDefaults(
      const std::string& printer_name,
      GetPrinterCapsAndDefaultsCallback callback) override;
  void GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      GetPrinterSemanticCapsAndDefaultsCallback callback) override;
};

}  // namespace printing

#endif  // CHROME_UTILITY_PRINTING_HANDLER_H_
