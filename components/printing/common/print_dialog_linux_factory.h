// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_COMMON_PRINT_DIALOG_LINUX_FACTORY_H_
#define COMPONENTS_PRINTING_COMMON_PRINT_DIALOG_LINUX_FACTORY_H_

#include <memory>

#include "printing/buildflags/buildflags.h"
#include "printing/printing_context_linux.h"

namespace printing {

class PrintDialogLinuxInterface;

class PrintDialogLinuxFactory
    : public PrintingContextLinux::PrintDialogFactory {
 public:
  PrintDialogLinuxFactory();
  PrintDialogLinuxFactory(const PrintDialogLinuxFactory&) = delete;
  PrintDialogLinuxFactory& operator=(const PrintDialogLinuxFactory&) = delete;
  ~PrintDialogLinuxFactory() override;

  // PrintingContextLinux::PrintDialogFactory:
  std::unique_ptr<PrintDialogLinuxInterface> CreatePrintDialog(
      PrintingContextLinux* context,
      bool show_system_dialog) override;
#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
  std::unique_ptr<PrintDialogLinuxInterface> CreatePrintDialogForSettings(
      PrintingContextLinux* context,
      const PrintSettings& settings) override;
#endif
};

}  // namespace printing

#endif  // COMPONENTS_PRINTING_COMMON_PRINT_DIALOG_LINUX_FACTORY_H_
