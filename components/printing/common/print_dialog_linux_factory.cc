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
PrintDialogLinuxFactory::CreatePrintDialog(PrintingContextLinux* context) {
#if BUILDFLAG(USE_DBUS)
  if (base::FeatureList::IsEnabled(features::kLinuxXdgPrintPortal)) {
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

}  // namespace printing
