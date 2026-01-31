// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/common/print_dialog_linux_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "build/config/linux/dbus/buildflags.h"
#include "printing/print_dialog_linux_interface.h"
#include "printing/printing_context_linux.h"
#include "printing/printing_features.h"
#include "ui/linux/linux_ui.h"

#if BUILDFLAG(USE_DBUS)
#include "components/printing/common/print_dialog_linux_portal.h"
#endif

namespace printing {

PrintDialogLinuxFactory::PrintDialogLinuxFactory() {
  PrintingContextLinux::SetPrintDialogFactory(this);
}

PrintDialogLinuxFactory::~PrintDialogLinuxFactory() {
  PrintingContextLinux::SetPrintDialogFactory(nullptr);
}

std::unique_ptr<PrintDialogLinuxInterface>
PrintDialogLinuxFactory::CreatePrintDialog(PrintingContextLinux* context,
                                           bool show_system_dialog) {
#if BUILDFLAG(USE_DBUS)
  if (show_system_dialog &&
      base::FeatureList::IsEnabled(features::kLinuxXdgPrintPortal)) {
    // A fallback dialog will not be created because ShowDialog is never
    // called in the OOP print service.
    return std::make_unique<PrintDialogLinuxPortal>(context);
  }
#endif
  if (auto* linux_ui = ui::LinuxUi::instance()) {
    return linux_ui->CreatePrintDialog(context);
  }
  return nullptr;
}

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
std::unique_ptr<PrintDialogLinuxInterface>
PrintDialogLinuxFactory::CreatePrintDialogForSettings(
    PrintingContextLinux* context,
    const PrintSettings& settings) {
#if BUILDFLAG(USE_DBUS)
  if (settings.system_print_dialog_data().FindString(
          kLinuxSystemPrintDialogDataPrintToken)) {
    return std::make_unique<PrintDialogLinuxPortal>(context);
  }
#endif
  if (auto* linux_ui = ui::LinuxUi::instance()) {
    return linux_ui->CreatePrintDialog(context);
  }
  return nullptr;
}
#endif

}  // namespace printing
